// Resize.cpp — separable Bicubic/Lanczos3 resampler.
#include "Resize.hpp"

#include <algorithm>
#include <cmath>

namespace anvil_lab {
namespace {

constexpr double kBicubicA = -0.5;

double bicubicKernel(double x) {
    x = std::fabs(x);
    if (x <= 1.0)
        return (kBicubicA + 2.0) * x * x * x - (kBicubicA + 3.0) * x * x + 1.0;
    if (x < 2.0)
        return kBicubicA * (x * x * x - 5.0 * x * x + 8.0 * x - 4.0);
    return 0.0;
}

double sinc(double x) {
    if (x == 0.0) return 1.0;
    const double px = M_PI * x;
    return std::sin(px) / px;
}

double lanczos3Kernel(double x) {
    x = std::fabs(x);
    if (x < 3.0) return sinc(x) * sinc(x / 3.0);
    return 0.0;
}

struct Weights {
    std::vector<int> start;
    std::vector<std::vector<double>> w; // per output sample
};

Weights computeWeights(int inN, int outN, bool cubic) {
    Weights wt;
    wt.start.resize(outN);
    wt.w.resize(outN);
    // Downscale widens the kernel so the filter integrates over the source
    // footprint (area-consistent, no aliasing-free claim implied).
    const double scale = static_cast<double>(inN) / static_cast<double>(outN);
    const double filterScale = std::max(1.0, scale);
    const double support = (cubic ? 2.0 : 3.0) * filterScale;
    for (int o = 0; o < outN; ++o) {
        const double center = (o + 0.5) * scale - 0.5;
        const int i0 = static_cast<int>(std::floor(center - support));
        const int i1 = static_cast<int>(std::ceil(center + support));
        double sum = 0.0;
        wt.w[o].reserve(static_cast<size_t>(i1 - i0 + 1));
        wt.start[o] = i0;
        for (int i = i0; i <= i1; ++i) {
            const double x = (i - center) / filterScale;
            const double k = cubic ? bicubicKernel(x) : lanczos3Kernel(x);
            wt.w[o].push_back(k);
            sum += k;
        }
        if (sum != 0.0)
            for (double& k : wt.w[o]) k /= sum;
    }
    return wt;
}

} // namespace

const char* scaleFilterName(ScaleFilter f) {
    return f == ScaleFilter::Bicubic ? "bicubic" : "lanczos3";
}

std::vector<double> resizePlane(const std::vector<double>& in, int inW, int inH,
                                int outW, int outH, ScaleFilter filter) {
    if (inW <= 0 || inH <= 0 || outW <= 0 || outH <= 0
        || in.size() != static_cast<size_t>(inW) * inH)
        return {};
    const bool cubic = filter == ScaleFilter::Bicubic;
    const Weights wx = computeWeights(inW, outW, cubic);
    const Weights wy = computeWeights(inH, outH, cubic);

    auto sample = [&](int x, int y) {
        x = std::clamp(x, 0, inW - 1);
        y = std::clamp(y, 0, inH - 1);
        return in[static_cast<size_t>(y) * inW + x];
    };

    // Horizontal pass into a transposed intermediate, then vertical.
    std::vector<double> tmp(static_cast<size_t>(outW) * inH);
    for (int y = 0; y < inH; ++y)
        for (int ox = 0; ox < outW; ++ox) {
            double acc = 0.0;
            const int s = wx.start[ox];
            for (size_t k = 0; k < wx.w[ox].size(); ++k)
                acc += wx.w[ox][k] * sample(s + static_cast<int>(k), y);
            tmp[static_cast<size_t>(y) * outW + ox] = acc;
        }
    std::vector<double> out(static_cast<size_t>(outW) * outH);
    for (int oy = 0; oy < outH; ++oy)
        for (int x = 0; x < outW; ++x) {
            double acc = 0.0;
            const int s = wy.start[oy];
            for (size_t k = 0; k < wy.w[oy].size(); ++k)
                acc += wy.w[oy][k] * tmp[static_cast<size_t>(s + static_cast<int>(k)) * outW + x];
            out[static_cast<size_t>(oy) * outW + x] = acc;
        }
    return out;
}

Image resizeImage(const Image& in, int outW, int outH, ScaleFilter filter) {
    Image out;
    out.width = outW;
    out.height = outH;
    out.maxval = in.maxval;
    const auto convert = [&](const std::vector<uint16_t>& plane) {
        if (plane.empty()) return std::vector<uint16_t>();
        std::vector<double> d(plane.size());
        for (size_t i = 0; i < plane.size(); ++i) d[i] = plane[i];
        std::vector<double> r = resizePlane(d, in.width, in.height, outW, outH, filter);
        std::vector<uint16_t> u(r.size());
        for (size_t i = 0; i < r.size(); ++i) {
            const double v = std::lround(r[i]);
            u[i] = static_cast<uint16_t>(
                std::clamp<double>(v, 0.0, static_cast<double>(in.maxval)));
        }
        return u;
    };
    out.r = convert(in.r);
    out.g = convert(in.g);
    out.b = convert(in.b);
    return out;
}

} // namespace anvil_lab
