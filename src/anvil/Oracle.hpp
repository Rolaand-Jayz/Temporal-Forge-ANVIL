// Oracle.hpp — oracle injection for controlled experiments.
//
// Oracle inputs replace estimated stages with ground truth. Formats are
// deterministic text/PNM so fixtures are reviewable:
//   correspondence_<frame>.txt  one block per line:
//     refFrameIndex direction temporalDistance dstX dstY blockW blockH
//     mvX mvY precision frameType intra skip
//   visibility_<frame>.pgm      0=invalid 128=unknown 255=valid
//   geometry_<frame>.txt        state phaseX phaseY   (0=unknown 1=estimated 2=known)
#pragma once
#include <optional>
#include <string>
#include <vector>

#include "Core.hpp"

namespace anvil {

std::optional<std::vector<BlockMotion>> loadOracleCorrespondence(
    const std::string& dir, uint64_t frameIndex);

std::optional<std::vector<Visibility>> loadOracleVisibility(
    const std::string& dir, uint64_t frameIndex, int expectedW, int expectedH);

std::optional<SampleGeometry> loadOracleGeometry(const std::string& dir,
                                                 uint64_t frameIndex);

bool oracleFileExists(const std::string& dir, uint64_t frameIndex);

} // namespace anvil
