// Opt-in real-time ANVIL predecessor to the existing FSR stage.
// This is a CPU proof of placement, NOT a qualified 30/60 FPS engine.
// Each call produces either a same-PTS decoded frame or a bypass.
#pragma once
#include "anvil/Core.hpp"
#include "media/VideoDecoder.hpp"
#include <optional>
#include <array>

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

// User-facing aggregate of the opt-in live stage, folded from the per-frame
// LiveFrameStatus by the decode loop. The native player UI binds its compare
// pane label, HUD and settings text to this state so a pane is called SOURCE
// only while it genuinely displays decoder-owned pixels.
enum class LiveUserStage { Off, Standby, Reconstructing, Bypassed };

class LiveStageTracker {
public:
    explicit LiveStageTracker(bool enabled)
        : stage_(enabled ? LiveUserStage::Standby : LiveUserStage::Off) {}

    void observe(LiveFrameStatus status) {
        if (stage_ == LiveUserStage::Off || stage_ == LiveUserStage::Bypassed)
            return;
        switch (status) {
        case LiveFrameStatus::Reconstructed:
            stage_ = LiveUserStage::Reconstructing;
            break;
        case LiveFrameStatus::OverBudget:
            // A budget miss permanently disables the stage for this playback
            // instance; later frames are decoder-owned again, but the stage
            // must not silently vanish from the status surface.
            stage_ = LiveUserStage::Bypassed;
            break;
        default:
            // Priming, unsupported format, discontinuity and absent
            // correspondence all leave THIS frame's pixels decoder-owned:
            // the stage reports standby until reconstruction resumes, so a
            // pane never attributes unprocessed pixels to ANVIL either.
            stage_ = LiveUserStage::Standby;
            break;
        }
    }

    LiveUserStage stage() const { return stage_; }

    static const char* stageName(LiveUserStage stage) {
        switch (stage) {
        case LiveUserStage::Off: return "off";
        case LiveUserStage::Standby: return "standby";
        case LiveUserStage::Reconstructing: return "reconstructing";
        case LiveUserStage::Bypassed: return "bypassed";
        }
        return "unknown";
    }

private:
    LiveUserStage stage_;
};
// A decoded frame is NOT a displayed frame. Remember each GPU slot's
// ownership until its compute/presentation work completes successfully.
// Failed and pending frames must never relabel a previously published image.
class LivePublicationTracker {
public:
    explicit LivePublicationTracker(bool enabled)
        : displayed_(enabled ? LiveUserStage::Standby : LiveUserStage::Off) {}

    void submit(unsigned slot, LiveUserStage stage) {
        if (slot < pending_.size()) pending_[slot] = stage;
    }
    LiveUserStage retire(unsigned slot, bool publishedSuccessfully) {
        if (slot < pending_.size()) {
            if (publishedSuccessfully && pending_[slot].has_value())
                displayed_ = *pending_[slot];
            pending_[slot].reset();
        }
        return displayed_;
    }
    LiveUserStage publishDirect(LiveUserStage stage) {
        displayed_ = stage;
        return displayed_;
    }
    LiveUserStage stage() const { return displayed_; }
    void clearPending() { for (auto& item : pending_) item.reset(); }

private:
    LiveUserStage displayed_;
    std::array<std::optional<LiveUserStage>, 2> pending_{};
};
} // namespace anvil
