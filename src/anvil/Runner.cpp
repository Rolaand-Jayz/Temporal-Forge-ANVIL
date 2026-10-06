// Runner.cpp — ANVIL deterministic pipeline implementation.
#include "Runner.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <map>

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
    m.config.outputFormat = "ppm_or_pgm_planes";
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
        m.dumpFiles.push_back(p.string());
        result.outputFiles.push_back(p.string());
    };

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

        // side-info state from measurement
        if (!target.codecMotionVectors.empty()) {
            rec.sideInfoState = sideInfoStateName(SideInfoState::Available);
            rec.codecMvCount = target.codecMotionVectors.size();
        } else {
            const bool exportMechanismWorks = std::any_of(
                m.codecCapabilities.begin(), m.codecCapabilities.end(),
                [](const Manifest::CodecCapability& c) { return c.mvExportProven; });
            // Frame carries no codec MV side data. If the export mechanism
            // works for some codec on this host, the honest state is
            // estimator-only (e.g. an I-frame); otherwise the codec's export
            // is unsupported outright.
            rec.sideInfoState = sideInfoStateName(
                exportMechanismWorks ? SideInfoState::EstimatorOnly
                                     : SideInfoState::Unsupported);
            m.events.push_back({t, "side_info_unsupported",
                                "no codec MV side data on this frame"});
        }

        // window selection (cut-aware)
        const Clock::time_point winStart = Clock::now();
        std::vector<uint64_t> window = WindowConfig{config.past, config.future}.windowFor(t);
        auto crossesCut = [&](uint64_t a, uint64_t b) {
            const uint64_t lo = std::min(a, b), hi = std::max(a, b);
            for (int64_t c : config.forcedCutFrames)
                if (c > 0 && static_cast<uint64_t>(c) > lo && static_cast<uint64_t>(c) <= hi)
                    return true;
            return false;
        };
        std::vector<uint64_t> neighbors;
        for (uint64_t s : window) {
            if (s == t) continue;
            if (!frames.count(s)) continue;
            if (crossesCut(t, s)) {
                m.events.push_back({s, "window_reset",
                                    "excluded across forced cut boundary"});
                continue;
            }
            neighbors.push_back(s);
        }
        // auto scene cut: mean-abs luma diff target vs previous frame
        if (config.autoSceneCut && t > 0 && frames.count(t - 1) && !crossesCut(t, t - 1)) {
            const Observation& prev = frames[t - 1];
            if (prev.width == width && prev.height == height && !target.plane[0].empty()) {
                uint64_t sad = 0;
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x)
                        sad += std::abs(int(target.plane[0][size_t(y) * target.linesize[0] + x])
                                        - int(prev.plane[0][size_t(y) * prev.linesize[0] + x]));
                const double meanDiff = double(sad) / double(width * height);
                if (meanDiff > config.autoCutThreshold)
                    m.events.push_back({t, "scene_cut",
                                        "auto: mean luma diff " + std::to_string(meanDiff)});
            }
        }
        addTiming(m, StageId::WindowSelect, elapsedNs(winStart));

        // correspondence per neighbor
        const Clock::time_point corrStart = Clock::now();
        std::vector<FlowField> flows;
        std::vector<std::vector<Visibility>> visMasks;
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
            FlowField flow = buildFlowField(width, height, blocks,
                                            static_cast<int64_t>(s));
            flows.push_back(std::move(flow));

            // visibility: oracle replaces; default valid, minus unproven
            // correspondence coverage (unknown provenance is never applied).
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
            } else if (config.correspondenceMode == "codec") {
                // mark pixels without proven correspondence as invalid
                for (int y = 0; y < height; ++y)
                    for (int x = 0; x < width; ++x) {
                        const size_t i = size_t(y) * width + x;
                        if (flows.back()[i * 2] == 0.0f && flows.back()[i * 2 + 1] == 0.0f)
                            vis[i] = Visibility::Invalid;
                    }
            }
            visMasks.push_back(std::move(vis));
        }
        rec.correspondenceSource = corrSourceUsed;
        addTiming(m, StageId::Correspondence, elapsedNs(corrStart));

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
                         acc.frame.linesize[0]))
                recordDump(p);
        }
        addTiming(m, StageId::Accumulate, elapsedNs(accStart));

        // output (non-FSR backend)
        const Clock::time_point outStart = Clock::now();
        fs::path outP;
        if (ws.state == WorkingSpaceResult::State::converted) {
            std::vector<uint8_t> rgb;
            int stride = 0;
            if (convertToRgb(acc.frame, rgb, stride)) {
                outP = fs::path(config.outputDir) / ("frame_" + std::to_string(t) + ".ppm");
                if (writePpm(outP.string(), width, height, rgb.data(), stride))
                    result.outputFiles.push_back(outP.string());
            }
        }
        if (outP.empty()) {
            // planes path: Y plane as PGM (chroma planes dumped alongside)
            outP = fs::path(config.outputDir) / ("frame_" + std::to_string(t) + "_y.pgm");
            if (writePgm(outP.string(), width, height, acc.frame.plane[0].data(),
                         acc.frame.linesize[0]))
                result.outputFiles.push_back(outP.string());
            if (acc.frame.planeCount > 1 && !acc.frame.plane[1].empty()) {
                fs::path uP = fs::path(config.outputDir)
                    / ("frame_" + std::to_string(t) + "_u.pgm");
                const int cw = (acc.frame.avPixelFormat == AV_PIX_FMT_YUV420P) ? width / 2 : width;
                const int ch = (acc.frame.avPixelFormat == AV_PIX_FMT_YUV420P) ? height / 2 : height;
                if (writePgm(uP.string(), cw, ch, acc.frame.plane[1].data(),
                             acc.frame.linesize[1]))
                    result.outputFiles.push_back(uP.string());
            }
        }
        addTiming(m, StageId::Output, elapsedNs(outStart));
        m.frames.push_back(rec);

        if (!dumpBase.empty() && stageDumpable(config.dumpStages, StageId::Correspondence)) {
            std::string content = "# frame " + std::to_string(t)
                                + " source=" + corrSourceUsed + "\n";
            for (size_t i = 0; i < neighbors.size(); ++i) {
                content += "neighbor " + std::to_string(neighbors[i]) + "\n";
            }
            const fs::path p = dumpPath(StageId::Correspondence, t, "neighbors.txt");
            if (writeTextFile(p, content)) recordDump(p);
        }
    }

    if (decodeFailed) result.error = decodeError;

    // --- manifest ---
    const fs::path manifestPath = fs::path(config.outputDir) / "manifest.json";
    if (writeTextFile(manifestPath, m.toJson() + "\n"))
        result.outputFiles.push_back(manifestPath.string());
    result.ok = result.error.empty();
    return result;
}

} // namespace anvil
