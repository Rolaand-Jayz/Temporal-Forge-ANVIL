// Image.cpp — ANVIL Lab sample container implementation.
#include "Image.hpp"

namespace anvil_lab {

int Image::bitDepth() const {
    int depth = 1;
    int v = maxval;
    while (v > 1) {
        v >>= 1;
        ++depth;
    }
    return depth;
}

uint8_t toDisplay8(uint16_t sample, int maxval) {
    if (maxval <= 0) maxval = 255;
    const uint32_t scaled =
        (static_cast<uint32_t>(sample) * 255u + static_cast<uint32_t>(maxval) / 2u)
        / static_cast<uint32_t>(maxval);
    return static_cast<uint8_t>(scaled > 255u ? 255u : scaled);
}

std::vector<double> Image::luma709() const {
    std::vector<double> y(pixelCount());
    if (isGray()) {
        for (size_t i = 0; i < y.size(); ++i) y[i] = r[i];
        return y;
    }
    for (size_t i = 0; i < y.size(); ++i)
        y[i] = 0.2126 * r[i] + 0.7152 * g[i] + 0.0722 * b[i];
    return y;
}

} // namespace anvil_lab
