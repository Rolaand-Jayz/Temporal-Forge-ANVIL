// anvil_geometry_estimate_tests.cpp — sample-geometry estimation contract.
//
// The fixture knows the truth; the estimator sees only images. The analytic
// pattern is a deterministic smooth textured ramp field along x (saturating
// exponential ramps per 8 px period with a partial smoothstep reset) plus a
// sinusoid along y, rendered into 16-bit little-endian luma planes. The
// neighbor is the same analytic function sampled at a +0.25 px x grid
// offset: N(x, y) = A(x + 0.25, y), i.e. the neighbor's sampling grid sits
// +0.25 px off the target's, so the target-relative neighbor phase is
// expected at frac(-(-0.25)) = 0.25 per the sign derivation in
// SampleGeometryEstimate.hpp (content displacement f = -0.25).
#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "anvil/Core.hpp"
#include "anvil/Reconstruct.hpp"
#include "anvil/SampleGeometryEstimate.hpp"

using namespace anvil;

static int failures = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

namespace {

constexpr int kW = 96;          // >= 64 per the brief; 16 px border tiles are
constexpr int kH = 96;          // skipped by the parabolic stencil (fail closed)
constexpr int kBlock = 16;
constexpr double kHamp = 30000.0;
constexpr double kBase = 3000.0;
constexpr double kMyAmp = 6000.0;
constexpr double kLy = 16.0;
constexpr double kTau = 0.6;
constexpr double kPeriod = 8.0;
constexpr double kCutStart = 7.0;
constexpr double kCutWidth = 3.0;

// Analytic ramp+sinusoid pattern; dx shifts the sampling grid (the neighbor
// is rendered with dx = -0.25 so that N(x,y) = A(x + 0.25, y)).
double analytic(double x, double y) {
    double u = std::fmod(x, kPeriod);
    if (u < 0.0)
        u += kPeriod;
    u = kPeriod - u; // mirrored ramp orientation (see fixture selection notes)
    const double z = std::clamp((u - kCutStart) / kCutWidth, 0.0, 1.0);
    const double s = z * z * (3.0 - 2.0 * z);
    const double ramp = kHamp * (1.0 - std::exp(-u / kTau)) * (1.0 - s);
    return kBase + ramp + kMyAmp * std::sin(2.0 * M_PI * y / kLy);
}

Observation makeObs(uint64_t frameIndex) {
    Observation o;
    o.width = kW;
    o.height = kH;
    o.avPixelFormat = AV_PIX_FMT_YUV420P16LE;
    o.frameIndex = frameIndex;
    o.planeCount = 1;
    o.color.pixelFormat = AV_PIX_FMT_YUV420P16LE;
    o.color.bitDepth = 16;
    o.linesize[0] = kW * 2;
    o.plane[0].assign(static_cast<size_t>(kW) * kH * 2, 0);
    return o;
}

void setSample(Observation& o, int x, int y, double value) {
    double v = std::lround(value);
    if (v < 0.0)
        v = 0.0;
    if (v > 65535.0)
        v = 65535.0;
    const auto u = static_cast<uint16_t>(v);
    const size_t off = (static_cast<size_t>(y) * kW + x) * 2;
    o.plane[0][off] = static_cast<uint8_t>(u & 0xff);
    o.plane[0][off + 1] = static_cast<uint8_t>(u >> 8);
}

Observation render(uint64_t frameIndex, double dx) {
    Observation o = makeObs(frameIndex);
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < kW; ++x)
            setSample(o, x, y, analytic(x - dx, static_cast<double>(y)));
    return o;
}

// Bilinear luma sample mirroring the accumulator's sampler (NaN when the
// fractional position leaves the plane).
double bilinear(const Observation& o, double fx, double fy, bool* ok) {
    *ok = false;
    if (!std::isfinite(fx) || !std::isfinite(fy) || fx < 0.0 || fy < 0.0
        || fx > static_cast<double>(o.width - 1)
        || fy > static_cast<double>(o.height - 1))
        return 0.0;
    const int x0 = static_cast<int>(std::floor(fx));
    const int y0 = static_cast<int>(std::floor(fy));
    const double tx = fx - x0, ty = fy - y0;
    const int x1 = std::min(x0 + 1, o.width - 1);
    const int y1 = std::min(y0 + 1, o.height - 1);
    auto at = [&](int x, int y) -> double {
        const size_t off = (static_cast<size_t>(y) * kW + x) * 2;
        return static_cast<double>(o.plane[0][off])
             + 256.0 * static_cast<double>(o.plane[0][off + 1]);
    };
    *ok = true;
    return (1.0 - tx) * (1.0 - ty) * at(x0, y0)
         + tx * (1.0 - ty) * at(x1, y0)
         + (1.0 - tx) * ty * at(x0, y1)
         + tx * ty * at(x1, y1);
}

