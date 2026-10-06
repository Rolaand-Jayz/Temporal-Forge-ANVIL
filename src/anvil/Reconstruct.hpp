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

// Deterministic block-SAD correspondence estimate between `target` (frame t)
// and `obs` (frame s). Blocks are 16x16 over the luma plane, integer search
// radius around (0,0). Output blocks carry refFrameIndex = obs.frameIndex
// (proven by construction — the estimator compared exactly those frames).
std::vector<BlockMotion> estimateCorrespondence(const Observation& target,
                                                const Observation& obs,
                                                int blockSize = 16,
                                                int searchRadius = 8);

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

FlowField buildFlowField(int width, int height, const std::vector<BlockMotion>& blocks,
                         int64_t refFrameIndex);

AccumulateResult accumulate(const Observation& target,
                            const std::vector<Observation>& neighbors,
                            const std::vector<FlowField>& neighborFlows,
                            const std::vector<std::vector<Visibility>>& neighborVisibility);

} // namespace anvil
