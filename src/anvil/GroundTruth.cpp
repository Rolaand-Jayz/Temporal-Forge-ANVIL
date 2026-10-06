// GroundTruth.cpp — HR ground-truth validation + provenance.
#include "GroundTruth.hpp"

#include <cctype>
#include <climits>
#include <cstdio>
#include <filesystem>
#include <string>

#include "Sha256.hpp"

namespace fs = std::filesystem;
namespace anvil {

const char* groundTruthErrorName(GroundTruthError e) {
    switch (e) {
        case GroundTruthError::None: return "none";
        case GroundTruthError::Missing: return "missing";
        case GroundTruthError::Malformed: return "malformed";
        case GroundTruthError::LowerResolutionThanObservation:
            return "lower_resolution_than_observation";
        case GroundTruthError::DuplicateFrame: return "duplicate_frame";
        case GroundTruthError::FrameOutOfRange: return "frame_out_of_range";
    }
    return "?";
}

namespace {
bool readToken(FILE* f, std::string& out) {
    out.clear();
    int c = 0;
    for (;;) {
        c = std::fgetc(f);
        if (c == EOF) return false;
        if (std::isspace(static_cast<unsigned char>(c))) continue;
        if (c == '#') {
            while ((c = std::fgetc(f)) != EOF && c != '\n') {}
            if (c == EOF) return false;
            continue;
        }
        break;
    }
    for (;;) {
        out.push_back(static_cast<char>(c));
        c = std::fgetc(f);
        if (c == EOF) return false;
        if (std::isspace(static_cast<unsigned char>(c))) {
            if (c == '\r') {
                const int n = std::fgetc(f);
                if (n != '\n' && n != EOF) std::ungetc(n, f);
            }
            return true;
        }
    }
}

bool positiveInt(const std::string& s, int& out) {
    if (s.empty()) return false;
    uint64_t v = 0;
    for (unsigned char c : s) {
        if (c < '0' || c > '9') return false;
        v = v * 10 + (c - '0');
        if (v > static_cast<uint64_t>(INT_MAX)) return false;
    }
    if (!v) return false;
    out = static_cast<int>(v);
    return true;
}

bool parseHeader(const std::string& path, std::string& magic, int& w, int& h,
                 int& maxval, long& rasterOffset) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    std::string sw, sh, sm;
    const bool ok = readToken(f, magic) && (magic == "P5" || magic == "P6")
                 && readToken(f, sw) && readToken(f, sh) && readToken(f, sm)
                 && positiveInt(sw, w) && positiveInt(sh, h)
                 && positiveInt(sm, maxval) && maxval <= 65535;
    rasterOffset = ok ? std::ftell(f) : -1;
    std::fclose(f);
    return ok && rasterOffset >= 0;
}
} // namespace

GroundTruthError validateGroundTruth(const std::string& path,
                                     int observationWidth, int observationHeight,
                                     GroundTruthRecord& record) {
    std::error_code ec;
    if (!fs::exists(path, ec) || !fs::is_regular_file(path, ec))
        return GroundTruthError::Missing;
    if (observationWidth <= 0 || observationHeight <= 0)
        return GroundTruthError::Malformed;

    std::string magic;
    int w=0,h=0,maxval=0; long raster=0;
    if (!parseHeader(path, magic, w, h, maxval, raster))
        return GroundTruthError::Malformed;

    const uintmax_t channels = magic == "P6" ? 3u : 1u;
    const uintmax_t bps = maxval > 255 ? 2u : 1u;
    std::error_code sec;
    const uintmax_t fileSize = fs::file_size(path, sec);
    if (sec) return GroundTruthError::Malformed;
    const uintmax_t expected = static_cast<uintmax_t>(raster)
        + static_cast<uintmax_t>(w) * h * channels * bps;
    if (fileSize != expected) return GroundTruthError::Malformed;

    if (w < observationWidth || h < observationHeight)
        return GroundTruthError::LowerResolutionThanObservation;

    std::string hash;
    if (!sha256FileHex(path, hash)) return GroundTruthError::Missing;
    record.path=path; record.sha256=hash; record.sizeBytes=fileSize;
    record.width=w; record.height=h; record.maxval=maxval;
    record.bytesPerSample=static_cast<int>(bps);
    record.format = magic=="P6" ? "ppm" : "pgm";
    record.observationWidth=observationWidth;
    record.observationHeight=observationHeight;
    record.scaleX=static_cast<double>(w)/observationWidth;
    record.scaleY=static_cast<double>(h)/observationHeight;
    record.resolutionRelation =
        (w==observationWidth && h==observationHeight)
            ? "same_resolution" : "higher_resolution";
    return GroundTruthError::None;
}
} // namespace anvil
