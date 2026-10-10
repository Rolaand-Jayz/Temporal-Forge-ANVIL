// Sources.hpp — Git-tracked upstream source pins shared by the exhibition
// generator and the review-lab qualification paths.
//
// The pins live in exhibitions/home_field_2026-10/SOURCES.md; these
// constants are the executable form of that file. Changing a pin is a
// dataset identity decision: update SOURCES.md and the constant together,
// with review.
#ifndef ANVIL_LAB_SOURCES_HPP
#define ANVIL_LAB_SOURCES_HPP

namespace anvil_lab {

// sha256 of big_buck_bunny_1080p_h264.mov (© Blender Foundation,
// peach.blender.org, CC BY 3.0) — the exact bytes the Home Field
// Exhibition's real-material scenes were cut from. Any other master is a
// different dataset: canonical exhibitions must reproduce this digest, and
// only explicitly labeled exploratory datasets may deviate.
inline constexpr const char* kCanonicalBbbMasterSha256 =
    "dc2146a2b1172def56730143ad80cd1825b7fad15f1fc9c23a4e7d01a741ac11";

} // namespace anvil_lab

#endif // ANVIL_LAB_SOURCES_HPP
