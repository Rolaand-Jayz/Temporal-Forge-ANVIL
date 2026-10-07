// GroundTruth.cpp — HR ground-truth validation + provenance.
#include "GroundTruth.hpp"

#include <cctype>
#include <climits>
#include <cstdio>
#include <filesystem>
#include <limits>
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
    int v = 0;
    for (unsigned char c : s) {
        if (c < '0' || c > '9') return false;
        const int digit = c - '0';
        if (v > (INT_MAX - digit) / 10) return false;
        v = v * 10 + digit;
    }
    if (v <= 0) return false;
    out = v;
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

bool validateRasterSamples(const std::string& path, long rasterOffset,
                           uintmax_t sampleCount, uintmax_t bytesPerSample,
                           int maxval) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) return false;
    if (std::fseek(f, rasterOffset, SEEK_SET) != 0) {
        std::fclose(f);
        return false;
    }
    bool ok = true;
    for (uintmax_t i = 0; i < sampleCount && ok; ++i) {
        const int hi = std::fgetc(f);
        if (hi == EOF) { ok = false; break; }
        unsigned value = static_cast<unsigned>(hi);
        if (bytesPerSample == 2) {
            const int lo = std::fgetc(f);
            if (lo == EOF) { ok = false; break; }
            value = (value << 8) | static_cast<unsigned>(lo);
        }
        if (value > static_cast<unsigned>(maxval)) ok = false;
    }
    const bool ioError = std::ferror(f) != 0;
    std::fclose(f);
    return ok && !ioError;
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

    uintmax_t sampleCount = static_cast<uintmax_t>(w);
    auto checkedMul = [&](uintmax_t factor) {
        if (factor != 0
            && sampleCount > std::numeric_limits<uintmax_t>::max() / factor)
            return false;
        sampleCount *= factor;
        return true;
    };
    if (!checkedMul(static_cast<uintmax_t>(h)) || !checkedMul(channels))
        return GroundTruthError::Malformed;
    if (sampleCount > std::numeric_limits<uintmax_t>::max() / bps)
        return GroundTruthError::Malformed;
    const uintmax_t rasterBytes = sampleCount * bps;
    const uintmax_t rasterStart = static_cast<uintmax_t>(raster);
    if (rasterStart > std::numeric_limits<uintmax_t>::max() - rasterBytes)
        return GroundTruthError::Malformed;
    const uintmax_t expected = rasterStart + rasterBytes;
    if (fileSize != expected) return GroundTruthError::Malformed;
    if (!validateRasterSamples(path, raster, sampleCount, bps, maxval))
        return GroundTruthError::Malformed;

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
