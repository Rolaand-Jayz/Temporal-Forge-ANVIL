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
#include <utility>
#include <vector>

#include "JsonWriter.hpp"

namespace anvil {

// Pipeline stages in fixed execution order. Side-info normalization is an
// independent stage between window selection and coarse correspondence: raw
// codec side information crosses an explicit raw->normalized boundary before
// any correspondence algorithm consumes it (GOAL.txt capability D).
enum class StageId : uint8_t {
    Decode = 0,
    WindowSelect,
    SideInfoNormalization,
    Correspondence,
    CorrespondenceRefinement,
    Visibility,
    SampleGeometryStage,
    Confidence,
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
        std::string outputDir;
        int64_t startFrame = 0;
        std::optional<int64_t> startPtsUs; // nullopt = frame-index selection
        // Exact selection by native stream ticks (the input stream's own
        // timebase): nullopt = not requested. Distinct native ticks that
        // collide after microsecond rescaling stay individually addressable
        // through this control, which --start-pts-us cannot distinguish.
        std::optional<int64_t> startPtsTicks;
        int64_t frameCount = 1;
        int past = 0;
        int future = 0;
        std::string sideInfoNormalizationMode; // normalize | bypass
        std::string correspondenceMode; // codec | estimate | oracle | none
        std::string refinementMode;     // none | local
        std::string visibilityMode;     // valid | oracle
        std::string confidenceMode;     // unit | estimate | oracle
        std::string geometryMode;       // unknown | estimate | oracle
        std::vector<std::pair<int64_t, int64_t>> excludedNeighbors;
        bool accumulateEnabled = true;
        bool colorConvertEnabled = true;
        std::vector<int64_t> forcedCutFrames;
        bool autoSceneCut = false;
        double autoCutThreshold = 0.0; // mean-abs luma diff [0..255]
        std::string oracleDir;         // empty = none
        std::string dumpDir;           // empty = no dumps
        std::string dumpStages;        // comma list of stage names
        uint64_t seed = 0;             // recorded; deterministic mode has no RNG
        std::string outputBackend;     // pnm | null (identity of the
                                       // replaceable output-backend stage)
        std::string outputFormat;      // backend-provided format label
    } config;

    // --- provenance ---
    struct Provenance {
        std::optional<std::string> gitSha;     // nullopt recorded as null
        std::optional<std::string> gitDirty;   // "true"/"false" when known
        // SHA-256 over (porcelain status + "git diff HEAD") captured when the
        // tree was dirty at build time; nullopt recorded as null. nullopt is
        // the truthful value for a clean tree, a non-repository, or an
        // unprovable state — it must never be guessed.
        std::optional<std::string> gitDirtyHash;
        // Which tier supplied the source identity (never guessed):
        //   "build_generated" | "compile_macro" | "runtime_git" | "unknown"
        std::string provenanceSource;
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
        int bytesPerSample = 1;
        std::string format; // pgm | ppm
        int observationWidth = 0, observationHeight = 0;
        double scaleX = 1.0, scaleY = 1.0;
        std::string resolutionRelation;
        std::string usageNote;
    };
    std::vector<GroundTruthEntry> groundTruth;

    struct OracleArtifact {
        std::string type;
        uint64_t targetFrame = 0;
        std::optional<int64_t> referenceFrame;
        std::string path;
        std::string sha256;
        uint64_t sizeBytes = 0;
    };
    std::vector<OracleArtifact> oracleArtifacts;

    // --- results ---
    std::vector<std::string> outputFiles;
    std::vector<StageTiming> stageTimings;
    std::vector<RunEvent> events;
    std::vector<std::string> dumpFiles;
    struct FrameRecord {
        uint64_t frameIndex = 0;
        int64_t ptsUs = -1;
        // Native timestamp identity: exact stream ticks, the stream timebase
        // they live in, and where the canonical timestamp came from. ptsUs is
        // a convenience rescaling of ptsTicks and is LOSSY for timebases
        // finer than 1/1e6 (distinct ticks can share one microsecond).
        int64_t ptsTicks = -1;
        int timebaseNum = 0;
        int timebaseDen = 0;
        std::string timestampSource;   // pts | best_effort | none
        std::string sideInfoState;   // SideInfoState name
        // Result of the explicit side-info normalization stage:
        // normalized | bypassed | not_applicable.
        std::string sideInfoNormalizationState;
        size_t codecMvCount = 0;
        size_t codecMvUsableCount = 0; // entries with PROVEN reference identity
        std::string correspondenceSource; // which source actually used
        std::string confidenceSource;     // unit | estimate | oracle
        std::string geometryState;
        std::string colorConversion;      // WorkingSpaceResult description
        bool hasGroundTruth = false;
        // Structured color metadata so every manifest consumer can interpret
        // the samples without optional debug dumps. Names use the ColorMeta
        // vocabulary; "unspecified" is preserved verbatim, never guessed.
        struct ColorFields {
            std::string range, primaries, transfer, matrix, chromaLocation;
            std::string pixelFormat;
            int bitDepth = 0;
            bool hasMasteringDisplay = false, hasContentLightLevel = false;
        };
        ColorFields colorSource;   // decoded source space
        ColorFields colorOutput;   // space of the written output artifacts
        uint64_t validSamples = 0;
        uint64_t totalSamples = 0;
    };
    std::vector<FrameRecord> frames;

    // Codec capability matrix (proven at runtime, not assumed).
    struct CodecCapability {
        std::string codec;             // h264 | hevc | av1
        std::string encoder;           // encoder used for the probe, "absent" if none
        bool encoderAvailable = false;
        bool decoderAvailable = false; // decoder implementation is present
        bool decodeProbePassed = false; // synthetic stream decoded completely
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

// Provenance helpers. Each detector resolves through three identity tiers
// and reports nullopt/"unknown" when identity cannot be proven — it never
// guesses. Tier order: compile-macro (CI g++-direct builds define these
// fresh at compile time), build-generated (cmake/AnvilProvenance.cmake
// refreshes the generated unit on every build), runtime git (last resort;
// only meaningful when the process cwd happens to be the repository).
std::optional<std::string> detectGitSha();       // nullopt when unprovable
std::optional<std::string> detectGitDirty();     // "true"/"false", nullopt unknown
std::optional<std::string> detectGitDirtyHash(); // 64-hex or nullopt
std::string detectProvenanceSource();            // tier that supplied the sha

} // namespace anvil
