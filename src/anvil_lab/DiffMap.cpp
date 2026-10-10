// DiffMap.cpp — abs-diff, heatmap render, connected change regions.
#include "DiffMap.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <queue>

namespace anvil_lab {

bool computeAbsDiff(const Image& a, const Image& b, AbsDiff& out, std::string& err) {
    if (a.width != b.width || a.height != b.height || a.maxval != b.maxval
        || a.isGray() != b.isGray()) {
        err = "computeAbsDiff requires matched geometry/maxval/representation";
        return false;
    }
    out.width = a.width;
    out.height = a.height;
    out.maxval = a.maxval;
    const size_t n = a.pixelCount();
    out.luma.assign(n, 0);
    if (a.isGray()) {
        for (size_t i = 0; i < n; ++i)
            out.luma[i] = static_cast<uint16_t>(std::abs(int(a.r[i]) - int(b.r[i])));
        return true;
    }
    out.r.assign(n, 0);
    out.g.assign(n, 0);
    out.b.assign(n, 0);
    for (size_t i = 0; i < n; ++i) {
        out.r[i] = static_cast<uint16_t>(std::abs(int(a.r[i]) - int(b.r[i])));
        out.g[i] = static_cast<uint16_t>(std::abs(int(a.g[i]) - int(b.g[i])));
        out.b[i] = static_cast<uint16_t>(std::abs(int(a.b[i]) - int(b.b[i])));
        // Luma difference proxy for region analysis: max channel difference
        // keeps single-channel artifacts visible.
        out.luma[i] = std::max(out.r[i], std::max(out.g[i], out.b[i]));
    }
    return true;
}

DiffStats diffStats(const AbsDiff& d, int hotThreshold) {
    DiffStats s;
    double sum = 0.0;
    for (size_t i = 0; i < d.luma.size(); ++i) {
        const double v = d.luma[i];
        sum += v;
        s.maxAbs = std::max(s.maxAbs, v);
        if (v >= 1) ++s.nonzeroPixels;
        if (v >= static_cast<double>(hotThreshold)) ++s.hotPixels;
    }
    const double n = static_cast<double>(d.luma.size());
    s.meanAbs = n ? sum / n : 0.0;
    s.hotRatio = n ? static_cast<double>(s.hotPixels) / n : 0.0;
    return s;
}

void heatmapColor(double normalized, uint8_t& rr, uint8_t& gg, uint8_t& bb) {
    normalized = std::clamp(normalized, 0.0, 1.0);
    // Black -> ANVIL green (0x37d17a) -> yellow -> red, piecewise linear.
    struct Stop { double t; double r, g, b; };
    static const Stop stops[] = {
        {0.00, 0x08, 0x0d, 0x0b},
        {0.25, 0x37, 0xd1, 0x7a},
        {0.60, 0xf5, 0xd1, 0x3b},
        {1.00, 0xff, 0x3d, 0x2f},
    };
    for (int i = 0; i < 3; ++i) {
        if (normalized <= stops[i + 1].t || i == 2) {
            const double span = stops[i + 1].t - stops[i].t;
            const double f = span > 0 ? (normalized - stops[i].t) / span : 0.0;
            rr = static_cast<uint8_t>(std::lround(stops[i].r + f * (stops[i + 1].r - stops[i].r)));
            gg = static_cast<uint8_t>(std::lround(stops[i].g + f * (stops[i + 1].g - stops[i].g)));
            bb = static_cast<uint8_t>(std::lround(stops[i].b + f * (stops[i + 1].b - stops[i].b)));
            return;
        }
    }
}

std::vector<uint8_t> renderHeatmap(const AbsDiff& d, double gain) {
    std::vector<uint8_t> rgb(static_cast<size_t>(d.width) * d.height * 3);
    const double norm = d.maxval > 0 ? static_cast<double>(d.maxval) : 255.0;
    for (size_t i = 0; i < d.luma.size(); ++i) {
        const double v = std::clamp(d.luma[i] * gain / norm, 0.0, 1.0);
        heatmapColor(v, rgb[i * 3], rgb[i * 3 + 1], rgb[i * 3 + 2]);
    }
    return rgb;
}

namespace {

struct Comp {
    int x0 = INT32_MAX, y0 = INT32_MAX, x1 = -1, y1 = -1;
    uint64_t pixels = 0;
    double sumDiff = 0.0;
    double maxDiff = 0.0;
    double sumX = 0.0, sumY = 0.0;
};

bool boxesNear(const Comp& a, const Comp& b, int mergePx) {
    const bool separatedX = b.x0 - a.x1 > mergePx || a.x0 - b.x1 > mergePx;
    const bool separatedY = b.y0 - a.y1 > mergePx || a.y0 - b.y1 > mergePx;
    return !(separatedX || separatedY); // touching/overlapping after slack
}

} // namespace

std::vector<DiffRegion> differenceRegions(const AbsDiff& d, int threshold,
                                          uint64_t minPixels, int mergePx) {
    const int w = d.width, h = d.height;
    if (w <= 0 || h <= 0) return {};
    std::vector<int32_t> label(static_cast<size_t>(w) * h, -1);
    std::vector<Comp> comps;
    const double thr = static_cast<double>(threshold);
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x) {
            const size_t i = static_cast<size_t>(y) * w + x;
            if (label[i] != -1 || d.luma[i] < thr) continue;
            const int32_t id = static_cast<int32_t>(comps.size());
            comps.emplace_back();
            std::queue<std::pair<int, int>> q;
            q.emplace(x, y);
            label[i] = id;
            while (!q.empty()) {
                const auto [cx, cy] = q.front();
                q.pop();
                const size_t ci = static_cast<size_t>(cy) * w + cx;
                Comp& c = comps[static_cast<size_t>(id)];
                c.x0 = std::min(c.x0, cx);
                c.y0 = std::min(c.y0, cy);
                c.x1 = std::max(c.x1, cx);
                c.y1 = std::max(c.y1, cy);
                c.pixels += 1;
                c.sumDiff += d.luma[ci];
                c.maxDiff = std::max(c.maxDiff, static_cast<double>(d.luma[ci]));
                c.sumX += cx;
                c.sumY += cy;
                static const int kDx[4] = {1, -1, 0, 0};
                static const int kDy[4] = {0, 0, 1, -1};
                for (int k = 0; k < 4; ++k) {
                    const int nx = cx + kDx[k], ny = cy + kDy[k];
                    if (nx < 0 || ny < 0 || nx >= w || ny >= h) continue;
                    const size_t ni = static_cast<size_t>(ny) * w + nx;
                    if (label[ni] != -1 || d.luma[ni] < thr) continue;
                    label[ni] = id;
                    q.emplace(nx, ny);
                }
            }
        }

