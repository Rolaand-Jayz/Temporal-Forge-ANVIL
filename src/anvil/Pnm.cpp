// Pnm.cpp — deterministic PGM/PPM I/O.
#include "Pnm.hpp"

#include <cstdio>
#include <string>

namespace anvil {

namespace {
bool writeAll(const std::string& path, const std::vector<uint8_t>& bytes) {
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) return false;
    const size_t n = std::fwrite(bytes.data(), 1, bytes.size(), f);
    const bool ok = n == bytes.size();
    std::fclose(f);
    return ok;
}
} // namespace

bool writePgm(std::string_view path, int width, int height,
              const uint8_t* data, size_t stride) {
    if (width <= 0 || height <= 0 || !data || stride < static_cast<size_t>(width))
        return false;
    std::string header = "P5\n" + std::to_string(width) + " " + std::to_string(height) + "\n255\n";
    std::vector<uint8_t> bytes(header.begin(), header.end());
    for (int y = 0; y < height; ++y)
        bytes.insert(bytes.end(), data + static_cast<size_t>(y) * stride,
                     data + static_cast<size_t>(y) * stride + width);
    return writeAll(std::string(path), bytes);
}

bool writePpm(std::string_view path, int width, int height,
              const uint8_t* rgb, size_t stride) {
    if (width <= 0 || height <= 0 || !rgb || stride < static_cast<size_t>(width) * 3)
        return false;
    std::string header = "P6\n" + std::to_string(width) + " " + std::to_string(height) + "\n255\n";
    std::vector<uint8_t> bytes(header.begin(), header.end());
    for (int y = 0; y < height; ++y)
        bytes.insert(bytes.end(), rgb + static_cast<size_t>(y) * stride,
                     rgb + static_cast<size_t>(y) * stride + static_cast<size_t>(width) * 3);
    return writeAll(std::string(path), bytes);
}

bool readPgm(std::string_view path, int& width, int& height,
             std::vector<uint8_t>& pixels) {
    FILE* f = std::fopen(std::string(path).c_str(), "rb");
    if (!f) return false;
    int w = 0, h = 0, maxval = 0;
    if (std::fscanf(f, "P5 %d %d %d", &w, &h, &maxval) != 3 || w <= 0 || h <= 0
        || maxval <= 0 || maxval > 255) {
        std::fclose(f);
        return false;
    }
    if (std::fgetc(f) != '\n') { std::fclose(f); return false; }
    pixels.resize(static_cast<size_t>(w) * h);
    const size_t n = std::fread(pixels.data(), 1, pixels.size(), f);
    std::fclose(f);
    if (n != pixels.size()) return false;
    width = w;
    height = h;
    return true;
}

} // namespace anvil