double meanResidual(const Observation& target, const Observation& neighbor,
                    const FlowField& flow) {
    double sum = 0.0;
    long count = 0;
    for (int y = 0; y < kH; ++y) {
        for (int x = 0; x < kW; ++x) {
            const size_t i = (static_cast<size_t>(y) * kW + x) * 2;
            bool ok = false;
            const double warped =
                bilinear(neighbor, x + flow[i], y + flow[i + 1], &ok);
            if (!ok)
                continue;
            const double truth = bilinear(target, x, y, &ok);
            if (!ok)
                continue;
            sum += std::fabs(warped - truth);
            ++count;
        }
    }
    return count > 0 ? sum / static_cast<double>(count) : 0.0;
}

void testEstimatedArmRecoversKnownPhase() {
    const Observation target = render(0, 0.0);
    const Observation neighbor = render(1, -0.25); // N(x) = A(x + 0.25)
    const SampleGeometryEstimateResult est =
        estimateRelativeSampleGeometry(target, neighbor);
    CHECK(est.geometry.state == SampleGeometryState::Estimated);
    CHECK(std::fabs(est.geometry.phaseX - 0.25) <= 0.05);
    CHECK(std::fabs(est.geometry.phaseY) <= 0.05);
    CHECK(est.blocksUsed >= 8);
    CHECK(est.blocksConsidered >= est.blocksUsed);
    CHECK(est.insufficientReason.empty());
}

void testZeroOffsetNeighborYieldsZeroPhase() {
    const Observation target = render(0, 0.0);
    const Observation neighbor = render(1, 0.0);
    const SampleGeometryEstimateResult est =
        estimateRelativeSampleGeometry(target, neighbor);
    CHECK(est.geometry.state == SampleGeometryState::Estimated);
    CHECK(std::fabs(est.geometry.phaseX) <= 0.05);
    CHECK(std::fabs(est.geometry.phaseY) <= 0.05);
    CHECK(est.blocksUsed >= 8);
}

void testInsufficientOrMalformedInputsFailClosed() {
    // Flat image: no subpixel evidence anywhere.
    Observation flat = makeObs(0);
    for (int y = 0; y < kH; ++y)
        for (int x = 0; x < kW; ++x)
            setSample(flat, x, y, kBase);
    Observation flat2 = flat;
    flat2.frameIndex = 1;
    const SampleGeometryEstimateResult flatEst =
        estimateRelativeSampleGeometry(flat, flat2);
    CHECK(flatEst.geometry.state == SampleGeometryState::Unknown);
    CHECK(!flatEst.insufficientReason.empty());

    // Empty luma plane.
    Observation empty = render(0, 0.0);
    Observation emptyNeighbor = render(1, 0.0);
    emptyNeighbor.plane[0].clear();
    const SampleGeometryEstimateResult emptyEst =
        estimateRelativeSampleGeometry(empty, emptyNeighbor);
    CHECK(emptyEst.geometry.state == SampleGeometryState::Unknown);
    CHECK(!emptyEst.insufficientReason.empty());

    // Dimension mismatch.
    Observation wrong = render(1, 0.0);
    wrong.height = 64;
    wrong.plane[0].resize(static_cast<size_t>(64) * kW * 2);
    const SampleGeometryEstimateResult mismatchEst =
        estimateRelativeSampleGeometry(empty, wrong);
    CHECK(mismatchEst.geometry.state == SampleGeometryState::Unknown);
    CHECK(!mismatchEst.insufficientReason.empty());

    // Degenerate block sizes.
    const Observation target = render(0, 0.0);
    const Observation neighbor = render(1, 0.0);
    CHECK(estimateRelativeSampleGeometry(target, neighbor, 0)
              .geometry.state == SampleGeometryState::Unknown);
    CHECK(estimateRelativeSampleGeometry(target, neighbor, kH + 1)
              .geometry.state == SampleGeometryState::Unknown);

    // Unknown results must never carry a nonzero phase.
    const SampleGeometryEstimateResult flatAgain =
        estimateRelativeSampleGeometry(flat, flat2);
    CHECK(flatAgain.geometry.phaseX == 0.0f
          && flatAgain.geometry.phaseY == 0.0f);
}

