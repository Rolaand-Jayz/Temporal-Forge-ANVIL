// TimestampSelect.hpp — exact timestamp frame-selection helpers.
//
// Native ticks are the lossless address. Microsecond selection remains a
// convenience domain and is exact only within that rescaled domain. Missing
// timestamps (source == none) are NEVER candidates, even though the decoder's
// internal convenience value for microseconds is zero.
//
// Contract:
// - exact equality only; NEVER nearest-frame fallback;
// - duplicate matching timestamps are Ambiguous;
// - negative timestamp values are valid;
// - source 0 (none) is not addressable;
// - deterministic, header-only shared semantics for runner + tests.
#pragma once
#include <cstdint>
#include <utility>
#include <vector>

namespace anvil {

enum class TickSelectionOutcome {
    Match = 0,
    NoMatch = 1,
    Ambiguous = 2,
};

inline bool hasResolvedTimestampSource(int ptsSource) {
    return ptsSource == 1 || ptsSource == 2;
}

// Native timestamp addressability is keyed on resolved source, not a sentinel:
// pts == -1 is a legitimate value when source is PTS/best-effort.
inline bool hasNativeTimestamp(int64_t ticks, int ptsSource) {
    (void)ticks;
    return hasResolvedTimestampSource(ptsSource);
}

inline TickSelectionOutcome selectFrameByTicks(
        const std::vector<std::pair<int64_t, uint64_t>>& candidates,
        int64_t wantTick, uint64_t& outFrameIndex) {
    uint64_t matched = 0;
    bool found = false;
    bool duplicate = false;
    for (const auto& [tick, index] : candidates) {
        if (tick != wantTick) continue;
        if (found) { duplicate = true; break; }
        matched = index;
        found = true;
    }
    if (duplicate) return TickSelectionOutcome::Ambiguous;
    if (!found) return TickSelectionOutcome::NoMatch;
    outFrameIndex = matched;
    return TickSelectionOutcome::Match;
}

struct MicrosecondTimestampCandidate {
    int64_t microseconds = 0;
    int source = 0; // 0 none, 1 pts, 2 best_effort
    uint64_t frameIndex = 0;
};

// Exact microsecond-domain selection. Source-none observations are filtered
// before equality, preventing the decoder's internal 0-us placeholder for
// "no timestamp" from fabricating an exact timestamp-0 match.
inline TickSelectionOutcome selectFrameByMicroseconds(
        const std::vector<MicrosecondTimestampCandidate>& candidates,
        int64_t wantUs, uint64_t& outFrameIndex) {
    uint64_t matched = 0;
    bool found = false;
    bool duplicate = false;
    for (const auto& candidate : candidates) {
        if (!hasResolvedTimestampSource(candidate.source)) continue;
        if (candidate.microseconds != wantUs) continue;
        if (found) { duplicate = true; break; }
        matched = candidate.frameIndex;
        found = true;
    }
    if (duplicate) return TickSelectionOutcome::Ambiguous;
    if (!found) return TickSelectionOutcome::NoMatch;
    outFrameIndex = matched;
    return TickSelectionOutcome::Match;
}

} // namespace anvil
