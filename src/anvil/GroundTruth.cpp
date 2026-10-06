// GroundTruth.cpp — HR ground-truth validation + provenance.
#include "GroundTruth.hpp"

#include <cstdio>
#include <filesystem>

#include "Sha256.hpp"

namespace fs = std::filesystem;

namespace anvil {

const char* groundTruthErrorName(GroundTruthError e) {
    switch (e) {
        case GroundTruthError::None: return "none";
        case GroundTruthError::Missing: return "missing";
        case GroundTruthError::Malformed: return "malformed";
        case GroundTruthError::DimensionMismatch: return "dimension_mismatch";
        case GroundTruthError::DuplicateFrame: return "duplicate_frame";
        case GroundTruthError::FrameOutOfRange: return "frame_out_of_range";
    }
    return "?";
}

namespace {
// Parses a binary PNM header: magic P5/P6, whitespace-separated
// width/height/maxval, single whitespace before raster. Returns false on
// any deviation. `rasterOffset` is where pixel data begins.
bool parsePnmHeader(const std::string& path, std::string& magic, int& width,
                    int& height, int& maxval, long& rasterOffset) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    char c1 = 0, c2 = 0;
    if (std::fread(&c1, 1, 1, f) != 1 || std::fread(&c2, 1, 1, f) != 1
        || c1 != 'P' || (c2 != '5' && c2 != '6')) {
        std::fclose(f);
        return false;
    }
    magic = std::string("P") + c2;
    auto skipWsAndComments = [&]() {
        int c;
        for (;;) {
            c = std::fgetc(f);
            if (c == '#') {
                while (c != '\n' && c != EOF) c = std::fgetc(f);
            } else if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
                // keep scanning
            } else {
                if (c != EOF) std::ungetc(c, f);
                return;
            }
        }
    };
    auto readInt = [&](int& out) {
        skipWsAndComments();
        int v = 0;
        bool any = false;
        int c;
        while ((c = std::fgetc(f)) != EOF && c >= '0' && c <= '9') {
            v = v * 10 + (c - '0');
            any = true;
        }
        if (c != EOF) std::ungetc(c, f);
        out = v;
        return any;
    };
    bool ok = readInt(width) && readInt(height) && readInt(maxval);
    if (ok) {
        skipWsAndComments();
        const long pos = std::ftell(f);
        ok = width > 0 && height > 0 && maxval > 0 && maxval <= 255 && pos >= 0;
        if (ok) rasterOffset = pos;
    }
    std::fclose(f);
    return ok;
}
} // namespace

GroundTruthError validateGroundTruth(const std::string& path,
                                     int expectedWidth, int expectedHeight,
                                     GroundTruthRecord& record) {
    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec))
        return GroundTruthError::Missing;

    std::string magic;
    int w = 0, h = 0, maxval = 0;
    long raster = 0;
    if (!parsePnmHeader(path, magic, w, h, maxval, raster))
        return GroundTruthError::Malformed;
    // Header may claim fewer bytes than the file holds; the file must hold
    // at least the declared raster for its format to be coherent.
    const size_t channels = magic == "P6" ? 3 : 1;
    std::error_code sizeEc;
    const uintmax_t fileSize = fs::file_size(path, sizeEc);
    if (sizeEc) return GroundTruthError::Malformed;
    const uintmax_t needed =
        static_cast<uintmax_t>(raster) + static_cast<uintmax_t>(w) * h * channels;
    if (fileSize < needed) return GroundTruthError::Malformed;

    if (w != expectedWidth || h != expectedHeight)
        return GroundTruthError::DimensionMismatch;

    std::string hash;
    if (!sha256FileHex(path, hash)) return GroundTruthError::Missing;

    record.frameIndex = 0; // set by caller
    record.path = path;
    record.sha256 = hash;
    record.sizeBytes = static_cast<uint64_t>(fileSize);
    record.width = w;
    record.height = h;
    record.maxval = maxval;
    record.format = magic == "P6" ? "ppm" : "pgm";
    return GroundTruthError::None;
}

} // namespace anvil
