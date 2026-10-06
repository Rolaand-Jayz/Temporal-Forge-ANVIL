// GroundTruth.hpp — HR ground-truth attachment for synthetic fixtures.
//
// Ground truth is REFERENCE EVIDENCE ONLY: the runner validates its header
// and hashes its bytes, but its pixels are never loaded into the
// reconstruction pipeline. This keeps later metrics uncontaminated by
// oracle leakage; an experiment that wants truth-guided reconstruction must
// request it through a separate, explicitly-recorded mechanism.
//
// Interface: repeatable `--ground-truth FRAME=PATH` CLI mapping. Each entry
// is validated against the decoded target frame (dimensions, format) before
// acceptance; provenance (SHA-256, size, dimensions, maxval, format) is
// recorded in the run manifest under "ground_truth".
#pragma once
#include <cstdint>
#include <string>

namespace anvil {

struct GroundTruthRecord {
    uint64_t frameIndex = 0;
    std::string path;
    std::string sha256;      // lowercase hex of the whole file
    uint64_t sizeBytes = 0;
    int width = 0;
    int height = 0;
    int maxval = 0;
    std::string format;      // "pgm" (P5) | "ppm" (P6)
    // Fixed truthfulness note serialized verbatim into the manifest.
    static constexpr const char* kUsageNote =
        "reference evidence only; excluded from candidate reconstruction";
};

enum class GroundTruthError {
    None = 0,
    Missing,            // file does not exist / unreadable
    Malformed,          // not a P5/P6 PNM, bad header, truncated data
    DimensionMismatch,  // dims differ from the decoded target frame
    DuplicateFrame,     // two ground truths mapped to the same frame
    FrameOutOfRange,    // mapped frame is not a target of this run
};

const char* groundTruthErrorName(GroundTruthError e);

// Validates PATH as HR ground truth for a frame whose decoded planes are
// expectedW x expectedH. On success fills `record` (hash, size, dims,
// format). Only the PNM header and file bytes are read; pixel data is never
// returned to the pipeline.
GroundTruthError validateGroundTruth(const std::string& path,
                                     int expectedWidth, int expectedHeight,
                                     GroundTruthRecord& record);

} // namespace anvil
