// TimestampResolve.hpp — pure display-timestamp resolution for decoded
// frames (review 4209783084).
//
// The decoder fills DecodedVideoFrame::ptsUs from AVFrame::pts when present
// and otherwise from AVFrame::best_effort_timestamp. Before this helper the
// native tick (ptsTicks) mirrored AVFrame::pts ONLY, so a frame whose
// displayed timestamp came from best_effort carried pts_ticks = -1 while
// claiming timestamp_source "best_effort" — its exact native identity was
// silently discarded. The helper resolves ticks, microseconds and the
// provenance label from ONE source so they can never disagree:
//
//   pts valid        -> ticks = pts,         source = 1 ("pts")
//   pts NOPTS,
//   best_effort set  -> ticks = best_effort, source = 2 ("best_effort")
//   both NOPTS       -> ticks = -1,          source = 0 ("none")
//
// Microseconds use av_rescale_q exactly as the fill path always has, so
// existing µs values are unchanged; the native tick is the exact identity
// (µs rescaling is lossy for timebases finer than 1/1000000 by design).
// Header-only and free of AVFrame so every branch is unit-testable without
// a decoder. Deterministic; negative ticks are valid values, not errors.
#pragma once
#include <cstdint>

extern "C" {
#include <libavutil/avutil.h>
#include <libavutil/mathematics.h>
#include <libavutil/rational.h>
}

namespace temporal_forge {

struct ResolvedTimestamp {
    int64_t ticks = -1;   // native tick of the displayed timestamp (-1 none)
    int64_t microseconds = 0;
    int source = 0;       // 0 none, 1 AVFrame::pts, 2 best_effort_timestamp
};

inline ResolvedTimestamp resolveDisplayTimestamp(int64_t pts,
                                                 int64_t bestEffort,
                                                 AVRational timebase) {
    ResolvedTimestamp r;
    if (pts != AV_NOPTS_VALUE) {
        r.ticks = pts;
        r.source = 1;
    } else if (bestEffort != AV_NOPTS_VALUE) {
        r.ticks = bestEffort;
        r.source = 2;
    } else {
        return r;
    }
    if (timebase.num != 0 && timebase.den != 0) {
        r.microseconds = av_rescale_q(r.ticks, timebase, AVRational{1, 1000000});
    }
    return r;
}

} // namespace temporal_forge
