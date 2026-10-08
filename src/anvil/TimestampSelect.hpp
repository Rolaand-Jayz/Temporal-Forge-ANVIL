// TimestampSelect.hpp — exact native-tick frame selection (review 4209783084).
//
// The runner's --start-pts-us control selects on rescaled microseconds, which
// is lossy for any stream timebase finer than 1/1000000: distinct native pts
// ticks can rescale to the same microsecond, making an original source
// timestamp ambiguous or impossible to address. This header provides the
// exact-tick counterpart over the native (ptsTicks, tbNum/tbDen) identity the
// decoder now carries through DecodedVideoFrame/Observation.
//
// Contract:
// - exact equality on native ticks only; there is NEVER a nearest-frame
//   fallback (same semantics as the runner's exact-timestamp selection);
// - duplicate ticks among candidates are Ambiguous — selection is refused
//   rather than silently picking one;
// - negative ticks are valid PTS values, not malformed input;
// - deterministic and allocation-free; header-only so any consumer (runner,
//   tests, later tooling) can share the identical selection semantics.
#pragma once
#include <cstdint>
#include <utility>
#include <vector>

namespace anvil {

enum class TickSelectionOutcome {
    Match = 0,     // exactly one candidate carries wantTick
    NoMatch = 1,   // no candidate carries wantTick
    Ambiguous = 2, // more than one candidate carries wantTick
};

// candidates: (native tick, decode frame index) pairs, in any order —
//             typically Observation::ptsTicks keyed by Observation::frameIndex.
// wantTick:   the native tick to address, in the stream timebase tbNum/tbDen
//             (callers must compare only ticks of one declared timebase).
// outFrameIndex: receives the single matching decode index on Match only
//             (untouched otherwise).
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
    outFrameIndex = matched; // written on Match only
    return TickSelectionOutcome::Match;
}

} // namespace anvil
