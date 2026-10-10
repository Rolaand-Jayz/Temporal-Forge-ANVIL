// Image.hpp — ANVIL Lab sample container.
//
// The Visual Review Lab and the Home Field Exhibition tools read ANVIL's PNM
// artifacts (PPM RGB or planar PGM, 1..16-bit samples) and must never lose
// high-bit-depth evidence while displaying or measuring. Samples are stored
// as uint16_t in their ORIGINAL 0..maxval representation; conversions to
// 8-bit happen only at the display-derivative boundary and are recorded.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

namespace anvil_lab {

struct Image {
    int width = 0;
    int height = 0;
    int maxval = 255;              // PNM maxval; samples are 0..maxval
    std::vector<uint16_t> r;       // plane 0 (luma when gray)
    std::vector<uint16_t> g;       // empty for grayscale images
    std::vector<uint16_t> b;

    size_t pixelCount() const { return static_cast<size_t>(width) * height; }
    bool isGray() const { return g.empty() && b.empty(); }
    int bitDepth() const;          // ceil(log2(maxval+1)), i.e. 8 or 10/12/16

    // ITU-R BT.709 luma from gamma-domain RGB (the exhibition inputs are
    // tagged bt709). Integer samples scaled to double 0..maxval.
    std::vector<double> luma709() const;
};

// Deterministic 8-bit display mapping (rounding, not truncation): the only
// place sample depth is reduced, and the derivative records that it happened.
uint8_t toDisplay8(uint16_t sample, int maxval);

} // namespace anvil_lab
