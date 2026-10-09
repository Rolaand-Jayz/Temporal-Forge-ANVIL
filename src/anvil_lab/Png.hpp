// Png.hpp — lossless 8-bit PNG encoding for validated display derivatives.
//
// Browsers do not render PNM and must not receive lossy re-encodings of
// scientific artifacts. Every PNG the Lab serves is a derivative whose
// relationship to the original PNM is recorded (original sha256 + derivative
// sha256 in the derivative index), so a reviewer can always tell display
// data apart from evidence. Encoding uses the vendored stb_image_write
// (external/stb, public-domain/MIT) — recorded in THIRD_PARTY_LICENSES.md.
#pragma once
#include <cstdint>
#include <string>
#include <vector>

#include "Image.hpp"

namespace anvil_lab {

// Writes an 8-bit RGB PNG (lossless, no chroma subsampling).
bool writePngRgb(const std::string& path, int width, int height,
                 const uint8_t* rgb, std::string& err);

// Writes an 8-bit RGBA PNG (alpha for overlays: region boxes, heatmaps).
bool writePngRgba(const std::string& path, int width, int height,
                  const uint8_t* rgba, std::string& err);

// Encodes to an in-memory PNG buffer (same formats) for HTTP responses.
bool encodePngRgb(std::vector<uint8_t>& out, int width, int height,
                  const uint8_t* rgb, std::string& err);
bool encodePngRgba(std::vector<uint8_t>& out, int width, int height,
                   const uint8_t* rgba, std::string& err);

// Converts a Lab Image to an 8-bit RGB display buffer using the recorded
// deterministic toDisplay8 mapping.
std::vector<uint8_t> displayRgb(const Image& img);

} // namespace anvil_lab
