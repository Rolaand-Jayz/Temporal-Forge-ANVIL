// Scenes.cpp — deterministic synthetic masters for the exhibition.
#include "Scenes.hpp"

#include <algorithm>
#include <cmath>

namespace anvil_lab {

uint64_t SplitMix64::next() {
    uint64_t z = (state += 0x9E3779B97F4A7C15ull);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

double SplitMix64::nextUnit() {
    return static_cast<double>(next() >> 11) * (1.0 / 9007199254740992.0);
}

double SplitMix64::nextGaussian() {
    // Box-Muller with a cached second value keeps the stream advance at
    // two draws per pair; determinism is per-call-order, documented here.
    double u1 = nextUnit();
    while (u1 <= 0.0) u1 = nextUnit();
    const double u2 = nextUnit();
    const double mag = std::sqrt(-2.0 * std::log(u1));
    if (cacheValid_) {
        cacheValid_ = false;
        return mag * std::sin(2.0 * M_PI * cached_);
    }
    // First of the pair: return cos, cache sin.
    cached_ = u2;
    cacheValid_ = true;
    return mag * std::cos(2.0 * M_PI * u2);
}

namespace {

inline double clamp01(double v) { return v < 0.0 ? 0.0 : (v > 1.0 ? 1.0 : v); }

double softLine(double d, double sigma) { // softened 1-px line profile
    return std::exp(-(d * d) / (2.0 * sigma * sigma));
}

uint64_t hash2(uint64_t x, uint64_t y, uint64_t salt) {
    SplitMix64 s(x * 0x9E3779B97F4A7C15ull ^ y * 0xC2B2AE3D27D4EB4Full
                 ^ salt * 0x165667B19E3779F9ull);
    return s.next();
}

// Scene A: fine print/grating archive texture, band-limited enough for
// 720p sampling; evaluated at fractional coordinates.
void archiveGrid(double u, double v, double& r, double& g, double& b) {
    double l = 0.30 + 0.10 * std::sin(v / 91.0) + 0.04 * std::sin(u / 137.0 + 0.8);
    l += 0.085 * std::sin(2.0 * M_PI * u / 9.0 + 1.7);
    l += 0.045 * std::sin(2.0 * M_PI * (0.31 * u + 0.95 * v) / 6.2);
    l += 0.05 * std::sin(2.0 * M_PI * v / 13.0 + 0.4);
    // Fine grid lines every 40 px.
    const double gx = std::fmod(std::fabs(u), 40.0);
    const double gy = std::fmod(std::fabs(v), 40.0);
    const double dx = std::min(gx, 40.0 - gx);
    const double dy = std::min(gy, 40.0 - gy);
    l -= 0.10 * (softLine(dx, 0.9) + softLine(dy, 0.9));
    // Pseudo-print glyph blocks: 7x5 cell dot matrices in 48x60 px rows.
    const int cellX = static_cast<int>(std::floor(u / 8.0));
    const int cellY = static_cast<int>(std::floor(v / 8.0));
    const double fx = std::fmod(u, 8.0), fy = std::fmod(v, 8.0);
    if (fx > 1.4 && fx < 5.6 && fy > 1.4 && fy < 5.6
        && (hash2(static_cast<uint64_t>(cellX), static_cast<uint64_t>(cellY), 7) & 3)) {
        l += 0.16;
    }
    // A few soft mid-scale blobs for tonal variety.
    const double b1 = softLine(std::hypot(u - 340.0, v - 210.0) - 120.0, 60.0);
    const double b2 = softLine(std::hypot(u - 950.0, v - 520.0) - 90.0, 50.0);
    l += 0.10 * b1 + 0.08 * b2;
    l = clamp01(l);
    // Slight sepia cast so channels differ measurably.
    r = clamp01(l * 1.04 + 0.012);
    g = clamp01(l * 0.99);
    b = clamp01(l * 0.90);
}

// Scene B background: broad blobs + medium checker + stripes.
void meadowGrid(double u, double v, double& r, double& g, double& b) {
    double l = 0.32 + 0.10 * std::sin(2.0 * M_PI * (u + 2.0 * v) / 61.0);
    const int cx = static_cast<int>(std::floor(u / 26.0));
    const int cy = static_cast<int>(std::floor(v / 26.0));
    if (((cx + cy) & 1) == 0) l += 0.06;
    l += 0.05 * std::sin(2.0 * M_PI * v / 17.0 + 1.1);
    const double b1 = softLine(std::hypot(u - 420.0, v - 300.0) - 150.0, 70.0);
    const double b2 = softLine(std::hypot(u - 900.0, v - 150.0) - 110.0, 55.0);
    const double b3 = softLine(std::hypot(u - 640.0, v - 620.0) - 130.0, 65.0);
    l += 0.09 * b1 + 0.07 * b2 + 0.06 * b3;
    l = clamp01(l);
    r = clamp01(l * 0.92);
    g = clamp01(l * 1.03);
    b = clamp01(l * 0.95);
}

struct Shape {
    double x0, y0, w, h;    // top-left + size at frame 0 (px)
    double vx, vy;          // px/frame
    int z;                  // painter's order (higher = on top)
    int pattern;            // texture selector
};

const Shape kShapes[] = {
    {240, 140, 300, 220, 1.35, 0.42, 3, 0}, // fast diagonal hatch slab (top z)
    {560, 300, 260, 300, -1.60, 0.55, 2, 1}, // counter-moving ring slab; edges
                                            // meet the hatch slab ~frame 7 and
                                            // stay overlapped (sustained
                                            // occlusion + boundary disocclusion)
    {520, 520, 200, 160, 0.50, -0.90, 1, 2}, // rising grid chip occluded by
                                            // the ring slab late in the run
};

void shapeTexture(int pattern, double lx, double ly, double& r, double& g, double& b) {
    double l = 0.55;
    if (pattern == 0) { // fine diagonal hatch + border
        l += 0.14 * std::sin(2.0 * M_PI * (lx + ly) / 8.0);
        const double bx = std::min(lx, 300.0 - lx);
        const double by = std::min(ly, 220.0 - ly);
        if (bx < 3.0 || by < 3.0) l = 0.22;
        r = clamp01(l * 1.02);
        g = clamp01(l * 0.80);
        b = clamp01(l * 0.70);
        return;
    }
    if (pattern == 1) { // concentric rings
        const double d = std::hypot(lx - 130.0, ly - 150.0);
        l += 0.15 * std::sin(2.0 * M_PI * d / 22.0);
        r = clamp01(l * 0.75);
        g = clamp01(l * 0.85);
        b = clamp01(l * 1.05);
        return;
    }
    // pattern 2: fine grid
    const double fx = std::fmod(lx, 10.0), fy = std::fmod(ly, 10.0);
    if (fx < 1.6 || fy < 1.6) l = 0.25;
    else l = 0.68;
    r = clamp01(l * 0.85);
    g = clamp01(l * 0.95);
    b = clamp01(l * 1.02);
}

} // namespace

SceneSpec sceneSpec(const std::string& sceneId) {
    SceneSpec s;
    if (sceneId == "archive_grid_drift") {
        s.id = sceneId;
        s.label = "Scene A — Archive Grid Drift (synthetic, detail + subpixel motion)";
        s.synthetic = true;
        s.width = 1280;
        s.height = 720;
        s.frameCount = 46;
        s.driftX = 0.37;
        s.driftY = 0.11;
        s.seed = 0xA11CE;
        s.profile = "detail_subpixel";
    } else if (sceneId == "crossing_occluders") {
        s.id = sceneId;
        s.label = "Scene B — Crossing Occluders (synthetic, motion + occlusion)";
        s.synthetic = true;
        s.width = 1280;
        s.height = 720;
        s.frameCount = 46;
        s.driftX = 0.21;
        s.driftY = -0.13;
        s.seed = 0xB0CCE;
        s.profile = "motion_occlusion";
    } else {
        s.id = sceneId;
        s.label = sceneId;
    }
    return s;
}

Image renderSceneFrame(const SceneSpec& spec, int frameIndex) {
    Image img;
    img.width = spec.width;
    img.height = spec.height;
    img.maxval = 255;
    img.r.assign(img.pixelCount(), 0);
    img.g.assign(img.pixelCount(), 0);
    img.b.assign(img.pixelCount(), 0);
    const double fu = spec.driftX * frameIndex;
    const double fv = spec.driftY * frameIndex;

    for (int y = 0; y < spec.height; ++y) {
        for (int x = 0; x < spec.width; ++x) {
            double r = 0, g = 0, b = 0;
            if (spec.profile == "detail_subpixel") {
                archiveGrid(x + fu, y + fv, r, g, b);
            } else {
                meadowGrid(x + fu, y + fv, r, g, b);
                // Topmost covering shape wins (z 3 > 2 > 1); shape texture
                // is shape-attached so boundaries move with the shape.
                const Shape* top = nullptr;
                double topLx = 0, topLy = 0;
                for (const Shape& sh : kShapes) {
                    const double sx = sh.x0 + sh.vx * frameIndex;
                    const double sy = sh.y0 + sh.vy * frameIndex;
                    if (x >= sx && x < sx + sh.w && y >= sy && y < sy + sh.h) {
                        if (!top || sh.z > top->z) {
                            top = &sh;
                            topLx = x - sx;
                            topLy = y - sy;
                        }
                    }
                }
                if (top) shapeTexture(top->pattern, topLx, topLy, r, g, b);
            }
            const size_t i = static_cast<size_t>(y) * spec.width + x;
            img.r[i] = static_cast<uint16_t>(std::lround(clamp01(r) * 255.0));
            img.g[i] = static_cast<uint16_t>(std::lround(clamp01(g) * 255.0));
            img.b[i] = static_cast<uint16_t>(std::lround(clamp01(b) * 255.0));
        }
    }
    return img;
}

Image addGaussianNoise(const Image& in, uint64_t seed, double sigma) {
    Image out = in;
    SplitMix64 rng(seed);
    for (size_t i = 0; i < out.pixelCount(); ++i) {
        if (!out.isGray()) {
            out.g[i] = static_cast<uint16_t>(std::lround(
                std::clamp<double>(double(out.g[i]) + sigma * rng.nextGaussian(), 0.0, 255.0)));
            out.b[i] = static_cast<uint16_t>(std::lround(
                std::clamp<double>(double(out.b[i]) + sigma * rng.nextGaussian(), 0.0, 255.0)));
        }
        out.r[i] = static_cast<uint16_t>(std::lround(
            std::clamp<double>(double(out.r[i]) + sigma * rng.nextGaussian(), 0.0, 255.0)));
    }
    return out;
}

} // namespace anvil_lab
