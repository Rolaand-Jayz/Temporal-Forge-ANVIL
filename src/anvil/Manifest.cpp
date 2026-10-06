// Manifest.cpp — run manifest serialization + provenance detection.
#include "Manifest.hpp"

#include <cstdio>

extern "C" {
#include <libavutil/ffversion.h>
}

namespace anvil {

const char* stageName(StageId id) {
    switch (id) {
        case StageId::Decode: return "decode";
        case StageId::WindowSelect: return "window_select";
        case StageId::Correspondence: return "correspondence";
        case StageId::Visibility: return "visibility";
        case StageId::SampleGeometryStage: return "sample_geometry";
        case StageId::ColorConvert: return "color_convert";
        case StageId::Accumulate: return "accumulate";
        case StageId::Output: return "output";
    }
    return "?";
}

StageId stageFromName(const std::string& name, bool& ok) {
    ok = true;
    if (name == "decode") return StageId::Decode;
    if (name == "window_select") return StageId::WindowSelect;
    if (name == "correspondence") return StageId::Correspondence;
    if (name == "visibility") return StageId::Visibility;
    if (name == "sample_geometry") return StageId::SampleGeometryStage;
    if (name == "color_convert") return StageId::ColorConvert;
    if (name == "accumulate") return StageId::Accumulate;
    if (name == "output") return StageId::Output;
    ok = false;
    return StageId::Decode;
}

std::optional<std::string> detectGitSha() {
    FILE* p = popen("git rev-parse HEAD 2>/dev/null", "r");
    if (!p) return std::nullopt;
    char buf[64] = {};
    const bool ok = std::fgets(buf, sizeof(buf), p) != nullptr;
    pclose(p);
    if (!ok || buf[0] != static_cast<char>('0')) return std::nullopt; // not a sha
    std::string s(buf);
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    if (s.size() != 40) return std::nullopt;
    return s;
}

std::optional<std::string> detectGitDirty() {
    FILE* p = popen("git status --porcelain 2>/dev/null", "r");
    if (!p) return std::nullopt;
    char buf[256];
    bool any = std::fgets(buf, sizeof(buf), p) != nullptr;
    const bool closedOk = pclose(p) == 0;
    if (!closedOk) return std::nullopt;
    return std::string(any ? "true" : "false");
}

std::string Manifest::toJson() const {
    JsonWriter w;
    w.beginObject();

    w.object("config");
    w.kv("input_path", config.inputPath);
    w.kv("start_frame", config.startFrame);
    w.kv("frame_count", config.frameCount);
    w.kv("past", config.past);
    w.kv("future", config.future);
    w.kv("correspondence_mode", config.correspondenceMode);
    w.kv("visibility_mode", config.visibilityMode);
    w.kv("geometry_mode", config.geometryMode);
    w.kv("accumulate_enabled", config.accumulateEnabled);
    w.kv("color_convert_enabled", config.colorConvertEnabled);
    w.array("forced_cut_frames");
    for (int64_t f : config.forcedCutFrames) w.value(f);
    w.endArray();
    w.kv("auto_scene_cut", config.autoSceneCut);
    w.kv("auto_cut_threshold", config.autoCutThreshold);
    w.kv("oracle_dir", config.oracleDir);
    w.kv("dump_dir", config.dumpDir);
    w.kv("dump_stages", config.dumpStages);
    w.kv("seed", config.seed);
    w.kv("deterministic_no_random_components", true);
    w.kv("output_format", config.outputFormat);
    w.endObject(); // config

    w.object("provenance");
    if (provenance.gitSha) w.kv("git_sha", *provenance.gitSha);
    else w.key("git_sha"), w.null();
    if (provenance.gitDirty) w.kv("git_dirty", *provenance.gitDirty);
    else w.key("git_dirty"), w.null();
    w.kv("ffmpeg_version", provenance.ffmpegVersion);
    w.kv("build_type", provenance.buildType);
    w.kv("compiler", provenance.compilerId);
    w.kv("input_sha256", provenance.inputSha256);
    w.kv("input_size_bytes", provenance.inputSizeBytes);
    w.kv("input_hash_ok", provenance.inputHashOk);
    w.kv("decode_mode", decodeMode);
    w.endObject(); // provenance

    w.object("codec_capabilities");
    for (const auto& c : codecCapabilities) {
        w.object(c.codec);
        w.kv("encoder", c.encoder);
        w.kv("encoder_available", c.encoderAvailable);
        w.kv("decoder_available", c.decoderAvailable);
        w.kv("mv_export_proven", c.mvExportProven);
        w.kv("probe_mv_frames", c.probeMvFrames);
        w.kv("probe_total_frames", c.probeTotalFrames);
        w.kv("note", c.note);
        w.endObject();
    }
    w.endObject();

    w.array("stage_timings");
    for (const auto& t : stageTimings) {
        w.beginObject();
        w.kv("stage", stageName(t.stage));
        w.kv("nanoseconds", t.nanoseconds);
        w.endObject();
    }
    w.endArray();

    w.array("events");
    for (const auto& e : events) {
        w.beginObject();
        w.kv("frame", e.frameIndex);
        w.kv("type", e.type);
        w.kv("detail", e.detail);
        w.endObject();
    }
    w.endArray();

    w.array("ground_truth");
    for (const auto& g : groundTruth) {
        w.beginObject();
        w.kv("frame_index", g.frameIndex);
        w.kv("path", g.path);
        w.kv("sha256", g.sha256);
        w.kv("size_bytes", g.sizeBytes);
        w.kv("width", g.width);
        w.kv("height", g.height);
        w.kv("maxval", g.maxval);
        w.kv("format", g.format);
        w.kv("usage", g.usageNote);
        w.endObject();
    }
    w.endArray();

    w.array("output_files");
    for (const auto& o : outputFiles) w.value(o);
    w.endArray();

    w.array("dump_files");
    for (const auto& d : dumpFiles) w.value(d);
    w.endArray();

    w.array("frames");
    for (const auto& fr : frames) {
        w.beginObject();
        w.kv("frame_index", fr.frameIndex);
        w.kv("pts_us", fr.ptsUs);
        w.kv("side_info_state", fr.sideInfoState);
        w.kv("codec_mv_count", fr.codecMvCount);
        w.kv("correspondence_source", fr.correspondenceSource);
        w.kv("geometry_state", fr.geometryState);
        w.kv("color_conversion", fr.colorConversion);
        w.kv("has_ground_truth", fr.hasGroundTruth);
        w.kv("valid_samples", fr.validSamples);
        w.kv("total_samples", fr.totalSamples);
        w.endObject();
    }
    w.endArray();

    w.endObject();
    return w.str();
}

} // namespace anvil
