// Oracle.cpp — oracle file loading with strict validation.
#include "Oracle.hpp"

#include <charconv>
#include <cstdio>

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

std::optional<std::vector<BlockMotion>> loadOracleCorrespondence(
    const std::string& dir, uint64_t frameIndex) {
    FILE* f = std::fopen(pathFor(dir, "correspondence", frameIndex, ".txt").c_str(), "r");
    if (!f) return std::nullopt;
    std::vector<BlockMotion> blocks;
    char line[512];
    uint64_t lineNo = 0;
    bool ok = true;
    while (std::fgets(line, sizeof(line), f)) {
        ++lineNo;
        std::string_view sv(line);
        while (!sv.empty() && (sv.back() == '\n' || sv.back() == '\r')) sv.remove_suffix(1);
        if (sv.empty() || sv[0] == '#') continue;
        const auto fields = splitFields(sv);
        if (fields.size() != 13) { ok = false; break; }
        BlockMotion b;
        b.frameIndex = frameIndex;
        b.source = CorrespondenceSource::Oracle;
        bool lineOk = parseNum(fields[0], b.refFrameIndex);
        int dirInt = 0, precInt = 0, intraInt = 0, skipInt = 0, dist = 0;
        int dstX = 0, dstY = 0, bw = 0, bh = 0;
        lineOk = lineOk && parseNum(fields[1], dirInt) && parseNum(fields[2], dist)
            && parseNum(fields[3], dstX) && parseNum(fields[4], dstY)
            && parseNum(fields[5], bw) && parseNum(fields[6], bh)
            && parseNum(fields[7], b.mvX) && parseNum(fields[8], b.mvY)
            && parseNum(fields[9], precInt) && !fields[10].empty()
            && parseNum(fields[11], intraInt) && parseNum(fields[12], skipInt);
        b.frameType = fields[10].empty() ? '?' : fields[10][0];
        if (!lineOk || dirInt < 0 || dirInt > 1 || precInt < 0 || precInt > 4
            || intraInt < 0 || intraInt > 1 || skipInt < 0 || skipInt > 1) {
            ok = false;
            break;
        }
        b.direction = dirInt == 0 ? RefDirection::Past : RefDirection::Future;
        b.temporalDistance = dist;
        b.dstX = static_cast<int16_t>(dstX);
        b.dstY = static_cast<int16_t>(dstY);
        b.blockW = static_cast<uint16_t>(bw);
        b.blockH = static_cast<uint16_t>(bh);
        b.precision = static_cast<MotionPrecision>(precInt);
        b.intra = intraInt != 0;
        b.skip = skipInt != 0;
        b.ambiguous = b.refFrameIndex < 0;
        blocks.push_back(b);
    }
    std::fclose(f);
    if (!ok) return std::nullopt;
    return blocks;
}

std::optional<std::vector<Visibility>> loadOracleVisibility(
    const std::string& dir, uint64_t frameIndex, int expectedW, int expectedH) {
    int w = 0, h = 0;
    std::vector<uint8_t> pix;
    if (!readPgm(pathFor(dir, "visibility", frameIndex, ".pgm"), w, h, pix))
        return std::nullopt;
    if (w != expectedW || h != expectedH) return std::nullopt;
    std::vector<Visibility> vis(pix.size());
    for (size_t i = 0; i < pix.size(); ++i) {
        vis[i] = pix[i] == 0   ? Visibility::Invalid
               : pix[i] == 255 ? Visibility::Valid
                               : Visibility::Unknown;
    }
    return vis;
}

std::optional<SampleGeometry> loadOracleGeometry(const std::string& dir,
                                                 uint64_t frameIndex) {
    FILE* f = std::fopen(pathFor(dir, "geometry", frameIndex, ".txt").c_str(), "r");
    if (!f) return std::nullopt;
    int state = -1;
    float px = 0, py = 0;
    const int n = std::fscanf(f, "%d %f %f", &state, &px, &py);
    std::fclose(f);
    if (n != 3 || state < 0 || state > 2) return std::nullopt;
    SampleGeometry g;
    g.state = static_cast<SampleGeometryState>(state);
    g.phaseX = px;
    g.phaseY = py;
    return g;
}

} // namespace anvil
