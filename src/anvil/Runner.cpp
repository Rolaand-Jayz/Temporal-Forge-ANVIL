// Runner.cpp — ANVIL deterministic pipeline implementation.
#include "Runner.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>
#include <set>

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
#include <libavutil/pixdesc.h>
#include <libswscale/swscale.h>
}

#include "../media/Demuxer.hpp"
#include "../media/VideoDecoder.hpp"

#include "CodecProbe.hpp"
#include "Core.hpp"
#include "GroundTruth.hpp"
#include "Oracle.hpp"
#include "Pnm.hpp"
#include "Reconstruct.hpp"
#include "Sha256.hpp"

namespace fs = std::filesystem;

namespace anvil {

namespace {

using Clock = std::chrono::steady_clock;
uint64_t elapsedNs(Clock::time_point t0) {
    return static_cast<uint64_t>(
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - t0).count());
}

void addTiming(Manifest& m, StageId stage, uint64_t ns) {
    m.stageTimings.push_back({stage, ns});
}

bool stageDumpable(const std::string& dumpStages, StageId id) {
    if (dumpStages.empty()) return false;
    size_t pos = 0;
    const std::string name = stageName(id);
    while (pos <= dumpStages.size()) {
        const size_t comma = dumpStages.find(',', pos);
        const std::string tok = dumpStages.substr(
            pos, comma == std::string::npos ? std::string::npos : comma - pos);
        if (tok == name || tok == "all") return true;
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return false;
}

Observation observationFromDecoded(const temporal_forge::DecodedVideoFrame& d) {
    Observation o;
    o.width = d.width;
    o.height = d.height;
    o.avPixelFormat = d.avFormat;
    o.color.range = d.colorRange;
    o.color.matrix = d.colorSpace;
    o.color.transfer = d.colorTransfer;
    o.color.primaries = d.colorPrimaries;
    o.color.chromaLocation = d.chromaLocation;
    o.color.pixelFormat = d.avFormat;
    o.color.bitDepth = d.bitDepth;
    o.color.hasMasteringDisplay = d.hasMasteringDisplay;
    o.color.hasContentLightLevel = d.hasContentLightLevel;
    o.ptsUs = d.ptsUs;
    o.ptsTicks = d.ptsTicks;
    o.frameIndex = d.frameIndex;
    o.keyframe = d.keyframe;
    o.bFrame = d.bFrame;
    o.planeCount = d.planes;
    for (int p = 0; p < d.planes && p < 4; ++p) {
        o.plane[p] = d.plane[p];
        o.linesize[p] = d.linesize[p];
    }
    o.codecMotionVectors.reserve(d.motionVectors.size());
    for (const auto& mv : d.motionVectors) {
        Observation::RawMv r;
        r.dstX = mv.dstX;
        r.dstY = mv.dstY;
        r.mvX = mv.mvX;
        r.mvY = mv.mvY;
        r.w = mv.w;
        r.h = mv.h;
        r.source = mv.source;
        o.codecMotionVectors.push_back(r);
    }
    return o;
}

// Normalizes raw codec MVs of frame f into BlockMotion entries. Reference
// frame identity is NOT provable from the exported side data (direction only,
// and only for past refs of P/B pictures), so entries are marked ambiguous
// unless the reference index can be proven by other means. Ambiguous vectors
// are never applied by the accumulator.
std::vector<BlockMotion> normalizeCodecMv(const Observation& f) {
    std::vector<BlockMotion> out;
    out.reserve(f.codecMotionVectors.size());
    for (const Observation::RawMv& r : f.codecMotionVectors) {
        BlockMotion b;
        b.frameIndex = f.frameIndex;
        b.refFrameIndex = -1; // not provable from side data
        b.temporalDistance = 0;
        b.direction = r.source < 0 ? RefDirection::Past : RefDirection::Future;
        b.dstX = r.dstX;
        b.dstY = r.dstY;
        b.blockW = r.w;
        b.blockH = r.h;
        b.mvX = r.mvX;
        b.mvY = r.mvY;
        // Precision/scale: AVMotionVector carries motion_scale; the reused
        // decoder normalized to source pixels, so declared precision is
        // subpixel. Partition geometry (w/h) is preserved as delivered.
        b.precision = MotionPrecision::SubQuarter;
        b.ambiguous = true;
        b.source = CorrespondenceSource::CodecMv;
        out.push_back(b);
    }
    return out;
}

// Map AVColorSpace to swscale coefficient tables for explicit conversion.
int swsCoefficientsFor(int matrix) {
    switch (matrix) {
        case AVCOL_SPC_BT709: return SWS_CS_ITU709;
        case AVCOL_SPC_BT470BG:
        case AVCOL_SPC_SMPTE170M: return SWS_CS_ITU601;
        case AVCOL_SPC_BT2020_NCL:
        case AVCOL_SPC_BT2020_CL: return SWS_CS_BT2020;
        case AVCOL_SPC_FCC: return SWS_CS_FCC;
        case AVCOL_SPC_SMPTE240M: return SWS_CS_SMPTE240M;
        default: return SWS_CS_DEFAULT;
    }
}

// Explicit YUV->RGB24 conversion honoring recorded matrix + range. Returns
// false when metadata is insufficient (caller records unknown-metadata state).
// Chroma siting: swscale applies a fixed siting convention; the recorded
// chroma_location is carried in the manifest so the assumption is auditable.
bool convertToRgb(const Observation& o, std::vector<uint8_t>& rgb, int& stride) {
    if (!o.color.conversionFullySpecified()) return false;
    SwsContext* sws = sws_getContext(o.width, o.height,
                                     static_cast<AVPixelFormat>(o.avPixelFormat),
                                     o.width, o.height, AV_PIX_FMT_RGB24,
                                     SWS_BILINEAR | SWS_FULL_CHR_H_INT | SWS_ACCURATE_RND,
                                     nullptr, nullptr, nullptr);
    if (!sws) return false;
    const int* table = sws_getCoefficients(swsCoefficientsFor(o.color.matrix));
    sws_setColorspaceDetails(sws, table, o.color.range == AVCOL_RANGE_JPEG ? 1 : 0,
                             table, 1, 0, 1 << 16, 1 << 16);
    stride = o.width * 3;
    rgb.assign(static_cast<size_t>(stride) * o.height, 0);
    const uint8_t* src[4] = {o.plane[0].data(), o.plane[1].data(), o.plane[2].data(), nullptr};
    const int srcStride[4] = {o.linesize[0], o.linesize[1], o.linesize[2], 0};
    uint8_t* dst[4] = {rgb.data(), nullptr, nullptr, nullptr};
    const int dstStride[4] = {stride, 0, 0, 0};
    const int h = sws_scale(sws, src, srcStride, 0, o.height, dst, dstStride);
    sws_freeContext(sws);
    return h == o.height;
}

// Deterministic mean absolute luma difference in 8-bit scale, bit-depth
// normalized. Used for automatic scene-cut detection.
double meanLumaDiff8(const Observation& a, const Observation& b) {
    if (a.width != b.width || a.height != b.height || a.plane[0].empty()
        || b.plane[0].empty())
        return 0.0;
    const int bpsA = a.color.bitDepth > 8 ? 2 : 1;
    const int bpsB = b.color.bitDepth > 8 ? 2 : 1;
    const uint32_t maxA = a.color.bitDepth >= 16
        ? 65535u : ((1u << std::max(1, a.color.bitDepth)) - 1u);
    const uint32_t maxB = b.color.bitDepth >= 16
        ? 65535u : ((1u << std::max(1, b.color.bitDepth)) - 1u);
    uint64_t sad = 0;
    for (int y = 0; y < a.height; ++y) {
        for (int x = 0; x < a.width; ++x) {
            const size_t oa = size_t(y) * a.linesize[0] + size_t(x) * bpsA;
            const size_t ob = size_t(y) * b.linesize[0] + size_t(x) * bpsB;
            const uint32_t va = bpsA == 1 ? a.plane[0][oa]
                : uint32_t(a.plane[0][oa]) | (uint32_t(a.plane[0][oa + 1]) << 8);
            const uint32_t vb = bpsB == 1 ? b.plane[0][ob]
                : uint32_t(b.plane[0][ob]) | (uint32_t(b.plane[0][ob + 1]) << 8);
            sad += static_cast<uint64_t>(std::llround(
                std::abs(double(va) / std::max(1u, maxA)
                       - double(vb) / std::max(1u, maxB)) * 255.0));
        }
    }
    return double(sad) / double(a.width * a.height);
}

Manifest::FrameRecord::ColorFields colorFieldsOf(const ColorMeta& c) {
    Manifest::FrameRecord::ColorFields f;
    f.range = ColorMeta::rangeName(c.range);
    f.primaries = ColorMeta::primariesName(c.primaries);
    f.transfer = ColorMeta::transferName(c.transfer);
    f.matrix = ColorMeta::matrixName(c.matrix);
    f.chromaLocation = ColorMeta::chromaLocationName(c.chromaLocation);
    f.pixelFormat = av_get_pix_fmt_name(static_cast<AVPixelFormat>(c.pixelFormat))
        ? av_get_pix_fmt_name(static_cast<AVPixelFormat>(c.pixelFormat))
        : std::string("invalid");
    f.bitDepth = c.bitDepth;
    f.hasMasteringDisplay = c.hasMasteringDisplay;
    f.hasContentLightLevel = c.hasContentLightLevel;
    return f;
}

bool writeTextFile(const fs::path& p, const std::string& content) {
    std::error_code ec;
    fs::create_directories(p.parent_path(), ec);
    std::ofstream f(p, std::ios::binary);
    if (!f) return false;
    f << content;
    return true;
}

} // namespace

RunResult runPipeline(const RunConfig& config) {
    RunResult result;
    Manifest& m = result.manifest;

    if (config.past < 0 || config.future < 0 || config.frameCount < 1) {
        result.error = "invalid window/frame-count parameters";
        return result;
    }

    // --- config + provenance ---
    m.config.inputPath = config.inputPath;
    m.config.startFrame = config.startFrame;
    m.config.frameCount = config.frameCount;
    m.config.past = config.past;
    m.config.future = config.future;
    m.config.correspondenceMode = config.correspondenceMode;
    m.config.visibilityMode = config.visibilityMode;
    m.config.geometryMode = config.geometryMode;
    m.config.accumulateEnabled = config.accumulateEnabled;
    m.config.colorConvertEnabled = config.colorConvertEnabled;
    m.config.forcedCutFrames = config.forcedCutFrames;
    m.config.autoSceneCut = config.autoSceneCut;
    m.config.autoCutThreshold = config.autoCutThreshold;
    m.config.oracleDir = config.oracleDir;
    m.config.dumpDir = config.dumpDir;
    m.config.dumpStages = config.dumpStages;
    m.config.seed = config.seed;
    m.config.outputFormat = "ppm_or_depth_preserving_pgm_planes";
    m.provenance.gitSha = detectGitSha();
    m.provenance.gitDirty = detectGitDirty();
    m.provenance.ffmpegVersion = av_version_info();
    m.provenance.buildType = ANVIL_BUILD_TYPE;
    m.provenance.compilerId = ANVIL_COMPILER_ID;
    m.provenance.inputHashOk = sha256FileHex(config.inputPath, m.provenance.inputSha256);
    std::error_code ecSize;
    m.provenance.inputSizeBytes = static_cast<uint64_t>(fs::file_size(config.inputPath, ecSize));
    if (ecSize) m.provenance.inputSizeBytes = 0;
    m.decodeMode = "software";

    const Clock::time_point probeStart = Clock::now();
    m.codecCapabilities = probeCodecCapabilities();
    (void)probeStart; // capability probe is not a per-stage pipeline timing

    // --- open input ---
    temporal_forge::Demuxer demuxer;
    if (!demuxer.open(config.inputPath) || demuxer.info().videoIndex < 0) {
        result.error = "failed to open input video stream";
        return result;
    }
    temporal_forge::VideoDecoder decoder;
    decoder.setMotionMetadataRequested(true); // forces software decode + MV export
    if (!decoder.open(demuxer.ctx(), demuxer.info().videoIndex)) {
        result.error = "failed to open video decoder";
        return result;
    }

    // --- decode pass (software, deterministic) ---
    const Clock::time_point decodeStart = Clock::now();
    const int64_t lastNeeded = config.startFrame + config.frameCount - 1 + config.future;
    std::map<uint64_t, Observation> frames;
    bool decodeFailed = false;
    std::string decodeError;
    while (static_cast<int64_t>(frames.size()) == 0
           || frames.rbegin()->first < static_cast<uint64_t>(lastNeeded)) {
        temporal_forge::Packet pkt;
        if (!demuxer.readPacket(pkt)) {
            decoder.sendPacket(nullptr); // EOF: flush delayed decoder frames
            break;
        }
        if (pkt.isEof) {
            decoder.sendPacket(nullptr);
            break;
        }
        if (pkt.isFlush) decoder.flush();
        if (pkt.av && pkt.streamIndex == demuxer.info().videoIndex) {
            decoder.sendPacket(pkt.av);
        }
        temporal_forge::DecodedVideoFrame d;
        while (decoder.receiveFrame(d)) {
            Observation o = observationFromDecoded(d);
            const uint64_t idx = o.frameIndex;
            frames.emplace(idx, std::move(o));
            d = temporal_forge::DecodedVideoFrame{};
        }
        if (decoder.drainComplete()) break;
    }
    // final drain in case the decoder still holds delayed frames
    while (!decoder.drainComplete()) {
        temporal_forge::DecodedVideoFrame d;
        if (!decoder.receiveFrame(d)) break;
        Observation o = observationFromDecoded(d);
        frames.emplace(o.frameIndex, std::move(o));
    }
    if (frames.empty()) {
        result.error = "no frames decoded from input";
        return result;
    }
    addTiming(m, StageId::Decode, elapsedNs(decodeStart));

    const int width = frames.begin()->second.width;
    const int height = frames.begin()->second.height;

    // F3: capability is judged from the ACTUAL input codec, never from the
    // global probe matrix (H.264 support must not imply HEVC/AV1 support).
    const char* inputCodecName = decoder.codecName();
    const std::string inputCodec = inputCodecName ? inputCodecName : "unknown";
    const Manifest::CodecCapability* inputCap = nullptr;
    for (const auto& c : m.codecCapabilities)
        if (c.codec == inputCodec) inputCap = &c;

    // F1: canonicalize automatic cut boundaries BEFORE any window is built.
    // Detection compares every consecutive decoded frame pair, so cuts
    // between arbitrary neighbor pairs are found regardless of which frame
    // is the reconstruction target. Forced cuts and detected cuts share one
    // exclusion rule downstream.
    std::set<int64_t> autoCuts;
    if (config.autoSceneCut) {
        uint64_t prevIndex = 0;
        bool havePrev = false;
        for (const auto& [idx, obs] : frames) {
            if (havePrev) {
                const double diff = meanLumaDiff8(frames.at(prevIndex), obs);
                if (diff > config.autoCutThreshold) {
                    autoCuts.insert(static_cast<int64_t>(idx));
                    m.events.push_back({idx, "scene_cut",
                                        "auto: mean luma diff "
                                        + std::to_string(diff)});
                }
            }
            prevIndex = idx;
            havePrev = true;
        }
    }

    // --- output dirs ---
    std::error_code fsEc;
    fs::create_directories(config.outputDir, fsEc);
    fs::path dumpBase;
    if (!config.dumpDir.empty()) {
        dumpBase = config.dumpDir;
        fs::create_directories(dumpBase, fsEc);
    }
    auto dumpPath = [&](StageId stage, uint64_t frame, const char* tag) {
        return dumpBase / (std::string(stageName(stage)) + "_f" + std::to_string(frame)
                           + "_" + tag);
    };
    auto recordDump = [&](const fs::path& p) {
        // Inventory paths are recorded relative to their base directory so
        // the manifest is deterministic across output locations.
        std::error_code relEc;
        const fs::path rel = fs::relative(p, config.dumpDir, relEc);
        const std::string stored = relEc ? p.string() : rel.string();
        m.dumpFiles.push_back(stored);
        result.outputFiles.push_back(p.string());
    };

    // --- ground-truth mapping validation (reference evidence only) ---
    {
        const int64_t firstTarget = config.startFrame;
        const int64_t lastTarget = config.startFrame + config.frameCount - 1;
        for (const auto& [gtFrame, gtPath] : config.groundTruth) {
            if (gtFrame < firstTarget || gtFrame > lastTarget) {
                result.error = std::string("ground truth ")
                    + groundTruthErrorName(GroundTruthError::FrameOutOfRange)
                    + ": frame " + std::to_string(gtFrame)
                    + " is not a target of this run [" + std::to_string(firstTarget)
                    + ", " + std::to_string(lastTarget) + "]";
                return result;
            }
        }
    }

    // --- per-target pipeline ---
    for (int64_t ti = 0; ti < config.frameCount; ++ti) {
        const uint64_t t = static_cast<uint64_t>(config.startFrame + ti);
        Manifest::FrameRecord rec;
        rec.frameIndex = t;
        auto it = frames.find(t);
        if (it == frames.end()) {
            decodeFailed = true;
            decodeError = "frame " + std::to_string(t) + " missing from decode output";
            m.events.push_back({t, "decode_gap", decodeError});
            break;
        }
        const Observation& target = it->second;
        rec.ptsUs = target.ptsUs;

        // Side-info state derived from the INPUT codec's measured
        // capability and the frame's actual evidence. Exported data with
        // unproven reference identity is reported ambiguous (exported but
        // unusable) — never "available", which would contradict the
        // reconstruction behavior that rejects ambiguous vectors.
        const auto normalizedMv = normalizeCodecMv(target);
        size_t usableMv = 0;
        for (const BlockMotion& b : normalizedMv)
            if (!b.ambiguous && b.refFrameIndex >= 0) ++usableMv;
        rec.codecMvCount = target.codecMotionVectors.size();
        rec.codecMvUsableCount = usableMv;
        if (!target.codecMotionVectors.empty()) {
            rec.sideInfoState = sideInfoStateName(
                usableMv > 0 ? SideInfoState::Available
                             : SideInfoState::Ambiguous);
            if (usableMv == 0)
                m.events.push_back({t, "side_info_ambiguous",
                                    "codec MV side data exported but reference "
                                    "identity unproven; not applied to "
                                    "reconstruction"});
        } else if (inputCap && inputCap->mvExportProven) {
            // The input codec CAN export MVs (measured), this frame simply
            // carries none (e.g. an I-frame): estimator fallback is allowed.
            rec.sideInfoState = sideInfoStateName(SideInfoState::EstimatorOnly);
            m.events.push_back({t, "side_info_absent",
                                "no codec MV side data on this frame; input "
                                "codec " + inputCodec + " export is proven"});
        } else {
            rec.sideInfoState = sideInfoStateName(SideInfoState::Unsupported);
            m.events.push_back({t, "side_info_unsupported",
                                "input codec " + inputCodec
                                + " cannot export MV side data (measured "
                                "capability)"});
        }

        // decode-stage capture: source planes (pre-pipeline) and raw codec
        // side information as delivered (never normalized, never invented).
        if (!dumpBase.empty() && stageDumpable(config.dumpStages, StageId::Decode)) {
            const fs::path yP = dumpPath(StageId::Decode, t, "y.pgm");
            if (writePgm(yP.string(), width, height, target.plane[0].data(),
                         target.linesize[0], target.color.bitDepth))
                recordDump(yP);
            std::string mvs = "frame " + std::to_string(t) + "\n";
            if (target.codecMotionVectors.empty()) {
                mvs += "state=none count=0\n";
            } else {
                mvs += "state=present count="
                    + std::to_string(target.codecMotionVectors.size()) + "\n";
                for (const Observation::RawMv& r : target.codecMotionVectors) {
                    mvs += "dst " + std::to_string(r.dstX) + " "
                         + std::to_string(r.dstY) + " size " + std::to_string(r.w)
                         + " " + std::to_string(r.h) + " mv " + std::to_string(r.mvX)
                         + " " + std::to_string(r.mvY) + " source "
                         + std::to_string(int(r.source)) + "\n";
                }
            }
            const fs::path mvP = dumpPath(StageId::Decode, t, "mvs.txt");
            if (writeTextFile(mvP, mvs)) recordDump(mvP);
        }

        // ground truth: validate against THIS target's decoded geometry,
        // record provenance, and do nothing else with it.
        if (auto gtit = config.groundTruth.find(static_cast<int64_t>(t));
            gtit != config.groundTruth.end()) {
            GroundTruthRecord g;
            const GroundTruthError gerr =
                validateGroundTruth(gtit->second, target.width, target.height, g);
            if (gerr != GroundTruthError::None) {
                result.error = std::string("ground truth for frame ")
                    + std::to_string(t) + ": " + groundTruthErrorName(gerr)
                    + " (" + gtit->second + ")";
                return result;
            }
            g.frameIndex = t;
            Manifest::GroundTruthEntry e;
            e.frameIndex = g.frameIndex;
            e.path = g.path;
            e.sha256 = g.sha256;
            e.sizeBytes = g.sizeBytes;
            e.width = g.width;
            e.height = g.height;
            e.maxval = g.maxval;
            e.bytesPerSample = g.bytesPerSample;
            e.format = g.format;
            e.observationWidth = g.observationWidth;
            e.observationHeight = g.observationHeight;
            e.scaleX = g.scaleX;
            e.scaleY = g.scaleY;
            e.resolutionRelation = g.resolutionRelation;
            e.usageNote = GroundTruthRecord::kUsageNote;
            m.groundTruth.push_back(e);
            rec.hasGroundTruth = true;
        }

        // window selection (cut-aware)
        const Clock::time_point winStart = Clock::now();
        std::vector<uint64_t> window = WindowConfig{config.past, config.future}.windowFor(t);
        // One exclusion rule for forced AND automatically detected
        // boundaries; a neighbor is excluded when any cut lies strictly
        // between it and the target, in both temporal directions.
        auto crossesCut = [&](uint64_t a, uint64_t b) {
            const uint64_t lo = std::min(a, b), hi = std::max(a, b);
            for (int64_t c : config.forcedCutFrames)
                if (c > 0 && static_cast<uint64_t>(c) > lo && static_cast<uint64_t>(c) <= hi)
                    return true;
            for (int64_t c : autoCuts)
                if (static_cast<uint64_t>(c) > lo && static_cast<uint64_t>(c) <= hi)
                    return true;
            return false;
        };
        std::vector<uint64_t> neighbors;
        std::vector<std::string> excluded;
        for (uint64_t s : window) {
            if (s == t) continue;
            if (!frames.count(s)) {
                excluded.push_back(std::to_string(s) + " reason=not_decoded");
                continue;
            }
            if (crossesCut(t, s)) {
                excluded.push_back(std::to_string(s) + " reason=cut_boundary");
                m.events.push_back({s, "window_reset",
                                    "excluded across scene-cut boundary"});
                continue;
            }
            neighbors.push_back(s);
        }
        if (!dumpBase.empty() && stageDumpable(config.dumpStages, StageId::WindowSelect)) {
            std::string content = "target " + std::to_string(t)
                + " past=" + std::to_string(config.past)
                + " future=" + std::to_string(config.future) + "\n";
            for (uint64_t s : neighbors)
                content += "neighbor " + std::to_string(s) + "\n";
            for (const std::string& e : excluded) content += "excluded " + e + "\n";
            const fs::path wP = dumpPath(StageId::WindowSelect, t, "window.txt");
            if (writeTextFile(wP, content)) recordDump(wP);
        }
        addTiming(m, StageId::WindowSelect, elapsedNs(winStart));

        // correspondence per neighbor (flow + block normalization only;
        // visibility is timed separately below)
        const Clock::time_point corrStart = Clock::now();
        std::vector<FlowField> flows;
        std::vector<std::vector<uint8_t>> coverages;
        std::vector<std::vector<BlockMotion>> neighborBlocks;
        std::vector<uint64_t> neighborOrder;
        std::string corrSourceUsed = config.correspondenceMode;
        for (uint64_t s : neighbors) {
            const Observation& obs = frames[s];
            std::vector<BlockMotion> blocks;
            if (config.correspondenceMode == "oracle") {
                if (!oracleFileExists(config.oracleDir, t)) {
                    result.error = "oracle correspondence missing for frame "
                                 + std::to_string(t);
                    return result;
                }
                auto ob = loadOracleCorrespondence(config.oracleDir, t);
                if (!ob) {
                    result.error = "oracle correspondence parse failure for frame "
                                 + std::to_string(t);
                    return result;
                }
                if (s == t) continue; // unreachable: neighbors exclude target
                // Oracle lines may address all window frames at once; keep
                // only proven references to this neighbor.
                for (const BlockMotion& b : *ob)
                    if (b.refFrameIndex == static_cast<int64_t>(s)) blocks.push_back(b);
                m.events.push_back({t, "oracle_used", "correspondence"});
            } else if (config.correspondenceMode == "estimate") {
                blocks = estimateCorrespondence(target, obs);
            } else if (config.correspondenceMode == "codec") {
                blocks = normalizeCodecMv(target);
                size_t ambiguous = 0;
                for (const BlockMotion& b : blocks)
                    if (b.ambiguous || b.refFrameIndex != static_cast<int64_t>(s)) ++ambiguous;
                if (ambiguous == blocks.size() && !blocks.empty())
                    m.events.push_back({t, "side_info_ambiguous",
                                        "codec MV reference identity unproven; "
                                        "not applied to reconstruction"});
                // only proven entries may enter the flow field
                std::vector<BlockMotion> proven;
                for (const BlockMotion& b : blocks)
                    if (!b.ambiguous && b.refFrameIndex == static_cast<int64_t>(s))
                        proven.push_back(b);
                blocks = std::move(proven);
            } else if (config.correspondenceMode == "none") {
                blocks.clear();
                corrSourceUsed = "none";
            }
            std::vector<uint8_t> coverage;
            FlowField flow = buildFlowField(width, height, blocks,
                                            static_cast<int64_t>(s), &coverage);
            flows.push_back(std::move(flow));
            coverages.push_back(std::move(coverage));
            neighborBlocks.push_back(std::move(blocks));
            neighborOrder.push_back(s);
        }
        rec.correspondenceSource = corrSourceUsed;
        addTiming(m, StageId::Correspondence, elapsedNs(corrStart));

        // visibility per neighbor, timed independently: oracle replaces;
        // otherwise default valid, minus pixels WITHOUT proven correspondence
        // coverage. Coverage distinguishes genuinely measured zero motion
        // (covered, valid) from unavailable correspondence (uncovered,
        // invalid) — placeholder zero-flow never enters accumulation.
        const Clock::time_point visStart = Clock::now();
        std::vector<std::vector<Visibility>> visMasks;
        for (size_t i = 0; i < neighbors.size(); ++i) {
            std::vector<Visibility> vis(static_cast<size_t>(width) * height,
                                        Visibility::Valid);
            if (config.visibilityMode == "oracle") {
                auto ov = loadOracleVisibility(config.oracleDir, t, width, height);
                if (!ov) {
                    result.error = "oracle visibility missing/invalid for frame "
                                 + std::to_string(t);
                    return result;
                }
                vis = std::move(*ov);
                m.events.push_back({t, "oracle_used", "visibility"});
            } else if (!coverages[i].empty()) {
                for (size_t j = 0; j < vis.size(); ++j)
                    if (!coverages[i][j]) vis[j] = Visibility::Invalid;
            }
            visMasks.push_back(std::move(vis));
        }
        addTiming(m, StageId::Visibility, elapsedNs(visStart));

        // sample geometry
        const Clock::time_point geoStart = Clock::now();
        SampleGeometry geo;
        if (config.geometryMode == "oracle") {
            auto og = loadOracleGeometry(config.oracleDir, t);
            if (!og) {
                result.error = "oracle geometry missing/invalid for frame "
                             + std::to_string(t);
                return result;
            }
            geo = *og;
            m.events.push_back({t, "oracle_used", "sample_geometry"});
        }
        rec.geometryState = sampleGeometryStateName(geo.state);
        if (!dumpBase.empty() && stageDumpable(config.dumpStages, StageId::SampleGeometryStage)) {
            const fs::path p = dumpPath(StageId::SampleGeometryStage, t, "geometry.txt");
            const std::string content = std::to_string(int(geo.state)) + " "
                + std::to_string(geo.phaseX) + " " + std::to_string(geo.phaseY) + "\n";
            if (writeTextFile(p, content)) recordDump(p);
        }
        addTiming(m, StageId::SampleGeometryStage, elapsedNs(geoStart));

        // color stage: explicit working-space decision
        const Clock::time_point colorStart = Clock::now();
        WorkingSpaceResult ws;
        if (!config.colorConvertEnabled) {
            ws.state = WorkingSpaceResult::State::identityKnown;
            ws.description = "color_convert disabled; source space preserved";
        } else if (target.color.conversionFullySpecified()) {
            if (target.color.isHdrTransfer()) {
                ws.state = WorkingSpaceResult::State::identityKnown;
                ws.description = "hdr transfer ("
                    + ColorMeta::transferName(target.color.transfer)
                    + "): no transfer conversion performed; planes preserved";
            } else {
                ws.state = WorkingSpaceResult::State::converted;
                ws.description = "yuv->rgb24 explicit matrix="
                    + ColorMeta::matrixName(target.color.matrix)
                    + " range=" + ColorMeta::rangeName(target.color.range);
            }
        } else {
            ws.state = WorkingSpaceResult::State::unknownMetadata;
            ws.description = "color metadata incomplete; no conversion performed";
            m.events.push_back({t, "color_unknown_metadata", ws.description});
        }
        rec.colorConversion = ws.description;
        if (!dumpBase.empty() && stageDumpable(config.dumpStages, StageId::ColorConvert)) {
            std::string content = "frame " + std::to_string(t) + "\n";
            content += "range " + ColorMeta::rangeName(target.color.range) + "\n";
            content += "primaries " + ColorMeta::primariesName(target.color.primaries) + "\n";
            content += "transfer " + ColorMeta::transferName(target.color.transfer) + "\n";
            content += "matrix " + ColorMeta::matrixName(target.color.matrix) + "\n";
            content += "chroma_location "
                + ColorMeta::chromaLocationName(target.color.chromaLocation) + "\n";
            content += "pixel_format "
                + std::string(av_get_pix_fmt_name(static_cast<AVPixelFormat>(target.avPixelFormat))
                    ? av_get_pix_fmt_name(static_cast<AVPixelFormat>(target.avPixelFormat)) : "invalid") + "\n";
            content += "bit_depth " + std::to_string(target.color.bitDepth) + "\n";
            content += "hdr_mastering_display "
                     + std::to_string(int(target.color.hasMasteringDisplay)) + "\n";
            content += "hdr_content_light_level "
                     + std::to_string(int(target.color.hasContentLightLevel)) + "\n";
            content += "working_space state="
                + std::to_string(static_cast<int>(ws.state))
                + " description=" + ws.description + "\n";
            const fs::path cP = dumpPath(StageId::ColorConvert, t, "color.txt");
            if (writeTextFile(cP, content)) recordDump(cP);
        }
        addTiming(m, StageId::ColorConvert, elapsedNs(colorStart));

        // accumulate
        const Clock::time_point accStart = Clock::now();
        AccumulateResult acc;
        acc.frame = target;
        if (config.accumulateEnabled) {
            std::vector<Observation> nb;
            std::vector<FlowField> nbFlows;
            std::vector<std::vector<Visibility>> nbVis;
            for (size_t i = 0; i < neighbors.size(); ++i) {
                nb.push_back(frames[neighbors[i]]);
                nbFlows.push_back(flows[i]);
                nbVis.push_back(visMasks[i]);
            }
            acc = accumulate(target, nb, nbFlows, nbVis);
        }
        rec.validSamples = acc.validSamples;
        rec.totalSamples = acc.totalSamples;
        if (!dumpBase.empty() && stageDumpable(config.dumpStages, StageId::Accumulate)) {
            const fs::path p = dumpPath(StageId::Accumulate, t, "y.pgm");
            if (writePgm(p.string(), width, height, acc.frame.plane[0].data(),
                         acc.frame.linesize[0], acc.frame.color.bitDepth))
                recordDump(p);
        }
        addTiming(m, StageId::Accumulate, elapsedNs(accStart));

        // output (non-FSR backend)
        const Clock::time_point outStart = Clock::now();
        fs::path outP;
        if (ws.state == WorkingSpaceResult::State::converted) {
            std::vector<uint8_t> rgb;
            int stride = 0;
            // The actual YUV->RGB transform belongs to the color stage.
            const Clock::time_point convStart = Clock::now();
            const bool converted = convertToRgb(acc.frame, rgb, stride);
            addTiming(m, StageId::ColorConvert, elapsedNs(convStart));
            if (converted) {
                outP = fs::path(config.outputDir) / ("frame_" + std::to_string(t) + ".ppm");
                if (writePpm(outP.string(), width, height, rgb.data(), stride)) {
                    result.outputFiles.push_back(outP.string());
                    m.outputFiles.push_back(
                        fs::relative(outP, config.outputDir, fsEc).string());
                }
            }
        }
        if (outP.empty()) {
            const AVPixFmtDescriptor* desc=av_pix_fmt_desc_get(
                static_cast<AVPixelFormat>(acc.frame.avPixelFormat));
            const char* suffix[4]={"y","u","v","p3"};
            for(int p=0;p<acc.frame.planeCount&&p<4;++p){
                if(acc.frame.plane[p].empty())continue;
                int pw=width,ph=height;
                if(desc&&(p==1||p==2)){
                    pw=AV_CEIL_RSHIFT(width,desc->log2_chroma_w);
                    ph=AV_CEIL_RSHIFT(height,desc->log2_chroma_h);
                }
                const fs::path q=fs::path(config.outputDir)/
                    ("frame_"+std::to_string(t)+"_"+suffix[p]+".pgm");
                if(!writePgm(q.string(),pw,ph,acc.frame.plane[p].data(),
                             acc.frame.linesize[p],acc.frame.color.bitDepth)){
                    result.error="failed to write depth-preserving output plane "
                        +std::to_string(p)+" for frame "+std::to_string(t);return result;
                }
                result.outputFiles.push_back(q.string());
                m.outputFiles.push_back(fs::relative(q,config.outputDir,fsEc).string());
                if(p==0)outP=q;
            }
            if(outP.empty()){result.error="no output plane available for frame "+std::to_string(t);return result;}
        }
        addTiming(m, StageId::Output, elapsedNs(outStart));
        // Structured color metadata: source space as decoded, output space
        // as actually written. RGB output carries full range, RGB matrix and
        // the source primaries/transfer (no transfer conversion is ever
        // performed); chroma siting is not applicable to packed RGB.
        rec.colorSource = colorFieldsOf(target.color);
        if (!outP.empty() && outP.string().ends_with(".ppm")) {
            rec.colorOutput.range = "full";
            rec.colorOutput.primaries = rec.colorSource.primaries;
            rec.colorOutput.transfer = rec.colorSource.transfer;
            rec.colorOutput.matrix = "rgb";
            rec.colorOutput.chromaLocation = "not_applicable";
            rec.colorOutput.pixelFormat = "rgb24";
            rec.colorOutput.bitDepth = 8;
        } else {
            rec.colorOutput = rec.colorSource;
        }
        m.frames.push_back(rec);

        if (!dumpBase.empty() && stageDumpable(config.dumpStages, StageId::Correspondence)) {
            // Actual correspondence values with provenance, per neighbor.
            std::string content = "frame " + std::to_string(t)
                                + " source=" + corrSourceUsed + "\n";
            for (size_t i = 0; i < neighborBlocks.size(); ++i) {
                content += "neighbor " + std::to_string(neighborOrder[i]) + " blocks "
                         + std::to_string(neighborBlocks[i].size()) + "\n";
                for (const BlockMotion& b : neighborBlocks[i]) {
                    content += "block " + std::to_string(b.dstX) + " "
                         + std::to_string(b.dstY) + " " + std::to_string(b.blockW)
                         + " " + std::to_string(b.blockH) + " mv "
                         + std::to_string(b.mvX) + " " + std::to_string(b.mvY)
                         + " ref " + std::to_string(b.refFrameIndex)
                         + " ambiguous " + std::to_string(int(b.ambiguous))
                         + " precision " + std::to_string(static_cast<int>(b.precision))
                         + " source " + std::to_string(static_cast<int>(b.source))
                         + "\n";
                }
            }
            const fs::path cP = dumpPath(StageId::Correspondence, t, "correspondence.txt");
            if (writeTextFile(cP, content)) recordDump(cP);
        }
        if (!dumpBase.empty() && stageDumpable(config.dumpStages, StageId::Visibility)) {
            if (visMasks.empty()) {
                // single-frame window: visibility is vacuous (no neighbor
                // samples); record the state explicitly rather than silently.
                const fs::path vP = dumpPath(StageId::Visibility, t, "none.txt");
                if (writeTextFile(vP, "frame " + std::to_string(t)
                                + " state=no_neighbors\n"))
                    recordDump(vP);
            } else {
                for (size_t i = 0; i < visMasks.size(); ++i) {
                    std::vector<uint8_t> bytes(visMasks[i].size());
                    for (size_t j = 0; j < bytes.size(); ++j)
                        bytes[j] = visMasks[i][j] == Visibility::Valid ? 255
                                 : visMasks[i][j] == Visibility::Unknown ? 128 : 0;
                    const fs::path vP = dumpPath(StageId::Visibility, t,
                                                 ("i" + std::to_string(i) + ".pgm").c_str());
                    if (writePgm(vP.string(), width, height, bytes.data(), width))
                        recordDump(vP);
                }
            }
        }
    }

    if (decodeFailed) result.error = decodeError;

    // --- manifest ---
    const fs::path manifestPath = fs::path(config.outputDir) / "manifest.json";
    if (writeTextFile(manifestPath, m.toJson() + "\n")) {
        result.outputFiles.push_back(manifestPath.string());
        m.outputFiles.push_back("manifest.json");
    }
    result.ok = result.error.empty();
    return result;
}

} // namespace anvil
