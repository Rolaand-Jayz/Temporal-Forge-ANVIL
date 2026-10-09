// Metrics.hpp — full-reference and reference-free quality measurements.
//
// Scientific-integrity rules encoded here:
// - PSNR/SSIM are only computed against a legitimately matched reference
//   (same geometry, same sample depth); the caller guarantees pairing and
//   records whether a reference was valid. Reference-free callers must not
//   invoke these functions.
// - SSIM uses the standard 11x11 Gaussian window (sigma 1.5) with
//   C1=(0.01 L)^2, C2=(0.03 L)^2 where L is the shared maxval, computed on
//   BT.709 luma; only fully-supported window positions contribute.
// - Temporal-stability metrics measure consecutive OUTPUT frames of one arm
//   (an indicator of flicker), never across arms.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "Image.hpp"

namespace anvil_lab {

struct PlaneMetrics {
    double mse = 0.0;          // mean squared error (luma)
    double psnr = -1.0;        // dB; -1 marks infinite (identical) — check `identical`
    bool identical = false;    // zero mean abs difference
    double ssim = 1.0;         // mean SSIM over valid windows
    double meanAbsDiff = 0.0;  // luma mean absolute difference
    double maxAbsDiff = 0.0;   // luma max absolute difference (sample units)
    double edgeDiffMean = 0.0; // mean abs Sobel-magnitude difference (luma)
};

// Requires equal geometry and maxval; returns false (with err) otherwise —
// the caller must treat that as an invalid pairing, not a zero score.
bool computePlaneMetrics(const Image& a, const Image& b, PlaneMetrics& out,
                         std::string& err);

// Mean absolute luma difference between two consecutive frames of the SAME
// arm (temporal flicker indicator). Same pairing requirements.
double temporalDelta(const Image& frameA, const Image& frameB, std::string& err);

} // namespace anvil_lab
