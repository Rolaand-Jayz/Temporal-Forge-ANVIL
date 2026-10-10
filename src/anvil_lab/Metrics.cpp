// Metrics.cpp — PSNR / SSIM / edge / temporal measurements on luma planes.
#include "Metrics.hpp"

#include <algorithm>
#include <cmath>

namespace anvil_lab {
namespace {

// 11x11 Gaussian window, sigma 1.5 (SSIM standard).
constexpr int kWin = 11;
constexpr double kSigma = 1.5;

const std::vector<double>& gaussianWindow() {
    static const std::vector<double> w = [] {
        std::vector<double> g(static_cast<size_t>(kWin) * kWin);
        const double c = (kWin - 1) / 2.0;
        double sum = 0.0;
        for (int y = 0; y < kWin; ++y)
            for (int x = 0; x < kWin; ++x) {
                const double dx = x - c, dy = y - c;
                const double v = std::exp(-(dx * dx + dy * dy) / (2.0 * kSigma * kSigma));
                g[static_cast<size_t>(y) * kWin + x] = v;
                sum += v;
            }
        for (double& v : g) v /= sum;
        return g;
    }();
    return w;
}

bool sameGeometry(const Image& a, const Image& b, std::string& err) {
    if (a.width != b.width || a.height != b.height) {
        err = "geometry mismatch: " + std::to_string(a.width) + "x"
            + std::to_string(a.height) + " vs " + std::to_string(b.width) + "x"
            + std::to_string(b.height);
        return false;
    }
    if (a.maxval != b.maxval) {
        err = "maxval mismatch: " + std::to_string(a.maxval) + " vs "
            + std::to_string(b.maxval);
        return false;
    }
    if (a.isGray() != b.isGray()) {
        err = "color representation mismatch (gray vs RGB)";
        return false;
    }
    return true;
}

} // namespace

double temporalDelta(const Image& frameA, const Image& frameB, std::string& err) {
    if (!sameGeometry(frameA, frameB, err)) return -1.0;
    const std::vector<double> la = frameA.luma709();
    const std::vector<double> lb = frameB.luma709();
    double sum = 0.0;
    for (size_t i = 0; i < la.size(); ++i) sum += std::fabs(la[i] - lb[i]);
    return la.empty() ? 0.0 : sum / static_cast<double>(la.size());
}

bool computePlaneMetrics(const Image& a, const Image& b, PlaneMetrics& out,
                         std::string& err) {
    if (!sameGeometry(a, b, err)) return false;
    const int w = a.width, h = a.height;
    const double L = static_cast<double>(a.maxval);
    const std::vector<double> x = a.luma709();
    const std::vector<double> y = b.luma709();
    const size_t n = x.size();

    double se = 0.0, sa = 0.0;
    double maxAbs = 0.0;
    for (size_t i = 0; i < n; ++i) {
        const double d = x[i] - y[i];
        se += d * d;
        sa += std::fabs(d);
        maxAbs = std::max(maxAbs, std::fabs(d));
    }
    out.mse = se / static_cast<double>(n);
    out.meanAbsDiff = sa / static_cast<double>(n);
    out.maxAbsDiff = maxAbs;
    out.identical = (sa == 0.0);
    out.psnr = out.identical ? -1.0 : 10.0 * std::log10((L * L) / out.mse);

    // Sobel magnitudes (edge-sensitive detail comparison).
    auto sobel = [&](const std::vector<double>& p) {
        std::vector<double> mag(n, 0.0);
        for (int j = 1; j < h - 1; ++j)
            for (int i = 1; i < w - 1; ++i) {
                const size_t c = static_cast<size_t>(j) * w + i;
                const double gx = -p[c - w - 1] - 2 * p[c - 1] - p[c + w - 1]
                    + p[c - w + 1] + 2 * p[c + 1] + p[c + w + 1];
                const double gy = -p[c - w - 1] - 2 * p[c - w] - p[c - w + 1]
                    + p[c + w - 1] + 2 * p[c + w] + p[c + w + 1];
                mag[c] = std::sqrt(gx * gx + gy * gy);
            }
        return mag;
    };
    const auto magX = sobel(x);
    const auto magY = sobel(y);
    double edgeSum = 0.0;
    size_t edgeCount = 0;
    for (size_t i = 0; i < n; ++i) {
        edgeSum += std::fabs(magX[i] - magY[i]);
        ++edgeCount;
    }
    out.edgeDiffMean = edgeCount ? edgeSum / static_cast<double>(edgeCount) : 0.0;

    // Mean SSIM over fully-supported 11x11 windows (no padded truncation).
    const std::vector<double>& win = gaussianWindow();
    const double C1 = (0.01 * L) * (0.01 * L);
    const double C2 = (0.03 * L) * (0.03 * L);
    if (w < kWin || h < kWin) {
        err = "image smaller than the SSIM window";
        return false;
    }
    double ssimSum = 0.0;
    uint64_t ssimCount = 0;
    const int half = kWin / 2;
    for (int j = half; j <= h - 1 - half; ++j)
        for (int i = half; i <= w - 1 - half; ++i) {
            double mx = 0, my = 0;
            for (int dy = -half; dy <= half; ++dy)
                for (int dx = -half; dx <= half; ++dx) {
                    const double wv =
                        win[static_cast<size_t>(dy + half) * kWin + (dx + half)];
                    mx += wv * x[static_cast<size_t>(j + dy) * w + (i + dx)];
                    my += wv * y[static_cast<size_t>(j + dy) * w + (i + dx)];
                }
            double vx = 0, vy = 0, cov = 0;
            for (int dy = -half; dy <= half; ++dy)
                for (int dx = -half; dx <= half; ++dx) {
                    const double wv =
                        win[static_cast<size_t>(dy + half) * kWin + (dx + half)];
                    const double ddx =
                        x[static_cast<size_t>(j + dy) * w + (i + dx)] - mx;
                    const double ddy =
                        y[static_cast<size_t>(j + dy) * w + (i + dx)] - my;
                    vx += wv * ddx * ddx;
                    vy += wv * ddy * ddy;
                    cov += wv * ddx * ddy;
                }
            const double num = (2 * mx * my + C1) * (2 * cov + C2);
            const double den = (mx * mx + my * my + C1) * (vx + vy + C2);
            ssimSum += num / den;
            ++ssimCount;
        }
    out.ssim = ssimCount ? ssimSum / static_cast<double>(ssimCount) : 1.0;
    return true;
}

} // namespace anvil_lab
