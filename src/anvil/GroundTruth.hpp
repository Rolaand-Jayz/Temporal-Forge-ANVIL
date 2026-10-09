// GroundTruth.hpp — HR ground-truth attachment for synthetic fixtures.
//
// Reference truth is evidence only: it is validated and hashed but never fed
// into candidate reconstruction. Same-resolution truth is allowed, while
// genuinely higher-resolution truth is supported for controlled HR→LR tests.
#pragma once
#include <cstdint>
#include <string>

namespace anvil {

struct GroundTruthRecord {
    uint64_t frameIndex = 0;
    std::string path;
    std::string sha256;
    uint64_t sizeBytes = 0;
    int width = 0, height = 0, maxval = 0;
    int bytesPerSample = 1;
    std::string format;
    int observationWidth = 0, observationHeight = 0;
    double scaleX = 1.0, scaleY = 1.0;
    std::string resolutionRelation;
    static constexpr const char* kUsageNote =
        "reference evidence only; excluded from candidate reconstruction";
};

enum class GroundTruthError {
    None = 0,
    Missing,
    Malformed,
    LowerResolutionThanObservation,
    DuplicateFrame,
    FrameOutOfRange,
};

const char* groundTruthErrorName(GroundTruthError e);
GroundTruthError validateGroundTruth(const std::string& path,
                                     int observationWidth, int observationHeight,
                                     GroundTruthRecord& record);
} // namespace anvil
