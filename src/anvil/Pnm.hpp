// Pnm.hpp — deterministic PGM/PPM I/O.
#pragma once
#include <cstdint>
#include <string_view>
#include <vector>

namespace anvil {

// P5 writer for 8..16-bit source planes. FFmpeg high-bit-depth LE samples
// are serialized as the big-endian two-byte raster required by PNM.
bool writePgm(std::string_view path, int width, int height,
              const uint8_t* data, size_t stride, int bitDepth = 8);

bool writePpm(std::string_view path, int width, int height,
              const uint8_t* rgb, size_t stride);

// Visibility oracle reader intentionally remains byte-valued.
bool readPgm(std::string_view path, int& width, int& height,
             std::vector<uint8_t>& pixels, int* maxvalOut = nullptr);
} // namespace anvil
