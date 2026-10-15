// One-frame causal CPU integration slice: decoded -> ANVIL -> existing FSR.
// The hardware/zero-copy implementation is a separate performance milestone.
#include "anvil/LiveFrameProcessor.hpp"
#include "anvil/Reconstruct.hpp"
#include "anvil/ObservationAdapter.hpp"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <limits>
#include <vector>

extern "C" {
#include <libavutil/pixfmt.h>
}

namespace anvil {
namespace {
using Clock = std::chrono::steady_clock;
double elapsedMs(Clock::time_point start) {
    return std::chrono::duration<double, std::milli>(Clock::now() - start).count();
}
bool planar420(int fmt) {
    return fmt == AV_PIX_FMT_YUV420P ||
           fmt == AV_PIX_FMT_YUVJ420P ||
           fmt == AV_PIX_FMT_YUV420P10LE ||
           fmt == AV_PIX_FMT_YUV420P12LE ||
           fmt == AV_PIX_FMT_YUV420P16LE;
}
} // namespace

LiveFrameProcessor::LiveFrameProcessor(double budgetMs)
    : budgetMs_(std::isfinite(budgetMs) && budgetMs > 0 ? budgetMs : 6.0) {}

void LiveFrameProcessor::reset() {
    previousRaw_.reset();
    // Budget disable survives seeks: don't accidentally re-enable a path
    // already proven unsuitable for this playback session.
}

bool LiveFrameProcessor::supported(const temporal_forge::DecodedVideoFrame& d) {
    if (d.hwFrame || !planar420(d.avFormat) || d.width <= 0 || d.height <= 0 ||
        d.width % 2 || d.height % 2 || d.planes != 3 ||
        d.ptsUs < 0 || d.bitDepth < 8 || d.bitDepth > 16)
        return false;
    const int bps = d.bitDepth > 8 ? 2 : 1;
    for (int p = 0; p < 3; ++p) {
        const int w = p == 0 ? d.width : d.width / 2;
        const int h = p == 0 ? d.height : d.height / 2;
        if (d.linesize[p] < w * bps || d.linesize[p] <= 0)
            return false;
        const size_t minSize =
            static_cast<size_t>(h - 1) * static_cast<size_t>(d.linesize[p]) +
            static_cast<size_t>(w) * bps;
        if (d.plane[p].size() < minSize)
            return false;
    }
    return true;
}

LiveFrameResult LiveFrameProcessor::process(temporal_forge::DecodedVideoFrame& frame,
                                             bool resetHistory) {
    if (resetHistory) reset();
    if (budgetDisabled_) return {LiveFrameStatus::OverBudget, 0.0, 0};
    if (!supported(frame)) {
        reset();
        return {LiveFrameStatus::Unsupported, 0.0, 0};
    }
    const auto started = Clock::now();
    Observation raw = observationFromDecoded(frame);
    if (elapsedMs(started) > budgetMs_) {
        budgetDisabled_ = true;
        previousRaw_.reset();
        return {LiveFrameStatus::OverBudget, elapsedMs(started), 0};
    }
    if (!previousRaw_) {
        previousRaw_ = std::move(raw);
        return {LiveFrameStatus::Primed, elapsedMs(started), 0};
    }
    if (frame.frameIndex != previousRaw_->frameIndex + 1 ||
        frame.ptsUs <= previousRaw_->ptsUs ||
        !reconstructionSpaceCompatible(raw, *previousRaw_)) {
        previousRaw_ = std::move(raw);
        return {LiveFrameStatus::Discontinuity, elapsedMs(started), 0};
    }

    // This is deliberately the minimal causal arm. The full ANVIL roster may
    // later select codec/estimated correspondence, larger windows, or GPU stages.
    const auto blocks = estimateCorrespondence(raw, *previousRaw_, 16, 4, 2);
    std::vector<uint8_t> coverage;
    FlowField flow = buildFlowField(raw.width, raw.height, blocks,
                                    static_cast<int64_t>(previousRaw_->frameIndex),
                                    &coverage);
    std::vector<Visibility> visibility(coverage.size(), Visibility::Invalid);
    for (size_t i = 0; i < coverage.size(); ++i)
        if (coverage[i]) visibility[i] = Visibility::Valid;
    ConfidenceField confidence = estimateConfidence(raw, *previousRaw_, flow, coverage);

    // Advance history with original decoded observations, not processed output.
    std::vector<Observation> neighbors;
    neighbors.push_back(std::move(*previousRaw_));
    previousRaw_ = std::move(raw);
    AccumulateResult result = accumulate(previousRaw_.value(), neighbors,
                                         {std::move(flow)}, {std::move(visibility)},
                                         {std::move(confidence)});
    const double elapsed = elapsedMs(started);
    if (elapsed > budgetMs_) {
        budgetDisabled_ = true;
        previousRaw_.reset();
        // Never publish an over-budget result; retain the decoder's source.
        return {LiveFrameStatus::OverBudget, elapsed, result.validSamples};
    }
    if (result.validSamples == 0)
        return {LiveFrameStatus::NoCorrespondence, elapsed, 0};

    // Reuse the input decoder's exact PTS, dimensions, color metadata and
    // motion side information: only replace the supported pixel planes.
    for (int p = 0; p < frame.planes && p < 4; ++p)
        frame.plane[p] = std::move(result.frame.plane[p]);
    return {LiveFrameStatus::Reconstructed, elapsed, result.validSamples};
}

const char* liveFrameStatusName(LiveFrameStatus status) {
    switch (status) {
        case LiveFrameStatus::Bypassed: return "bypassed";
        case LiveFrameStatus::Primed: return "primed";
        case LiveFrameStatus::Reconstructed: return "reconstructed";
        case LiveFrameStatus::NoCorrespondence: return "no-correspondence";
        case LiveFrameStatus::Unsupported: return "unsupported";
        case LiveFrameStatus::Discontinuity: return "discontinuity";
        case LiveFrameStatus::OverBudget: return "over-budget";
    }
    return "unknown";
}
} // namespace anvil
