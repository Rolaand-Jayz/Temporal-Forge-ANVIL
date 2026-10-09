// SampleGeometryEstimate.cpp — deterministic block subpixel registration.
//
// Image evidence only: integer SAD + per-axis parabolic subpixel fit over the
// luma planes, aggregated by median (see the header for the method, the
// sufficiency gate, and the derived sign convention). No codec motion
// vectors, no oracle data, no random or wall-clock inputs; identical inputs
// produce identical outputs.
#include "SampleGeometryEstimate.hpp"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <limits>
#include <vector>

namespace anvil {
namespace {

// Sentinel SAD: out-of-bounds or unreadable candidate blocks can never match.
constexpr int64_t kSadInvalid = std::numeric_limits<int64_t>::max();
constexpr int kSearchRadius = 4;   // integer search radius per the method
constexpr size_t kMinUsedTiles = 8;
constexpr double kMaxMadPx = 0.25; // per-axis agreement gate, pixels
constexpr size_t kMaxTiles = 1u << 20; // degenerate-input guard

// Luma sample at (x, y) or NaN when out of bounds / not representable.
// Mirrors Reconstruct.cpp's planeAt for plane 0 without touching it.
float lumaAt(const Observation& o, int x, int y) {
    if (x < 0 || y < 0 || x >= o.width || y >= o.height
        || o.linesize[0] <= 0)
        return std::numeric_limits<float>::quiet_NaN();
    const int bps = o.color.bitDepth > 8 ? 2 : 1;
    const size_t off = static_cast<size_t>(y) * o.linesize[0]
                     + static_cast<size_t>(x) * bps;
    if (off + static_cast<size_t>(bps) > o.plane[0].size())
        return std::numeric_limits<float>::quiet_NaN();
    if (bps == 1)
        return static_cast<float>(o.plane[0][off]);
    return static_cast<float>(uint32_t(o.plane[0][off])
                            | (uint32_t(o.plane[0][off + 1]) << 8));
}

// Sum of absolute luma differences of two blockSize-square blocks; kSadInvalid
// when either block leaves the frame or any sample is unreadable.
int64_t blockSad(const Observation& target, int tx, int ty,
                 const Observation& neighbor, int nx, int ny,
                 int blockSize) {
    if (nx < 0 || ny < 0 || nx + blockSize > neighbor.width
        || ny + blockSize > neighbor.height)
        return kSadInvalid;
    int64_t sad = 0;
    for (int y = 0; y < blockSize; ++y) {
        for (int x = 0; x < blockSize; ++x) {
            const float a = lumaAt(target, tx + x, ty + y);
            const float b = lumaAt(neighbor, nx + x, ny + y);
            if (!std::isfinite(a) || !std::isfinite(b))
                return kSadInvalid;
            sad += std::llabs(static_cast<int64_t>(a)
                            - static_cast<int64_t>(b));
        }
    }
    return sad;
}

// Median of a non-empty vector (even count = mean of the two central values).
double medianOf(std::vector<double> values) {
    std::sort(values.begin(), values.end());
    const size_t n = values.size();
    if (n % 2 == 1)
        return values[n / 2];
    return 0.5 * (values[n / 2 - 1] + values[n / 2]);
}

// Wrap v into [0, 1); maps -0.0 to 0.0 and clamps floating-point drift.
double fractionalPart(double v) {
    double f = v - std::floor(v);
    if (f >= 1.0)
        f -= 1.0;
    if (f < 0.0)
        f = 0.0;
    return f;
}

} // namespace

SampleGeometryEstimateResult estimateRelativeSampleGeometry(
    const Observation& target, const Observation& neighbor,
    int blockSize) {
    SampleGeometryEstimateResult result;

    // Fail closed on malformed inputs: no phase may be synthesized from a
    // frame we cannot read as a luma plane of the declared geometry.
    auto fail = [&](const char* reason) {
        result.geometry = SampleGeometry{}; // Unknown, phase 0
        result.insufficientReason = reason;
        return result;
    };
    if (blockSize <= 0)
        return fail("invalid blockSize");
    if (target.width <= 0 || target.height <= 0
        || neighbor.width != target.width
        || neighbor.height != target.height)
        return fail("observation geometry missing or mismatched");
    if (target.plane[0].empty() || neighbor.plane[0].empty()
        || target.linesize[0] <= 0 || neighbor.linesize[0] <= 0)
        return fail("luma plane empty or unusable linesize");
    if (blockSize > target.width || blockSize > target.height)
        return fail("blockSize exceeds frame; no fully-inside tiles");
    const size_t tilesX = static_cast<size_t>(target.width) / blockSize;
    const size_t tilesY = static_cast<size_t>(target.height) / blockSize;
    if (tilesX * tilesY > kMaxTiles)
        return fail("tile grid too large for this estimator");

    std::vector<double> fx, fy;
    for (int by = 0; by + blockSize <= target.height; by += blockSize) {
        for (int bx = 0; bx + blockSize <= target.width; bx += blockSize) {
            // Integer SAD minimum over +/-4. Scan order and strict
            // less-than keep the first minimum — deterministic, and the
            // same tie-break rule as estimateCorrespondence.
            int64_t bestSad = kSadInvalid;
            int bestDx = 0, bestDy = 0;
            for (int dy = -kSearchRadius; dy <= kSearchRadius; ++dy) {
                for (int dx = -kSearchRadius; dx <= kSearchRadius; ++dx) {
                    const int64_t sad = blockSad(target, bx, by, neighbor,
                                                 bx + dx, by + dy,
                                                 blockSize);
                    if (sad < bestSad) {
                        bestSad = sad;
                        bestDx = dx;
                        bestDy = dy;
                    }
                }
            }
            if (bestSad == kSadInvalid)
                continue; // no in-bounds candidate: tile not considered
            ++result.blocksConsidered;

            // Per-axis parabolic subpixel fit through SAD at (d-1, d, d+1).
            // A tile is used only when BOTH axes produce a valid fit;
            // anything else is reported as considered-but-not-used.
            double delta[2] = {0.0, 0.0};
            bool usable = true;
            for (int axis = 0; axis < 2 && usable; ++axis) {
                const int ox = axis == 0 ? 1 : 0;
                const int oy = axis == 0 ? 0 : 1;
                const int64_t sadL = blockSad(target, bx, by, neighbor,
                                              bx + bestDx - ox,
                                              by + bestDy - oy, blockSize);
                const int64_t sadR = blockSad(target, bx, by, neighbor,
                                              bx + bestDx + ox,
                                              by + bestDy + oy, blockSize);
                if (sadL == kSadInvalid || sadR == kSadInvalid) {
                    usable = false;
                    break;
                }
                const double den = static_cast<double>(sadL)
                                 - 2.0 * static_cast<double>(bestSad)
                                 + static_cast<double>(sadR);
                if (!(den > 0.0)) {
                    usable = false;
                    break;
                }
                const double d = 0.5
                    * (static_cast<double>(sadL) - static_cast<double>(sadR))
                    / den;
                if (!(std::fabs(d) <= 0.5)) {
                    usable = false;
                    break;
                }
                delta[axis] = d;
            }
            if (!usable)
                continue;
            ++result.blocksUsed;
            fx.push_back(static_cast<double>(bestDx) + delta[0]);
            fy.push_back(static_cast<double>(bestDy) + delta[1]);
        }
    }

    if (result.blocksUsed == 0)
        return fail("no tile produced a valid subpixel measurement");

    const double medFx = medianOf(fx);
    const double medFy = medianOf(fy);
    std::vector<double> absDevX, absDevY;
    absDevX.reserve(fx.size());
    absDevY.reserve(fy.size());
    for (double v : fx)
        absDevX.push_back(std::fabs(v - medFx));
    for (double v : fy)
        absDevY.push_back(std::fabs(v - medFy));
    const double madX = medianOf(absDevX);
    const double madY = medianOf(absDevY);
    result.madX = static_cast<float>(madX);
    result.madY = static_cast<float>(madY);

    if (result.blocksUsed < kMinUsedTiles) {
        result.insufficientReason =
            "insufficient usable tiles: " + std::to_string(result.blocksUsed)
            + " (need >= " + std::to_string(kMinUsedTiles) + ")";
        return result;
    }
    if (madX > kMaxMadPx || madY > kMaxMadPx) {
        result.insufficientReason = "subpixel estimates disagree: madX="
            + std::to_string(madX) + " madY=" + std::to_string(madY)
            + " (limit " + std::to_string(kMaxMadPx) + ")";
        return result;
    }

    // Neighbor phase relative to the target anchor (phase 0): the measured
    // displacement f locates the target's content in the neighbor at p + f,
    // so the neighbor's grid offset is -f, wrapped into [0,1). See the
    // header's sign-derivation note and test (d) for the end-to-end proof.
    result.geometry.state = SampleGeometryState::Estimated;
    result.geometry.phaseX = static_cast<float>(fractionalPart(-medFx));
    result.geometry.phaseY = static_cast<float>(fractionalPart(-medFy));
    return result;
}

} // namespace anvil
