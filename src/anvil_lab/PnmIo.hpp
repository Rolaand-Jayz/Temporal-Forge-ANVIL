// PnmIo.hpp — full PNM reader/writer for ANVIL Lab tools.
//
// anvil::Pnm covers the runner's deterministic writers and a byte-valued
// PGM reader (visibility oracles). The Lab additionally needs to READ the
// runner's PPM outputs and depth-preserving PGM planes at their true sample
// depth, so this reader handles P5/P6 with maxval 1..65535, comment lines,
// and PNM's required big-endian two-byte raster for maxval > 255.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "Image.hpp"

namespace anvil_lab {

// Reads a P5 (gray) or P6 (RGB) PNM file. Fails closed (returns false,
// sets err) on: magic mismatch, truncated/invalid header, maxval outside
// 1..65535, sample count not matching the declared geometry, or trailing
// garbage beyond the raster.
bool readPnm(const std::string& path, Image& out, std::string& err);

// Parses PNM content already in memory (same strictness as readPnm).
bool parsePnm(const std::vector<uint8_t>& bytes, Image& out, std::string& err);

// Writes P6 (RGB) or P5 (gray), big-endian samples when maxval > 255.
bool writePnm(const std::string& path, const Image& img, std::string& err);

} // namespace anvil_lab
