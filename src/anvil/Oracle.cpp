// Oracle.cpp — oracle file loading with strict validation.
#include "Oracle.hpp"

#include <charconv>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <limits>

#include "Pnm.hpp"

namespace anvil {

namespace {
std::string pathFor(const std::string& dir, const char* stem, uint64_t frame,
                    const char* ext) {
    return dir + "/" + stem + "_" + std::to_string(frame) + ext;
}

std::vector<std::string_view> splitFields(std::string_view line) {
    std::vector<std::string_view> out;
    size_t i = 0;
    while (i < line.size()) {
        while (i < line.size() && (line[i] == ' ' || line[i] == '\t')) ++i;
        const size_t start = i;
        while (i < line.size() && line[i] != ' ' && line[i] != '\t') ++i;
        if (i > start) out.push_back(line.substr(start, i - start));
    }
    return out;
}

template <typename T>
bool parseNum(std::string_view s, T& out) {
    const char* begin = s.data();
    const char* end = s.data() + s.size();
    auto [p, ec] = std::from_chars(begin, end, out);
    return ec == std::errc{} && p == end;
}
} // namespace

bool oracleFileExists(const std::string& dir, uint64_t frameIndex) {
    FILE* f = std::fopen(pathFor(dir, "correspondence", frameIndex, ".txt").c_str(), "r");
    if (f) std::fclose(f);
    return f != nullptr;
}

namespace {
// Strict integer parse: full token must consume, no overflow.
bool parseIntStrict(std::string_view tok, long long& out) {
    if (tok.empty()) return false;
    const char* b = tok.data();
    const char* e = tok.data() + tok.size();
    auto [p, ec] = std::from_chars(b, e, out);
    return ec == std::errc{} && p == e;
}

bool parseMotion(std::string_view tok, float& out) {
    if (tok.empty()) return false;
    double v = 0;
    const char* b = tok.data();
    const char* e = tok.data() + tok.size();
    auto [p, ec] = std::from_chars(b, e, v);
    if (ec != std::errc{} || p != e) return false;
    if (!std::isfinite(v)) return false;           // nan/inf are not truth
    const float f = static_cast<float>(v);
    if (!std::isfinite(f)) return false;           // unrepresentable in float
    out = f;
    return true;
}
} // namespace

std::optional<std::vector<BlockMotion>> loadOracleCorrespondence(
    const std::string& dir, uint64_t frameIndex, std::string* error) {
    const std::string path = pathFor(dir, "correspondence", frameIndex, ".txt");
    FILE* f = std::fopen(path.c_str(), "r");
    if (!f) {
        if (error) *error = "cannot open " + path;
        return std::nullopt;
    }
    std::vector<BlockMotion> blocks;
    char line[512];
    uint64_t lineNo = 0;
    bool ok = true;
    std::string reason;
    while (std::fgets(line, sizeof(line), f)) {
        ++lineNo;
        std::string_view sv(line);
        while (!sv.empty() && (sv.back() == '\n' || sv.back() == '\r')) sv.remove_suffix(1);
        if (sv.empty() || sv[0] == '#') continue;
        const auto fields = splitFields(sv);
        if (fields.size() != 13) {
            reason = "expected 13 fields, got " + std::to_string(fields.size());
            ok = false;
            break;
        }
        // Strict integer parsing into wide types BEFORE any narrowing.
        long long ref = 0, dirInt = 0, dist = 0, dstX = 0, dstY = 0;
        long long bw = 0, bh = 0, precInt = 0, intraInt = 0, skipInt = 0;
        float mvX = 0, mvY = 0;
        bool lineOk = parseIntStrict(fields[0], ref)
            && parseIntStrict(fields[1], dirInt) && parseIntStrict(fields[2], dist)
            && parseIntStrict(fields[3], dstX) && parseIntStrict(fields[4], dstY)
            && parseIntStrict(fields[5], bw) && parseIntStrict(fields[6], bh)
            && parseMotion(fields[7], mvX) && parseMotion(fields[8], mvY)
            && parseIntStrict(fields[9], precInt) && fields[10].size() == 1
            && parseIntStrict(fields[11], intraInt)
            && parseIntStrict(fields[12], skipInt);
        if (!lineOk) {
            reason = "malformed numeric field or non-finite motion";
            ok = false;
            break;
        }
        // Range/semantic invariants — checked before any narrowing cast.
        if (ref < -1) { reason = "refFrameIndex < -1"; ok = false; break; }
        if (dirInt < 0 || dirInt > 1) { reason = "direction must be 0 or 1"; ok = false; break; }
        if (dist < std::numeric_limits<int>::min()
            || dist > std::numeric_limits<int>::max()) {
            reason = "temporalDistance out of int range";
            ok = false;
            break;
        }
        if (ref >= 0) {
            if (frameIndex > static_cast<uint64_t>(std::numeric_limits<int64_t>::max())) {
                reason = "target frame index not representable as int64";
                ok = false;
                break;
            }
            const int64_t target = static_cast<int64_t>(frameIndex);
            if (ref == target) {
                reason = "reference frame must differ from target frame";
                ok = false;
                break;
            }
            const __int128 expected128 = static_cast<__int128>(ref)
                                       - static_cast<__int128>(target);
            if (expected128 < std::numeric_limits<int>::min()
                || expected128 > std::numeric_limits<int>::max()) {
                reason = "reference-target distance out of int range";
                ok = false;
                break;
            }
            const int expected = static_cast<int>(expected128);
            if (dist != expected) {
                reason = "temporalDistance disagrees with refFrameIndex-targetFrameIndex";
                ok = false;
                break;
            }
            if ((expected < 0 && dirInt != 0) || (expected > 0 && dirInt != 1)) {
                reason = "direction disagrees with known reference";
                ok = false;
                break;
            }
        } else if (dist != 0) {
            if (dirInt == 0 && dist > 0) { reason = "past reference with positive temporalDistance"; ok = false; break; }
            if (dirInt == 1 && dist < 0) { reason = "future reference with negative temporalDistance"; ok = false; break; }
        }
        if (dstX < 0 || dstX > 32767 || dstY < 0 || dstY > 32767) {
            reason = "block origin out of representable range";
            ok = false;
            break;
        }
        if (bw < 1 || bw > 65535 || bh < 1 || bh > 65535) {
            reason = "block extent must be in [1, 65535]";
            ok = false;
            break;
        }
        if (precInt < 0 || precInt > 4) { reason = "precision out of range"; ok = false; break; }
        const char frameType = fields[10][0];
        if (frameType != 'I' && frameType != 'P' && frameType != 'B'
            && frameType != '?') {
            reason = "frameType must be one of I/P/B/?";
            ok = false;
            break;
        }
        if (intraInt < 0 || intraInt > 1 || skipInt < 0 || skipInt > 1) {
            reason = "intra/skip must be 0 or 1";
            ok = false;
            break;
        }
        BlockMotion b;
        b.frameIndex = frameIndex;
        b.source = CorrespondenceSource::Oracle;
        b.refFrameIndex = ref;
        b.direction = dirInt == 0 ? RefDirection::Past : RefDirection::Future;
        b.temporalDistance = static_cast<int>(dist);
        b.dstX = static_cast<int16_t>(dstX);
        b.dstY = static_cast<int16_t>(dstY);
        b.blockW = static_cast<uint16_t>(bw);
        b.blockH = static_cast<uint16_t>(bh);
        b.mvX = mvX;
        b.mvY = mvY;
        b.precision = static_cast<MotionPrecision>(precInt);
        b.frameType = fields[10][0];
        b.intra = intraInt != 0;
        b.skip = skipInt != 0;
        b.ambiguous = b.refFrameIndex < 0;
        blocks.push_back(b);
    }
    std::fclose(f);
    if (!ok) {
        if (error) *error = path + " line " + std::to_string(lineNo) + ": " + reason;
        return std::nullopt;
    }
    return blocks;
}

std::optional<std::vector<Visibility>> loadOracleVisibility(
    const std::string& dir, uint64_t frameIndex, int64_t refFrameIndex,
    int expectedW, int expectedH, std::string* error) {
    // Per-neighbor key: visibility_<target>_ref<reference>.pgm. A mask is
    // never shared silently across temporal neighbors.
    std::string path = dir + "/visibility_" + std::to_string(frameIndex)
        + "_ref" + std::to_string(refFrameIndex) + ".pgm";
    int w = 0, h = 0, maxval = 0;
    std::vector<uint8_t> pix;
    if (!readPgm(path, w, h, pix, &maxval)) {
        if (error) *error = "cannot read strict per-neighbor visibility oracle " + path;
        return std::nullopt;
    }
    if (maxval != 255) {
        if (error) *error = path + ": visibility oracle maxval must be 255";
        return std::nullopt;
    }
    if (w != expectedW || h != expectedH) {
        if (error) *error = path + ": dimensions " + std::to_string(w) + "x"
            + std::to_string(h) + " do not match frame "
            + std::to_string(expectedW) + "x" + std::to_string(expectedH);
        return std::nullopt;
    }
    std::vector<Visibility> vis(pix.size());
    for (size_t i = 0; i < pix.size(); ++i) {
        if (pix[i] == 0) vis[i] = Visibility::Invalid;
        else if (pix[i] == 128) vis[i] = Visibility::Unknown;
        else if (pix[i] == 255) vis[i] = Visibility::Valid;
        else {
            if (error) *error = path + ": noncanonical visibility value "
                + std::to_string(pix[i]) + " at sample " + std::to_string(i)
                + " (allowed: 0,128,255)";
            return std::nullopt;
        }
    }
    return vis;
}

std::optional<std::vector<float>> loadOracleConfidence(
    const std::string& dir, uint64_t frameIndex, int64_t refFrameIndex,
    int expectedW, int expectedH, std::string* error) {
    const std::string path = dir + "/confidence_" + std::to_string(frameIndex)
        + "_ref" + std::to_string(refFrameIndex) + ".pgm";
    int w = 0, h = 0, maxval = 0;
    std::vector<uint8_t> pix;
    if (!readPgm(path, w, h, pix, &maxval)) {
        if (error) *error = "cannot read strict per-neighbor confidence oracle " + path;
        return std::nullopt;
    }
    if (maxval != 255) {
        if (error) *error = path + ": confidence oracle maxval must be 255";
        return std::nullopt;
    }
    if (w != expectedW || h != expectedH) {
        if (error) *error = path + ": dimensions " + std::to_string(w) + "x"
            + std::to_string(h) + " do not match frame "
            + std::to_string(expectedW) + "x" + std::to_string(expectedH);
        return std::nullopt;
    }
    std::vector<float> confidence(pix.size(), 0.0f);
    for (size_t i = 0; i < pix.size(); ++i)
        confidence[i] = static_cast<float>(pix[i]) / 255.0f;
    return confidence;
}

std::optional<SampleGeometry> loadOracleGeometry(const std::string& dir,
                                                 uint64_t frameIndex,
                                                 std::string* error) {
    const std::string path = pathFor(dir, "geometry", frameIndex, ".txt");
    std::ifstream f(path);
    if (!f) {
        if (error) *error = "cannot open " + path;
        return std::nullopt;
    }
    std::string line;
    bool haveData = false;
    SampleGeometry g;
    uint64_t lineNo = 0;
    while (std::getline(f, line)) {
        ++lineNo;
        std::string_view sv(line);
        if (sv.empty() || sv[0] == '#') continue;
        if (haveData) {
            if (error) *error = path + " line " + std::to_string(lineNo)
                + ": unexpected trailing data";
            return std::nullopt;
        }
        const auto fields = splitFields(sv);
        if (fields.size() != 3) {
            if (error) *error = path + " line " + std::to_string(lineNo)
                + ": expected exactly 3 fields";
            return std::nullopt;
        }
        long long state = -1;
        float px = 0.0f, py = 0.0f;
        if (!parseIntStrict(fields[0], state) || !parseMotion(fields[1], px)
            || !parseMotion(fields[2], py)) {
            if (error) *error = path + " line " + std::to_string(lineNo)
                + ": malformed or non-finite geometry value";
            return std::nullopt;
        }
        if (state < 0 || state > 2) {
            if (error) *error = path + " line " + std::to_string(lineNo)
                + ": geometry state must be 0,1,2";
            return std::nullopt;
        }
        if (px < 0.0f || px >= 1.0f || py < 0.0f || py >= 1.0f) {
            if (error) *error = path + " line " + std::to_string(lineNo)
                + ": phase must be finite and in [0,1)";
            return std::nullopt;
        }
        g.state = static_cast<SampleGeometryState>(state);
        g.phaseX = px;
        g.phaseY = py;
        haveData = true;
    }
    if (!haveData) {
        if (error) *error = path + ": no geometry record";
        return std::nullopt;
    }
    return g;
}

} // namespace anvil
