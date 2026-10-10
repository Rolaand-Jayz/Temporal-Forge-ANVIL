// Resize.hpp — separable high-quality spatial delivery scaler.
//
// The Home Field Exhibition separates temporal reconstruction from spatial
// delivery. ANVIL emits source-resolution output; delivery scaling is an
// EXPLICIT, separately-identified adapter (never folded into reconstruction
// and never used to silently transform native evidence). Two filters with
// distinct transfer behavior: Bicubic (a = -0.5) and Lanczos3.
#pragma once
#include "Image.hpp"

namespace anvil_lab {

enum class ScaleFilter { Bicubic, Lanczos3 };

const char* scaleFilterName(ScaleFilter f);

// Resizes with the given filter. Downscale widens and renormalizes the
// kernel (area-consistent); edges clamp. Double-precision accumulation.
Image resizeImage(const Image& in, int outW, int outH, ScaleFilter filter);

// Resize a single double plane (used by metrics/derivative helpers).
std::vector<double> resizePlane(const std::vector<double>& in, int inW, int inH,
                                int outW, int outH, ScaleFilter filter);

} // namespace anvil_lab
