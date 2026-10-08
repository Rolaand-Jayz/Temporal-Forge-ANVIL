// SampleGeometryEstimate.hpp — deterministic sample-geometry estimation from
// image evidence only.
//
// The Build-Ready contract (zcode_packs/.../BUILD_READY_CONTRACT.md:15) allows
// sample geometry to be known, estimated, or unknown; the later campaign
// contract (LATER_TEST_CAMPAIGN_CONTRACT.md:33) requires an estimated-phase
// arm. This estimator is the "estimated" arm's evidence source: a
// deterministic block subpixel registration over the luma planes. It never
// reads codec motion vectors, oracle data, or any random/time source, and its
// output is always labeled SampleGeometryState::Estimated or explicitly
// Unknown with a truthful reason — never a fabricated phase.
//
// Method (per fully-inside luma tile):
//   1. integer SAD minimum over offsets +/-4 (scan order dy ascending then dx
//      ascending, strict less-than keeps the first minimum — same tie-break
//      as estimateCorrespondence);
//   2. per-axis parabolic subpixel fit through SAD at (d-1, d, d+1):
//      delta = 0.5*(S(d-1)-S(d+1))/(S(d-1)-2*S(d)+S(d+1)), valid only when
//      the denominator is > 0 and |delta| <= 0.5 (otherwise the tile is
//      counted as considered but not used);
//   3. per-block fractional displacement f = integerBest + delta;
//   4. aggregate f per axis by MEDIAN (even count = mean of the two central
//      values) and compute the median absolute deviation (MAD) about it.
// Sufficiency: state Estimated only when blocksUsed >= 8 AND both axis MADs
// are <= 0.25 px; otherwise Unknown with a truthful insufficientReason.
//
// Sign convention (derived, and proven by
// anvil_geometry_estimate_tests.cpp test (d)):
//   The target observation is the anchor (phase 0); the result is the
//   NEIGHBOR's sampling-grid phase relative to the target. If target content
//   at position p is matched by neighbor content at p + f (f = the per-tile
//   displacement measured above), then applyRelativeSampleGeometry
//   (Reconstruct.hpp) adds targetPhase - neighborPhase to the proven flow and
//   the accumulator samples the neighbor at x + flow. With
//   neighborPhase = ((-f) mod 1) in [0,1) the sampler therefore evaluates the
//   neighbor's sampling grid at x - frac(-f), i.e. the fractional position
//   whose grid sample carries the target's content. Because the phase is a
//   [0,1) representative of a mod-1 offset, a zero-flow reconstruction is
//   position-exact when the neighbor's true grid offset lies in (0,1)
//   (positive side); offsets on the negative side are represented mod 1 and
//   need the integer part carried by the flow field's motion.
#pragma once
#include <cstddef>
#include <string>

#include "Core.hpp"

namespace anvil {

struct SampleGeometryEstimateResult {
    SampleGeometry geometry;
    size_t blocksConsidered = 0; // fully-inside tiles with an in-bounds match
    size_t blocksUsed = 0;       // tiles whose subpixel fit was valid both axes
    float madX = 0.0f;           // median absolute deviation of per-tile f, px
    float madY = 0.0f;
    std::string insufficientReason; // non-empty exactly when state == Unknown
};

// Estimate the neighbor's sampling-grid phase relative to `target` from luma
// image evidence only (see header comment for the method and sign
// convention). Malformed, empty, or too-small inputs yield Unknown with a
// non-empty reason — never a synthesized phase.
SampleGeometryEstimateResult estimateRelativeSampleGeometry(
    const Observation& target, const Observation& neighbor,
    int blockSize = 16);

} // namespace anvil
