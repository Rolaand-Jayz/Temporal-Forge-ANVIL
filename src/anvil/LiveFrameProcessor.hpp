// Opt-in real-time ANVIL predecessor to the existing FSR stage.
// This is a CPU proof of placement, NOT a qualified 30/60 FPS engine.
// Each call produces either a same-PTS decoded frame or a bypass.
#pragma once
#include "anvil/Core.hpp"
#include "media/VideoDecoder.hpp"
#include <optional>

namespace anvil {

enum class LiveFrameStatus {
    Bypassed, Primed, Reconstructed, NoCorrespondence, Unsupported,
    Discontinuity, OverBudget
};

struct LiveFrameResult {
    LiveFrameStatus status = LiveFrameStatus::Bypassed;
    double elapsedMs = 0.0;
    uint64_t usableSamples = 0;
};

class LiveFrameProcessor {
public:
    // Stage-only budget, not total playback frame budget. A missed deadline
    // disables reconstruction for this processor instance (fail-open).
    explicit LiveFrameProcessor(double budgetMs = 6.0);

    LiveFrameResult process(temporal_forge::DecodedVideoFrame& decoded,
                            bool resetHistory);
    void reset();
    bool disabledForBudget() const { return budgetDisabled_; }

private:
    static bool supported(const temporal_forge::DecodedVideoFrame& decoded);
    std::optional<Observation> previousRaw_;
    double budgetMs_ = 6.0;
    bool budgetDisabled_ = false;
};

const char* liveFrameStatusName(LiveFrameStatus status);
} // namespace anvil