    // Iterative merge of boxes within mergePx until stable.
    bool merged = true;
    std::vector<bool> dead(comps.size(), false);
    while (merged) {
        merged = false;
        for (size_t i = 0; i < comps.size() && !merged; ++i) {
            if (dead[i]) continue;
            for (size_t j = i + 1; j < comps.size() && !merged; ++j) {
                if (dead[j]) continue;
                if (!boxesNear(comps[i], comps[j], mergePx)) continue;
                Comp& a = comps[i];
                const Comp& b = comps[j];
                a.x0 = std::min(a.x0, b.x0);
                a.y0 = std::min(a.y0, b.y0);
                a.x1 = std::max(a.x1, b.x1);
                a.y1 = std::max(a.y1, b.y1);
                a.pixels += b.pixels;
                a.sumDiff += b.sumDiff;
                a.maxDiff = std::max(a.maxDiff, b.maxDiff);
                a.sumX += b.sumX;
                a.sumY += b.sumY;
                dead[j] = true;
                merged = true;
            }
        }
    }

    std::vector<DiffRegion> out;
    for (size_t i = 0; i < comps.size(); ++i) {
        if (dead[i] || comps[i].pixels < minPixels) continue;
        DiffRegion r;
        r.id = static_cast<int>(out.size()) + 1;
        r.x0 = comps[i].x0;
        r.y0 = comps[i].y0;
        r.x1 = comps[i].x1;
        r.y1 = comps[i].y1;
        r.pixels = comps[i].pixels;
        r.cx = comps[i].sumX / static_cast<double>(comps[i].pixels);
        r.cy = comps[i].sumY / static_cast<double>(comps[i].pixels);
        r.meanAbs = comps[i].sumDiff / static_cast<double>(comps[i].pixels);
        r.maxAbs = comps[i].maxDiff;
        out.push_back(r);
    }
    std::stable_sort(out.begin(), out.end(),
                     [](const DiffRegion& a, const DiffRegion& b) {
                         return a.pixels > b.pixels;
                     });
    for (size_t i = 0; i < out.size(); ++i)
        out[i].id = static_cast<int>(i) + 1;
    return out;
}

} // namespace anvil_lab
