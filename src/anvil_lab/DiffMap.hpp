// DiffMap.hpp — pixel-difference computation, heatmaps, and change regions.
//
// All difference math runs on ORIGINAL sample data (uint16, source maxval)
// server-side — never on browser screenshot pixels and never on lossy
// display derivatives. Heatmaps and region overlays are VISUALIZATIONS and
// are always returned alongside, never instead of, the numeric statistics.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "Image.hpp"

namespace anvil_lab {

struct DiffStats {
    double meanAbs = 0.0;          // luma mean |a-b| in sample units
    double maxAbs = 0.0;
    uint64_t nonzeroPixels = 0;    // any luma difference >= 1
    uint64_t hotPixels = 0;        // luma |a-b| >= hotThreshold
    double hotRatio = 0.0;
};

// Per-channel absolute difference (sample units), requires matched geometry.
struct AbsDiff {
    std::vector<uint16_t> luma;    // max over channels when RGB
    std::vector<uint16_t> r, g, b; // per channel (empty for gray)
    int width = 0, height = 0;
    int maxval = 255;
};
bool computeAbsDiff(const Image& a, const Image& b, AbsDiff& out, std::string& err);

DiffStats diffStats(const AbsDiff& d, int hotThreshold);

// Heatmap colorization: ANVIL ramp black -> green -> yellow -> red.
// normalized = value / maxval in [0,1]. Returns RGB888.
void heatmapColor(double normalized, uint8_t& rr, uint8_t& gg, uint8_t& bb);

// Renders |diff| as a heatmap image (8-bit RGB display buffer, 2x2x2 px
// legend strip appended at the bottom is NOT included — the UI draws the
// legend from returned scale anchors).
std::vector<uint8_t> renderHeatmap(const AbsDiff& d, double gain);

struct DiffRegion {
    int id = 0;
    int x0 = 0, y0 = 0, x1 = 0, y1 = 0; // inclusive bbox
    uint64_t pixels = 0;                // union pixel count
    double cx = 0.0, cy = 0.0;          // centroid of member pixels
    double meanAbs = 0.0;
    double maxAbs = 0.0;
};

// Threshold >= threshold (sample units), 4-connected components, then
// iteratively merges boxes whose edge gap <= mergePx; surviving groups with
// >= minPixels member pixels are numbered by descending pixel count.
std::vector<DiffRegion> differenceRegions(const AbsDiff& d, int threshold,
                                          uint64_t minPixels, int mergePx);

} // namespace anvil_lab
