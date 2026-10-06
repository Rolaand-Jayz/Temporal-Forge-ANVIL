// Reconstruct.cpp — deterministic estimation + confidence-weighted accumulate.
#include "Reconstruct.hpp"

#include <algorithm>
#include <cmath>
#include <cstring>

namespace anvil {

namespace {
// Bilinear blend of four corner samples; NaN propagates as "invalid".
inline float bilinearBlend(float p00, float p10, float p01, float p11,
                           float tx, float ty) {
    if (std::isnan(p00) || std::isnan(p10) || std::isnan(p01) || std::isnan(p11))
        return NAN;
    const float top = p00 + tx * (p10 - p00);
    const float bot = p01 + tx * (p11 - p01);
    return top + ty * (bot - top);
}
} // namespace

namespace {
inline float lumaAt(const Observation& o, int x, int y) {
    return static_cast<float>(o.plane[0][static_cast<size_t>(y) * o.linesize[0] + x]);
}

// Mean absolute difference of two blocks; large penalty when either block
// leaves the frame. Deterministic integer arithmetic.
uint32_t blockSad(const Observation& a, int ax, int ay, const Observation& b,
                  int bx, int by, int bw, int bh) {
    uint32_t sad = 0;
    for (int y = 0; y < bh; ++y) {
        const int ayi = ay + y, byi = by + y;
        if (ayi < 0 || ayi >= a.height || byi < 0 || byi >= b.height) return ~0u;
        for (int x = 0; x < bw; ++x) {
            const int axi = ax + x, bxi = bx + x;
            if (axi < 0 || axi >= a.width || bxi < 0 || bxi >= b.width) return ~0u;
            const int d = int(a.plane[0][size_t(ayi) * a.linesize[0] + axi])
                        - int(b.plane[0][size_t(byi) * b.linesize[0] + bxi]);
            sad += uint32_t(d < 0 ? -d : d);
        }
    }
    return sad;
}
} // namespace

std::vector<BlockMotion> estimateCorrespondence(const Observation& target,
                                                const Observation& obs,
                                                int blockSize, int searchRadius) {
    std::vector<BlockMotion> out;
    if (target.plane[0].empty() || obs.plane[0].empty()) return out;
    const int bw = blockSize;
    for (int by = 0; by + bw <= target.height; by += bw) {
        for (int bx = 0; bx + bw <= target.width; bx += bw) {
            uint32_t bestSad = ~0u;
            int bestDx = 0, bestDy = 0;
            for (int dy = -searchRadius; dy <= searchRadius; ++dy) {
                for (int dx = -searchRadius; dx <= searchRadius; ++dx) {
                    const uint32_t sad =
                        blockSad(target, bx, by, obs, bx + dx, by + dy, bw, bw);
                    if (sad < bestSad) {
                        bestSad = sad;
                        bestDx = dx;
                        bestDy = dy;
                    }
                }
            }
            if (bestSad == ~0u) continue; // block unreachable in obs
            BlockMotion m;
            m.frameIndex = target.frameIndex;
            m.refFrameIndex = static_cast<int64_t>(obs.frameIndex);
            m.temporalDistance = static_cast<int>(obs.frameIndex)
                               - static_cast<int>(target.frameIndex);
            m.direction = m.temporalDistance < 0 ? RefDirection::Past
                                                 : RefDirection::Future;
            m.dstX = static_cast<int16_t>(bx);
            m.dstY = static_cast<int16_t>(by);
            m.blockW = static_cast<uint16_t>(bw);
            m.blockH = static_cast<uint16_t>(bw);
            m.mvX = static_cast<float>(bestDx);
            m.mvY = static_cast<float>(bestDy);
            m.precision = MotionPrecision::Integer;
            m.source = CorrespondenceSource::ImageEstimate;
            m.ambiguous = false;
            out.push_back(m);
        }
    }
    return out;
}

FlowField buildFlowField(int width, int height, const std::vector<BlockMotion>& blocks,
                         int64_t refFrameIndex) {
    FlowField flow(static_cast<size_t>(width) * height * 2, 0.0f);
    for (const BlockMotion& b : blocks) {
        // Reference identity must be proven for the vector to enter
        // reconstruction. Ambiguous codec vectors are rejected here.
        if (b.ambiguous || b.refFrameIndex != refFrameIndex) continue;
        for (int y = b.dstY; y < b.dstY + b.blockH && y < height; ++y) {
            if (y < 0) continue;
            for (int x = b.dstX; x < b.dstX + b.blockW && x < width; ++x) {
                if (x < 0) continue;
                const size_t i = (static_cast<size_t>(y) * width + x) * 2;
                flow[i] = b.mvX;
                flow[i + 1] = b.mvY;
            }
        }
    }
    return flow;
}

namespace {
inline float bilinear(const Observation& o, float fx, float fy) {
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const float tx = fx - x0, ty = fy - y0;
    const int x1 = x0 + 1, y1 = y0 + 1;
    auto inside = [&](int x, int y) { return x >= 0 && x < o.width && y >= 0 && y < o.height; };
    if (!inside(x0, y0)) return NAN;
    auto at = [&](int x, int y) -> float {
        if (!inside(x, y)) return NAN;
        return lumaAt(o, x, y);
    };
    const float p00 = at(x0, y0), p10 = at(x1, y0), p01 = at(x0, y1), p11 = at(x1, y1);
    const float top = p00 + tx * (p10 - p00);
    const float bot = p01 + tx * (p11 - p01);
    return top + ty * (bot - top);
}
} // namespace

AccumulateResult accumulate(const Observation& target,
                            const std::vector<Observation>& neighbors,
                            const std::vector<FlowField>& neighborFlows,
                            const std::vector<std::vector<Visibility>>& neighborVisibility) {
    AccumulateResult res;
    res.frame = target;
    if (neighbors.empty()) return res;

    const int w = target.width, h = target.height;
    std::vector<float> accY(static_cast<size_t>(w) * h, 0.0f);
    std::vector<float> accW(static_cast<size_t>(w) * h, 0.0f);
    // U/V planes at half resolution for 4:2:0; other formats handled per plane.
    const int cw = target.planeCount > 2 ? (target.avPixelFormat == AV_PIX_FMT_YUV420P ? w / 2 : w) : 0;
    const int ch = target.planeCount > 2 ? (target.avPixelFormat == AV_PIX_FMT_YUV420P ? h / 2 : h) : 0;
    std::vector<float> accU, accV, accCU, accCV;
    if (cw > 0 && ch > 0) {
        accU.assign(static_cast<size_t>(w) * h, 0.0f);
        accV.assign(static_cast<size_t>(w) * h, 0.0f);
        accCU.assign(static_cast<size_t>(cw) * ch, 0.0f);
        accCV.assign(static_cast<size_t>(cw) * ch, 0.0f);
    }

    for (size_t n = 0; n < neighbors.size(); ++n) {
        const Observation& o = neighbors[n];
        const bool isTarget = o.frameIndex == target.frameIndex;
        const FlowField empty;
        const FlowField& flow = isTarget ? empty : neighborFlows[n];
        const std::vector<Visibility> noVis;
        const std::vector<Visibility>& vis =
            neighborVisibility.empty() ? noVis : neighborVisibility[n];
        // Backend-neutral confidence: known = 1.0 for all window members.
        // (Confidence estimation is a later-campaign hypothesis; the
        // build-ready path uses a fixed known weight and explicit visibility.)
        const float weight = 1.0f;
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const size_t i = static_cast<size_t>(y) * w + x;
                if (!vis.empty() && vis[i] == Visibility::Invalid) continue;
                if (!isTarget) ++res.totalSamples;
                float sample;
                if (isTarget) {
                    sample = lumaAt(o, x, y);
                } else {
                    const size_t f = i * 2;
                    sample = bilinear(o, x + flow[f], y + flow[f + 1]);
                }
                if (std::isnan(sample)) continue;
                accY[i] += weight * sample;
                accW[i] += weight;
                if (!isTarget) ++res.validSamples;
            }
        }
        // chroma (YUV420P): flow is halved; target chroma copied directly.
        if (cw > 0 && ch > 0 && target.avPixelFormat == AV_PIX_FMT_YUV420P) {
            for (int cy = 0; cy < ch; ++cy) {
                for (int cx = 0; cx < cw; ++cx) {
                    const size_t ci = static_cast<size_t>(cy) * cw + cx;
                    const size_t fullY = static_cast<size_t>(cy * 2) * w + cx * 2;
                    if (!vis.empty() && vis[fullY] == Visibility::Invalid) continue;
                    const float fx = static_cast<float>(cx * 2)
                        + (isTarget ? 0.0f : flow[fullY * 2] * 0.5f);
                    const float fy = static_cast<float>(cy * 2)
                        + (isTarget ? 0.0f : flow[fullY * 2 + 1] * 0.5f);
                    const int c0x = static_cast<int>(std::floor(fx));
                    const int c0y = static_cast<int>(std::floor(fy));
                    auto cAt = [&](int plane, int x, int y) -> float {
                        if (x < 0 || y < 0 || x >= cw || y >= ch) return NAN;
                        return static_cast<float>(o.plane[plane][static_cast<size_t>(y) * o.linesize[plane] + x]);
                    };
                    const float u = bilinearBlend(cAt(1, c0x, c0y), cAt(1, c0x + 1, c0y),
                                                  cAt(1, c0x, c0y + 1), cAt(1, c0x + 1, c0y + 1),
                                                  fx - c0x, fy - c0y);
                    const float v = bilinearBlend(cAt(2, c0x, c0y), cAt(2, c0x + 1, c0y),
                                                  cAt(2, c0x, c0y + 1), cAt(2, c0x + 1, c0y + 1),
                                                  fx - c0x, fy - c0y);
                    if (!std::isnan(u)) {
                        accCU[ci] += weight * u;
                        if (!std::isnan(v)) accCV[ci] += weight * v;
                    }
                }
            }
        }
    }

    // Write averaged planes back; pixels with zero weight keep target values
    // (recorded via valid/total ratio).
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const size_t i = static_cast<size_t>(y) * w + x;
            if (accW[i] > 0.0f) {
                const float v = accY[i] / accW[i];
                res.frame.plane[0][static_cast<size_t>(y) * res.frame.linesize[0] + x] =
                    static_cast<uint8_t>(std::lround(std::clamp(v, 0.0f, 255.0f)));
            }
        }
    }
    if (cw > 0 && ch > 0 && target.avPixelFormat == AV_PIX_FMT_YUV420P) {
        for (int plane = 1; plane <= 2; ++plane) {
            const auto& acc = plane == 1 ? accCU : accCV;
            for (int cy = 0; cy < ch; ++cy) {
                for (int cx = 0; cx < cw; ++cx) {
                    const size_t ci = static_cast<size_t>(cy) * cw + cx;
                    const size_t fullY = static_cast<size_t>(cy * 2) * w + cx * 2;
                    if (accW[fullY] > 0.0f) {
                        const float v = acc[ci] / accW[fullY];
                        res.frame.plane[plane][static_cast<size_t>(cy) * res.frame.linesize[plane] + cx] =
                            static_cast<uint8_t>(std::lround(std::clamp(v, 0.0f, 255.0f)));
                    }
                }
            }
        }
    }
    return res;
}

} // namespace anvil
