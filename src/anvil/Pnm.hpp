// Pnm.hpp — deterministic PGM/PPM writers for captures and dumps.
// PNM is dependency-free and byte-deterministic for fixed inputs.
#pragma once
#include <cstdint>
#include <string_view>
#include <vector>

namespace anvil {

// Writes binary PGM (P5) for single-plane 8-bit data.
bool writePgm(std::string_view path, int width, int height,
              const uint8_t* data, size_t stride);

// Writes binary PPM (P6) for interleaved RGB24.
bool writePpm(std::string_view path, int width, int height,
              const uint8_t* rgb, size_t stride);

// Reads a binary PGM (P5, maxval <= 255). Returns false on any mismatch.
bool readPgm(std::string_view path, int& width, int& height,
             std::vector<uint8_t>& pixels);

} // namespace anvil
