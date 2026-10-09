// Reconstruct.hpp — deterministic correspondence estimation + reconstruction.
//
// Estimate.hpp content: the image estimator is a deterministic block-SAD
// matcher. Its outputs are ALWAYS labeled CorrespondenceSource::ImageEstimate
// — never codec MVs. Codec MV normalization lives in Runner (decode-time) and
// preserves AVMotionVector semantics verbatim.
#pragma once
#include <cstdint>
#include <vector>

#include "Core.hpp"

namespace anvil {

// True only when two observations can be combined numerically without an
// implicit geometry/pixel/color-space conversion.
bool reconstructionSpaceCompatible(const Observation& target,
                                   const Observation& neighbor);

// The current temporal sampler is deliberately scoped to planar 4:2:0
// formats whose sample addressing is explicitly implemented and tested.
bool temporalReconstructionFormatSupported(const Observation& observation);

// Deterministic block-SAD correspondence estimate between `target` (frame t)
// and `obs` (frame s). Blocks are 16x16 over the luma plane. The coarse
// search is a PRIOR on the lattice {-searchRadius, -searchRadius+searchStep,
// ..., +searchRadius} around (0,0): with the default radius 8 step 2 the
// candidates are -8,-6,-4,-2,0,2,4,6,8, so a coarse vector can never land on
// an odd offset (searchStep=1 degenerates to the full exhaustive scan).
// Output blocks carry refFrameIndex = obs.frameIndex (proven by construction
// — the estimator compared exactly those frames).
std::vector<BlockMotion> estimateCorrespondence(const Observation& target,
                                                const Observation& obs,
                                                int blockSize = 16,
                                                int searchRadius = 8,
                                                int searchStep = 2);

// Integer +/-1 residual search around the coarse prior (all 9 candidates,
// strict < — first minimum in scan order wins ties). The two stages form a
// resolution hierarchy: the coarse stage contributes the even-lattice prior,
// refinement supplies the odd-lattice residual precision the coarse stage can
// never select; neither alone covers the full integer displacement field.
// Per-block only — no smoothing across motion boundaries.
std::vector<BlockMotion> refineCorrespondence(const Observation& target,
                                              const Observation& obs,
                                              const std::vector<BlockMotion>& coarse,
                                              int residualRadius = 1);

// Confidence-weighted aligned-average reconstruction of the target frame
// from the window observations. Fully deterministic, CPU-only, no random
// components. Confidence weighting: target frame weight 1; neighbors weight
// = confidence × visibility; samples outside source bounds are invalid.
struct AccumulateResult {
    Observation frame;          // reconstructed output in target pixel format
    uint64_t validSamples = 0;  // accumulated valid neighbor samples (Y)
    uint64_t totalSamples = 0;  // total neighbor samples considered (Y)
};

// flowFor: per-neighbor flow fields mapping target pixels -> neighbor pixels,
// built from block correspondences proven to reference the target frame
// (source Oracle or ImageEstimate, or unambiguous CodecMv). Blocks with
// unknown reference identity are rejected (never silently applied).
using FlowField = std::vector<float>; // 2 floats per pixel (dx, dy)
using ConfidenceField = std::vector<float>; // one value per target pixel [0,1]

// When `coverageOut` is provided it receives 1 byte per pixel: 1 where a
// PROVEN block covers the pixel (including genuinely measured zero motion),
// 0 where no proven correspondence exists. Uncovered pixels must be treated
// as Visibility::Invalid by callers — zero-valued flow there is a
// placeholder, not measured motion.
FlowField buildFlowField(int width, int height, const std::vector<BlockMotion>& blocks,
                         int64_t refFrameIndex,
                         std::vector<uint8_t>* coverageOut = nullptr);

// Apply relative per-observation sampling-grid phase to proven flow:
// neighbor_index = target_index + motion + target_phase - neighbor_phase.
bool applyRelativeSampleGeometry(FlowField& flow,
                                 const std::vector<uint8_t>& coverage,
                                 int width, int height,
                                 const SampleGeometry& targetGeometry,
                                 const SampleGeometry& neighborGeometry);

// Estimated-phase application with the coherence contract the estimated arm
// needs: the correspondence flow's integer part is the block-SAD argmin
// (~round of the true displacement), so the residual the sampler requires is
// D - round(D) in [-0.5, 0.5) per axis — NOT a mod-1 representative. This
// wraps the [0,1) phase difference into [-0.5, 0.5) before applying it, so
// the effective correction is position-exact for BOTH fractional quadrants
// regardless of which representative the estimator emitted (review pass 2:
// the mod-1 representative landed a full pixel away whenever the integer
// flow rounded where the representative ceiled, measurably DEGRADING
// reconstruction on the arm's own subpixel fixture).
bool applyEstimatedPhaseResidual(FlowField& flow,
                                 const std::vector<uint8_t>& coverage,
                                 int width, int height,
                                 float targetPhaseX, float targetPhaseY,
                                 float neighborPhaseX, float neighborPhaseY);

ConfidenceField estimateConfidence(const Observation& target,
                                   const Observation& neighbor,
                                   const FlowField& flow,
                                   const std::vector<uint8_t>& coverage);

AccumulateResult accumulate(const Observation& target,
                            const std::vector<Observation>& neighbors,
                            const std::vector<FlowField>& neighborFlows,
                            const std::vector<std::vector<Visibility>>& neighborVisibility,
                            const std::vector<ConfidenceField>& neighborConfidence);

AccumulateResult accumulate(const Observation& target,
                            const std::vector<Observation>& neighbors,
                            const std::vector<FlowField>& neighborFlows,
                            const std::vector<std::vector<Visibility>>& neighborVisibility);

} // namespace anvil