void testEstimatedGeometryReducesPhotometricResidual() {
    // BOTH fractional quadrants must improve (review pass 2, R2): the
    // estimator emits a [0,1) phase representative, and the runner's integer
    // flow is the SAD argmin (~round(D)), so the sampler needs the wrapped
    // SIGNED residual — a mod-1 application landed a full pixel away on one
    // quadrant and measurably DEGRADED reconstruction. This test fails on
    // the pre-fix applyRelativeSampleGeometry application for dx = +0.25.
    for (const double dx : {-0.25, +0.25}) {
        const Observation target = render(0, 0.0);
        const Observation neighbor = render(1, dx);
        const SampleGeometryEstimateResult est =
            estimateRelativeSampleGeometry(target, neighbor);
        CHECK(est.geometry.state == SampleGeometryState::Estimated);

        // Zero flow field with full proven coverage: the integer part of the
        // true displacement is 0, exactly like the runner's correspondence
        // flow for sub-pixel-only motion.
        FlowField flow(static_cast<size_t>(kW) * kH * 2, 0.0f);
        std::vector<uint8_t> coverage(static_cast<size_t>(kW) * kH, 1);
        const double residualWithout = meanResidual(target, neighbor, flow);
        CHECK(residualWithout > 0.0);

        // Target anchor (Estimated, phase 0) and the estimated neighbor
        // phase; applyEstimatedPhaseResidual wraps the difference into
        // (-0.5, 0.5] before adding it to the covered flow.
        CHECK(applyEstimatedPhaseResidual(flow, coverage, kW, kH,
                                          0.0f, 0.0f,
                                          est.geometry.phaseX,
                                          est.geometry.phaseY));
        const double residualWith = meanResidual(target, neighbor, flow);
        CHECK(residualWith < residualWithout);
    }
}

void testDeterministicRepeat() {
    const Observation target = render(0, 0.0);
    const Observation neighbor = render(1, -0.25);
    const SampleGeometryEstimateResult a =
        estimateRelativeSampleGeometry(target, neighbor);
    const SampleGeometryEstimateResult b =
        estimateRelativeSampleGeometry(target, neighbor);
    CHECK(a.geometry.state == b.geometry.state);
    CHECK(a.geometry.phaseX == b.geometry.phaseX);
    CHECK(a.geometry.phaseY == b.geometry.phaseY);
    CHECK(a.blocksConsidered == b.blocksConsidered);
    CHECK(a.blocksUsed == b.blocksUsed);
    CHECK(a.madX == b.madX);
    CHECK(a.madY == b.madY);
    CHECK(a.insufficientReason == b.insufficientReason);
}

void testCodecMotionVectorsAreIgnored() {
    const Observation target = render(0, 0.0);
    const Observation neighbor = render(1, -0.25);
    const SampleGeometryEstimateResult baseline =
        estimateRelativeSampleGeometry(target, neighbor);

    Observation noisyTarget = target;
    Observation noisyNeighbor = neighbor;
    Observation::RawMv garbage;
    garbage.dstX = 7;
    garbage.dstY = 13;
    garbage.mvX = -3.75f;
    garbage.mvY = 11.25f;
    garbage.w = 8;
    garbage.h = 4;
    garbage.source = 1;
    garbage.motionScale = 4;
    for (int i = 0; i < 5; ++i) {
        noisyTarget.codecMotionVectors.push_back(garbage);
        noisyNeighbor.codecMotionVectors.push_back(garbage);
    }
    const SampleGeometryEstimateResult noisy =
        estimateRelativeSampleGeometry(noisyTarget, noisyNeighbor);
    CHECK(noisy.geometry.state == baseline.geometry.state);
    CHECK(noisy.geometry.phaseX == baseline.geometry.phaseX);
    CHECK(noisy.geometry.phaseY == baseline.geometry.phaseY);
    CHECK(noisy.blocksConsidered == baseline.blocksConsidered);
    CHECK(noisy.blocksUsed == baseline.blocksUsed);
    CHECK(noisy.madX == baseline.madX);
    CHECK(noisy.madY == baseline.madY);
    CHECK(noisy.insufficientReason == baseline.insufficientReason);
}

} // namespace

int main() {
    testEstimatedArmRecoversKnownPhase();
    testZeroOffsetNeighborYieldsZeroPhase();
    testInsufficientOrMalformedInputsFailClosed();
    testEstimatedGeometryReducesPhotometricResidual();
    testDeterministicRepeat();
    testCodecMotionVectorsAreIgnored();
    if (failures == 0)
        std::printf("anvil_geometry_estimate_tests: all checks passed\n");
    else
        std::printf("anvil_geometry_estimate_tests: %d failure(s)\n",
                    failures);
    return failures == 0 ? 0 : 1;
}
