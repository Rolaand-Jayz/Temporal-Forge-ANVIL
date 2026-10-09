// Png.cpp — lossless 8-bit PNG encoding via vendored stb_image_write.
#include "Png.hpp"

#include <cstdio>

// Vendored single-header writer; pinned commit recorded in
// external/stb/STB_IMAGE_WRITE_COMMIT.txt.
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STB_IMAGE_WRITE_STATIC
#include "stb_image_write.h"

namespace anvil_lab {
namespace {

bool writeImpl(int w, int h, int comp, const uint8_t* data, const std::string& path,
               std::string& err) {
    if (w <= 0 || h <= 0 || !data) {
        err = "invalid image for PNG write";
        return false;
    }
    if (!stbi_write_png(path.c_str(), w, h, comp, data, w * comp)) {
        err = "stbi_write_png failed for '" + path + "'";
        return false;
    }
    return true;
}

bool encodeImpl(std::vector<uint8_t>& out, int w, int h, int comp,
                const uint8_t* data, std::string& err) {
    struct Ctx {
        std::vector<uint8_t>* out;
    } ctx{&out};
    const int ok = stbi_write_png_to_func(
        [](void* context, void* data, int size) {
            static_cast<Ctx*>(context)->out->insert(
                static_cast<Ctx*>(context)->out->end(),
                static_cast<uint8_t*>(data),
                static_cast<uint8_t*>(data) + size);
        },
        &ctx, w, h, comp, data, w * comp);
    if (!ok) {
        err = "stbi_write_png_to_func failed";
        return false;
    }
    return true;
}

} // namespace

bool writePngRgb(const std::string& path, int width, int height,
                 const uint8_t* rgb, std::string& err) {
    return writeImpl(width, height, 3, rgb, path, err);
}

bool writePngRgba(const std::string& path, int width, int height,
                  const uint8_t* rgba, std::string& err) {
    return writeImpl(width, height, 4, rgba, path, err);
}

bool encodePngRgb(std::vector<uint8_t>& out, int width, int height,
                  const uint8_t* rgb, std::string& err) {
    return encodeImpl(out, width, height, 3, rgb, err);
}

bool encodePngRgba(std::vector<uint8_t>& out, int width, int height,
                   const uint8_t* rgba, std::string& err) {
    return encodeImpl(out, width, height, 4, rgba, err);
}

std::vector<uint8_t> displayRgb(const Image& img) {
    std::vector<uint8_t> buf(img.pixelCount() * 3);
    if (img.isGray()) {
        for (size_t i = 0; i < img.pixelCount(); ++i) {
            const uint8_t v = toDisplay8(img.r[i], img.maxval);
            buf[i * 3] = v;
            buf[i * 3 + 1] = v;
            buf[i * 3 + 2] = v;
        }
        return buf;
    }
    for (size_t i = 0; i < img.pixelCount(); ++i) {
        buf[i * 3] = toDisplay8(img.r[i], img.maxval);
        buf[i * 3 + 1] = toDisplay8(img.g[i], img.maxval);
        buf[i * 3 + 2] = toDisplay8(img.b[i], img.maxval);
    }
    return buf;
}

} // namespace anvil_lab
