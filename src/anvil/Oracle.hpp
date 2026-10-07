// Oracle.hpp — oracle injection for controlled experiments.
//
// Oracle inputs replace estimated stages with ground truth. Formats are
// deterministic text/PNM so fixtures are reviewable:
//   correspondence_<frame>.txt  one block per line:
//     refFrameIndex direction temporalDistance dstX dstY blockW blockH
//     mvX mvY precision frameType intra skip
//   visibility_<frame>_ref<reference>.pgm   0=invalid 128=unknown 255=valid
//     (keyed by target AND temporal reference: past and future neighbors
//      can carry different occlusion ground truth; a mask is NEVER reused
//      across neighbors)
//   confidence_<frame>_ref<reference>.pgm  0..255 => confidence [0,1]
//   geometry_<frame>.txt        state phaseX phaseY   (0=unknown 1=estimated 2=known)
//
// Oracle data is ground truth: malformed or semantically inconsistent
// fixtures are REJECTED with a named reason, never coerced into usable
// motion. Correspondence invariants enforced before any BlockMotion exists:
//   - refFrameIndex >= -1 (-1 = reference identity unknown)
//   - direction 0=past/1=future; temporalDistance sign must match direction
//     (distance 0 = unspecified) — no contradictory metadata
//   - dstX/dstY in [0, 32767]; block extents in [1, 65535]
//   - motion values finite and representable in float
//   - block geometry must fit inside the decoded target frame (checked by
//     the runner, which knows the frame dimensions)
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "Core.hpp"

namespace anvil {

std::optional<std::vector<BlockMotion>> loadOracleCorrespondence(
    const std::string& dir, uint64_t frameIndex, std::string* error = nullptr);

std::optional<std::vector<Visibility>> loadOracleVisibility(
    const std::string& dir, uint64_t frameIndex, int64_t refFrameIndex,
    int expectedW, int expectedH, std::string* error = nullptr);

std::optional<std::vector<float>> loadOracleConfidence(
    const std::string& dir, uint64_t frameIndex, int64_t refFrameIndex,
    int expectedW, int expectedH, std::string* error = nullptr);

std::optional<SampleGeometry> loadOracleGeometry(const std::string& dir,
                                                 uint64_t frameIndex,
                                                 std::string* error = nullptr);

bool oracleFileExists(const std::string& dir, uint64_t frameIndex);

} // namespace anvil
