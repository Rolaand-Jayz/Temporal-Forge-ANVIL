// Manifest.hpp — ANVIL run manifest: stages, timings, event log, provenance.
//
// The manifest is the reproducibility contract: exact config, git/build/input
// provenance, per-stage timing, observable events (scene cut, reset,
// fallback, unknown metadata), and the dump inventory. Serialization is
// deterministic (JsonWriter); timing values are wall-clock and are excluded
// from deterministic-replay equality by the contract tests.
#pragma once
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "JsonWriter.hpp"

namespace anvil {

// Pipeline stages in fixed execution order.
enum class StageId : uint8_t {
    Decode = 0,
    WindowSelect,
    Correspondence,
    Visibility,
    SampleGeometryStage,
    ColorConvert,
    Accumulate,
    Output,
};

const char* stageName(StageId id);
StageId stageFromName(const std::string& name, bool& ok);

// One observable pipeline event. Events are the scene-cut/reset/fallback
// record required by the build-ready contract.
struct RunEvent {
    uint64_t frameIndex = 0;
    std::string type;   // "scene_cut" | "window_reset" | "fallback" |
                        // "side_info_unsupported" | "side_info_ambiguous" |
                        // "color_unknown_metadata" | "oracle_used"
    std::string detail;
};

// Per-stage timing record (nanoseconds, wall clock).
struct StageTiming {
    StageId stage = StageId::Decode;
    uint64_t nanoseconds = 0;
};

// Builder for the run manifest JSON.
class Manifest {
public:
    // --- configuration (serialized verbatim) ---
    struct Config {
        std::string inputPath;
        int64_t startFrame = 0;
        int64_t frameCount = 1;
        int past = 0;
        int future = 0;
        std::string correspondenceMode; // codec | estimate | oracle | none
        std::string visibilityMode;     // estimate | oracle | none
        std::string geometryMode;       // unknown | estimate | oracle
        bool accumulateEnabled = true;
        bool colorConvertEnabled = true;
        std::vector<int64_t> forcedCutFrames;
        bool autoSceneCut = false;
        double autoCutThreshold = 0.0; // mean-abs luma diff [0..255]
        std::string oracleDir;         // empty = none
        std::string dumpDir;           // empty = no dumps
        std::string dumpStages;        // comma list of stage names
        uint64_t seed = 0;             // recorded; deterministic mode has no RNG
        std::string outputFormat;      // ppm | pgm_planes
    } config;

    // --- provenance ---
    struct Provenance {
        std::optional<std::string> gitSha;     // nullopt recorded as null
        std::optional<std::string> gitDirty;   // "true"/"false" when known
        std::string ffmpegVersion;             // av_version_info()
        std::string buildType;                 // CMAKE_BUILD_TYPE
        std::string compilerId;                // compiler + version
        std::string inputSha256;
        uint64_t inputSizeBytes = 0;
        bool inputHashOk = false;
    } provenance;

    // HR ground-truth attachments (reference evidence only — excluded from
    // candidate reconstruction; see GroundTruth.hpp).
    struct GroundTruthEntry {
        uint64_t frameIndex = 0;
        std::string path;
        std::string sha256;
        uint64_t sizeBytes = 0;
        int width = 0, height = 0, maxval = 0;
        std::string format; // pgm | ppm
        std::string usageNote;
    };
    std::vector<GroundTruthEntry> groundTruth;

    // --- results ---
    std::vector<std::string> outputFiles;
    std::vector<StageTiming> stageTimings;
    std::vector<RunEvent> events;
    std::vector<std::string> dumpFiles;
    struct FrameRecord {
        uint64_t frameIndex = 0;
        int64_t ptsUs = -1;
        std::string sideInfoState;   // SideInfoState name
        size_t codecMvCount = 0;
        std::string correspondenceSource; // which source actually used
        std::string geometryState;
        std::string colorConversion;      // WorkingSpaceResult description
        bool hasGroundTruth = false;
        uint64_t validSamples = 0;
        uint64_t totalSamples = 0;
    };
    std::vector<FrameRecord> frames;

    // Codec capability matrix (proven at runtime, not assumed).
    struct CodecCapability {
        std::string codec;             // h264 | hevc | av1
        std::string encoder;           // encoder used for the probe, "absent" if none
        bool encoderAvailable = false;
        bool decoderAvailable = false;
        bool mvExportProven = false;   // MEASURED: MV side data observed on decode
        int probeMvFrames = 0;         // frames carrying MV side data in the probe
        int probeTotalFrames = 0;
        std::string note;              // truthful limitation note
    };
    std::vector<CodecCapability> codecCapabilities;

    // Software/hardware decode behavior actually used for the input.
    std::string decodeMode = "software"; // successor forces software decode

    std::string toJson() const;
};

// Provenance helpers.
std::optional<std::string> detectGitSha();       // nullopt when not a repo
std::optional<std::string> detectGitDirty();     // "true"/"false", nullopt unknown

} // namespace anvil
