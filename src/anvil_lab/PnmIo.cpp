// PnmIo.cpp — full PNM reader/writer for ANVIL Lab tools.
#include "PnmIo.hpp"

#include <cstdio>

namespace anvil_lab {
namespace {

// Header tokenizer per PNM spec: whitespace-separated tokens, '#' starts a
// comment running to end of line. Returns false when the input ends before
// `count` tokens are collected.
bool nextToken(const std::vector<uint8_t>& b, size_t& pos, std::string& tok) {
    tok.clear();
    while (pos < b.size()) {
        const uint8_t c = b[pos];
        if (c == '#') {
            while (pos < b.size() && b[pos] != '\n') ++pos;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            if (!tok.empty()) return true;
            ++pos;
            continue;
        }
        tok.push_back(static_cast<char>(c));
        ++pos;
        if (tok.size() > 16) return false; // header fields are bounded
    }
    return !tok.empty();
}

} // namespace

bool parsePnm(const std::vector<uint8_t>& bytes, Image& out, std::string& err) {
    size_t pos = 0;
    std::string magic;
    if (!nextToken(bytes, pos, magic) || (magic != "P5" && magic != "P6")) {
        err = "not a P5/P6 PNM (magic token '" + magic + "')";
        return false;
    }
    const bool rgb = magic == "P6";
    std::string wTok, hTok, mTok;
    if (!nextToken(bytes, pos, wTok) || !nextToken(bytes, pos, hTok)
        || !nextToken(bytes, pos, mTok)) {
        err = "truncated PNM header";
        return false;
    }
    long w = 0, h = 0, m = 0;
    try {
        w = std::stol(wTok);
        h = std::stol(hTok);
        m = std::stol(mTok);
    } catch (...) {
        err = "non-numeric PNM header field";
        return false;
    }
    if (w <= 0 || h <= 0 || w > 65535 || h > 65535) {
        err = "PNM geometry out of supported range";
        return false;
    }
    if (m < 1 || m > 65535) {
        err = "PNM maxval outside 1..65535";
        return false;
    }
    // Exactly one whitespace byte separates maxval from the raster.
    if (pos >= bytes.size()) {
        err = "missing raster start whitespace";
        return false;
    }
    ++pos;

    const size_t pixels = static_cast<size_t>(w) * static_cast<size_t>(h);
    const size_t comps = rgb ? 3 : 1;
    const size_t bytesPer = m > 255 ? 2 : 1;
    const size_t need = pixels * comps * bytesPer;
    if (bytes.size() - pos < need) {
        err = "truncated PNM raster";
        return false;
    }
    if (bytes.size() - pos > need) {
        err = "trailing bytes after PNM raster";
        return false;
    }

    out.width = static_cast<int>(w);
    out.height = static_cast<int>(h);
    out.maxval = static_cast<int>(m);
    out.r.assign(pixels, 0);
    if (rgb) {
        out.g.assign(pixels, 0);
        out.b.assign(pixels, 0);
    } else {
        out.g.clear();
        out.b.clear();
    }
    for (size_t i = 0; i < pixels * comps; ++i) {
        unsigned v = bytes[pos + i * bytesPer];
        if (bytesPer == 2)
            v = (v << 8) | bytes[pos + i * bytesPer + 1];
        if (v > static_cast<unsigned>(m)) {
            err = "sample exceeds declared maxval";
            return false;
        }
        const size_t p = i / comps;
        switch (i % comps) {
            case 0: out.r[p] = static_cast<uint16_t>(v); break;
            case 1: out.g[p] = static_cast<uint16_t>(v); break;
            default: out.b[p] = static_cast<uint16_t>(v); break;
        }
    }
    return true;
}

bool readPnm(const std::string& path, Image& out, std::string& err) {
    FILE* f = std::fopen(path.c_str(), "rb");
    if (!f) {
        err = "cannot open '" + path + "' for reading";
        return false;
    }
    std::vector<uint8_t> bytes;
    uint8_t buf[65536];
    size_t n = 0;
    while ((n = std::fread(buf, 1, sizeof buf, f)) > 0)
        bytes.insert(bytes.end(), buf, buf + n);
    const bool ok = std::ferror(f) == 0;
    std::fclose(f);
    if (!ok) {
        err = "read error on '" + path + "'";
        return false;
    }
    return parsePnm(bytes, out, err);
}

bool writePnm(const std::string& path, const Image& img, std::string& err) {
    if (img.width <= 0 || img.height <= 0 || img.maxval < 1 || img.maxval > 65535
        || img.r.size() != img.pixelCount()) {
        err = "invalid image for PNM write";
        return false;
    }
    const bool rgb = !img.isGray();
    if (rgb && (img.g.size() != img.pixelCount() || img.b.size() != img.pixelCount())) {
        err = "channel size mismatch for PPM write";
        return false;
    }
    FILE* f = std::fopen(path.c_str(), "wb");
    if (!f) {
        err = "cannot open '" + path + "' for writing";
        return false;
    }
    std::fprintf(f, "%s\n%d %d\n%d\n", rgb ? "P6" : "P5", img.width, img.height,
                 img.maxval);
    const size_t pixels = img.pixelCount();
    const size_t comps = rgb ? 3 : 1;
    const size_t bytesPer = img.maxval > 255 ? 2 : 1;
    std::vector<uint8_t> raster(pixels * comps * bytesPer);
    for (size_t i = 0; i < pixels * comps; ++i) {
        uint16_t v = 0;
        const size_t p = i / comps;
        switch (i % comps) {
            case 0: v = img.r[p]; break;
            case 1: v = img.g[p]; break;
            default: v = img.b[p]; break;
        }
        if (bytesPer == 1) raster[i] = static_cast<uint8_t>(v);
        else {
            raster[i * 2] = static_cast<uint8_t>(v >> 8);
            raster[i * 2 + 1] = static_cast<uint8_t>(v & 0xff);
        }
    }
    const bool ok = std::fwrite(raster.data(), 1, raster.size(), f) == raster.size();
    std::fclose(f);
    if (!ok) err = "short write on '" + path + "'";
    return ok;
}

} // namespace anvil_lab
