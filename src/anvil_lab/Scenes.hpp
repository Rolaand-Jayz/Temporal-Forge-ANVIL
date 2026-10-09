// Scenes.hpp — deterministic controlled scene synthesis for the exhibition.
//
// The Home Field Exhibition needs matched scenes with EXACT references:
//   Scene A "Archive Grid Drift"  — fine textures, clear edges, subpixel
//                                   global motion, stable geometry.
//   Scene B "Crossing Occluders"  — multiple local motions, object
//                                   boundaries, occlusion/disocclusion.
// Both masters are rendered from analytic texture fields evaluated at
// fractional coordinates (no resampling of a raster), so the clean HR
// truth is exact and fully reproducible from the recorded parameters and
// seeds. Synthetic fixtures validate the evaluation machinery and measure
// controlled behavior; they are NOT a real-world quality assessment, and
// the exhibition labels them as synthetic everywhere they appear.
#pragma once
#include <cstdint>
#include <string>

#include "Image.hpp"

namespace anvil_lab {

// Deterministic portable PRNG (splitmix64): identical streams on every
// platform and standard-library version, unlike <random> distributions.
struct SplitMix64 {
    uint64_t state = 0;
    explicit SplitMix64(uint64_t seed) : state(seed) {}
    uint64_t next();
    double nextUnit();     // uniform [0,1)
    double nextGaussian(); // Box-Muller, deterministic
private:
    double cached_ = 0.0;
    bool cacheValid_ = false;
};

struct SceneSpec {
    std::string id;        // e.g. "archive_grid_drift"
    std::string label;     // human label, e.g. "Scene A — Archive Grid Drift (synthetic)"
    bool synthetic = true; // false for real-material scenes
    int width = 1280;      // HR master geometry
    int height = 720;
    int frameCount = 46;   // master frames; targets use [2, frameCount-3]
    double driftX = 0.0;   // background global motion (px/frame)
    double driftY = 0.0;
    uint64_t seed = 0;
    std::string profile;   // "detail_subpixel" | "motion_occlusion"
};

SceneSpec sceneSpec(const std::string& sceneId);

// Renders one HR master frame (RGB, maxval 255). Scene A evaluates the
// analytic field at (x + f*driftX, y + f*driftY); Scene B composites moving
// textured shapes over the drifting background.
Image renderSceneFrame(const SceneSpec& spec, int frameIndex);

// Adds independent per-channel Gaussian noise (sigma in 8-bit sample
// units) with a frame-index-seeded stream; the exact model and seed are
// recorded in the scene manifest.
Image addGaussianNoise(const Image& in, uint64_t seed, double sigma);

} // namespace anvil_lab
