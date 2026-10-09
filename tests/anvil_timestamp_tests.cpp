// anvil_timestamp_tests.cpp — native-timestamp identity tests (review 4209783084).
//
// Two layers:
// 1. Unit tests for anvil::selectFrameByTicks: exact-equality-only selection
//    with no nearest-frame fallback, duplicates ambiguous, negative ticks valid.
// 2. Decode-level regression through the real Demuxer+VideoDecoder path on a
//    generated fine-timebase fixture (mp4 track timescale 10 MHz) whose video
//    adjacent frames sit 5 ticks (0.5 us) apart and collapse to the SAME
//    rounded microsecond after av_rescale_q. All fixture expectations are
//    derived at runtime from ffprobe of the actual container — nothing about
//    the ticks or timebase is hardcoded here.
//
// Skip conditions (deterministic, not failures): fixture file missing or
// ffprobe unavailable -> exit 77 (same convention as media_pipeline_tests).
#include "anvil/TimestampSelect.hpp"
#include "media/TimestampResolve.hpp"
#include "media/Demuxer.hpp"
#include "media/VideoDecoder.hpp"

extern "C" {
#include <libavutil/rational.h>
#include <libavutil/mathematics.h>
}

#include <cstdlib>
#include <filesystem>
#include <cstdio>
#include <string>
#include <vector>

using namespace temporal_forge;

static int g_failures = 0;
#define CHECK(cond) do { \
    if (!(cond)) { std::fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); ++g_failures; } \
} while (0)

namespace {

// --- Part 1: selectFrameByTicks unit tests (no media backend needed) --------

void testSelectByTicksUnit() {
    using anvil::selectFrameByTicks;
    using anvil::TickSelectionOutcome;

    // match: exact tick selects exactly its decode index
    const std::vector<std::pair<int64_t, uint64_t>> stream = {
        {0, 0}, {1000000, 1}, {2000000, 2}};
    uint64_t out = UINT64_MAX;
    CHECK(selectFrameByTicks(stream, 1000000, out) == TickSelectionOutcome::Match);
    CHECK(out == 1);

    // no-match: no nearest-frame fallback ever fires
    out = UINT64_MAX;
    CHECK(selectFrameByTicks(stream, 1500000, out) == TickSelectionOutcome::NoMatch);
    CHECK(out == UINT64_MAX); // untouched on failure
    CHECK(selectFrameByTicks(stream, 999999, out) == TickSelectionOutcome::NoMatch);
    CHECK(selectFrameByTicks(stream, 2000001, out) == TickSelectionOutcome::NoMatch);

    // hasNativeTimestamp keys on the resolved source, never a tick sentinel:
    // a legitimate pts == -1 (source 1) stays addressable; source 0 never is.
    CHECK(anvil::hasNativeTimestamp(-1, 1));
    CHECK(anvil::hasNativeTimestamp(0, 1));
    CHECK(anvil::hasNativeTimestamp(-1000000, 2));
    CHECK(!anvil::hasNativeTimestamp(-1, 0));
    CHECK(!anvil::hasNativeTimestamp(12345, 0));

    // ambiguous: duplicate ticks are refused, not silently resolved to one
    const std::vector<std::pair<int64_t, uint64_t>> colliding = {
        {4000006, 4}, {4000010, 5}};
    out = UINT64_MAX;
    CHECK(selectFrameByTicks(colliding, 4000006, out) == TickSelectionOutcome::Match);
    CHECK(out == 4);
    CHECK(selectFrameByTicks(colliding, 4000010, out) == TickSelectionOutcome::Match);
    CHECK(out == 5);
    // but the microseconds these rescale to are identical — the µs domain is
    // lossy and tick selection is what distinguishes the frames
    CHECK(av_rescale_q(4000006, AVRational{1, 10000000}, AVRational{1, 1000000})
          == av_rescale_q(4000010, AVRational{1, 10000000}, AVRational{1, 1000000}));

    const std::vector<std::pair<int64_t, uint64_t>> duplicated = {
        {7, 3}, {9, 4}, {7, 5}};
    out = UINT64_MAX;
    CHECK(selectFrameByTicks(duplicated, 7, out) == TickSelectionOutcome::Ambiguous);
    CHECK(out == UINT64_MAX); // untouched on ambiguity

    // empty candidate set: a named no-match, never a crash
    out = UINT64_MAX;
    CHECK(selectFrameByTicks({}, 0, out) == TickSelectionOutcome::NoMatch);
    CHECK(out == UINT64_MAX);

    // negative ticks are valid PTS values, not malformed input
    const std::vector<std::pair<int64_t, uint64_t>> negative = {
        {-1000000, 0}, {0, 1}};
    out = UINT64_MAX;
    CHECK(selectFrameByTicks(negative, -1000000, out) == TickSelectionOutcome::Match);
    CHECK(out == 0);
    CHECK(selectFrameByTicks(negative, 0, out) == TickSelectionOutcome::Match);
    CHECK(out == 1);
}

// --- Part 2: fixture-derived decode regression -------------------------------

struct ProbeTruth {
    int tbNum = 0;
    int tbDen = 0;
    std::vector<int64_t> frameTicks; // container frame pts, in stream order
};

std::string runProbe(const std::string& ffprobe, const std::string& args) {
    std::string cmd = ffprobe + " " + args + " 2>/dev/null";
    std::string out;
    FILE* pipe = popen(cmd.c_str(), "r");
    if (!pipe) return out;
    char buf[512];
    while (size_t n = fread(buf, 1, sizeof(buf), pipe)) out.append(buf, n);
    pclose(pipe);
    return out;
}

bool probeTruth(const std::string& ffprobe, const std::string& fixture,
                ProbeTruth& truth) {
    // stream time_base, e.g. "1/10000000"
    const std::string tb = runProbe(ffprobe,
        "-v quiet -select_streams v:0 -show_entries stream=time_base"
        " -of csv=p=0 '" + fixture + "'");
    const size_t slash = tb.find('/');
    if (slash == std::string::npos) return false;
    truth.tbNum = std::atoi(tb.substr(0, slash).c_str());
    truth.tbDen = std::atoi(tb.substr(slash + 1).c_str());
    if (truth.tbNum <= 0 || truth.tbDen <= 0) return false;

    // per-frame raw pts ticks (decimal integers, stream order)
    const std::string frames = runProbe(ffprobe,
        "-v quiet -select_streams v:0 -show_entries frame=pts"
        " -of csv=p=0 '" + fixture + "'");
    size_t pos = 0;
    while (pos < frames.size()) {
        size_t eol = frames.find('\n', pos);
        if (eol == std::string::npos) eol = frames.size();
        const std::string line = frames.substr(pos, eol - pos);
        if (!line.empty()) {
            // csv rows may carry trailing empty fields (observed for frame 0);
            // the pts tick is always the first field.
            const size_t comma = line.find(',');
            const std::string first =
                comma == std::string::npos ? line : line.substr(0, comma);
            char* end = nullptr;
            const long long v = std::strtoll(first.c_str(), &end, 10);
            if (end && *end == '\0') truth.frameTicks.push_back(v);
            else return false;
        }
        pos = eol + 1;
    }
    return truth.frameTicks.size() >= 2;
}

void testSelectByMicrosecondsUnit() {
    using anvil::MicrosecondTimestampCandidate;
    using anvil::selectFrameByMicroseconds;
    using anvil::TickSelectionOutcome;

    uint64_t out = UINT64_MAX;
    const std::vector<MicrosecondTimestampCandidate> withMissingAndRealZero = {
        {0, 0, 0}, // no timestamp source: internal placeholder only
        {0, 1, 1}, // legitimate PTS == 0
        {1000000, 2, 2},
    };
    CHECK(selectFrameByMicroseconds(withMissingAndRealZero, 0, out) ==
          TickSelectionOutcome::Match);
    CHECK(out == 1);

    out = UINT64_MAX;
    CHECK(selectFrameByMicroseconds({{0, 0, 7}}, 0, out) ==
          TickSelectionOutcome::NoMatch);
    CHECK(out == UINT64_MAX);

    out = UINT64_MAX;
    CHECK(selectFrameByMicroseconds({{0, 1, 3}, {0, 2, 4}}, 0, out) ==
          TickSelectionOutcome::Ambiguous);
    CHECK(out == UINT64_MAX);

    CHECK(anvil::hasResolvedTimestampSource(1));
    CHECK(anvil::hasResolvedTimestampSource(2));
    CHECK(!anvil::hasResolvedTimestampSource(0));
}

} // namespace

// Review-pass-1 defect repair: every branch of the display-timestamp
// resolution must keep ticks, microseconds and the source label in agreement
// (a best-effort frame previously lost its native tick).
void testResolveDisplayTimestampUnit() {
    using temporal_forge::resolveDisplayTimestamp;
    constexpr int64_t kNoPts = AV_NOPTS_VALUE;

    // pts present: ticks mirror pts, source = 1
    {
        const auto r = resolveDisplayTimestamp(4000006, 123, AVRational{1, 10000000});
        CHECK(r.ticks == 4000006);
        CHECK(r.source == 1);
        CHECK(r.microseconds == av_rescale_q(4000006, AVRational{1, 10000000},
                                             AVRational{1, 1000000}));
    }
    // pts NOPTS, best_effort present: ticks mirror BEST_EFFORT (the repair),
    // source = 2 — the exact native identity of a best-effort frame survives
    {
        const auto r = resolveDisplayTimestamp(kNoPts, 4000010,
                                               AVRational{1, 10000000});
        CHECK(r.ticks == 4000010);
        CHECK(r.source == 2);
        CHECK(r.microseconds == 400001); // 0.4 us-fine tick rescales exactly
    }
    // both NOPTS: none, ticks -1, no fabricated values
    {
        const auto r = resolveDisplayTimestamp(kNoPts, kNoPts, AVRational{1, 25});
        CHECK(r.ticks == -1);
        CHECK(r.source == 0);
        CHECK(r.microseconds == 0);
    }
    // negative ticks are valid PTS values
    {
        const auto r = resolveDisplayTimestamp(-1000000, kNoPts,
                                               AVRational{1, 1000000});
        CHECK(r.ticks == -1000000);
        CHECK(r.source == 1);
        CHECK(r.microseconds == -1000000);
    }
    // fine-timebase collision is preserved by construction (µs lossy, ticks exact)
    {
        const auto a = resolveDisplayTimestamp(4000006, kNoPts,
                                               AVRational{1, 10000000});
        const auto b = resolveDisplayTimestamp(4000010, kNoPts,
                                               AVRational{1, 10000000});
        CHECK(a.ticks != b.ticks);
        CHECK(a.microseconds == b.microseconds);
    }
}

int main(int argc, char** argv) {
    testSelectByTicksUnit();
    testSelectByMicrosecondsUnit();
    testResolveDisplayTimestampUnit();

    // Fixture + ffprobe may be provided via environment (ctest) or argv, like
    // media_pipeline_tests. Missing pieces are a deterministic skip.
    std::string fixture, ffprobe;
    if (const char* env = std::getenv("ANVIL_TICK_FIXTURE")) fixture = env;
    if (argc > 1) fixture = argv[1];
    if (const char* env = std::getenv("ANVIL_FFPROBE")) ffprobe = env;
    if (ffprobe.empty()) ffprobe = "ffprobe";
    if (fixture.empty() || !std::filesystem::exists(fixture) ||
        runProbe(ffprobe, "-version").empty()) {
        std::fprintf(stderr,
                     "anvil_timestamp_tests: SKIP (fixture or ffprobe unavailable)\n");
        return g_failures == 0 ? 77 : 1;
    }

    // Container truth, derived at runtime (never hardcoded).
    ProbeTruth truth;
    if (!probeTruth(ffprobe, fixture, truth)) {
        std::fprintf(stderr, "FAIL: ffprobe could not read fixture truth\n");
        return 1;
    }
    std::printf("fixture truth: timebase %d/%d, %zu frame ticks:",
                truth.tbNum, truth.tbDen, truth.frameTicks.size());
    for (int64_t t : truth.frameTicks) std::printf(" %lld", (long long)t);
    std::printf("\n");

    // Locate the sub-microsecond colliding pair from the container itself:
    // distinct ticks that rescale to the same microsecond. The fixture fails
    // loudly here if the generating ffmpeg could not preserve a fine timebase.
    const AVRational tb{truth.tbNum, truth.tbDen};
    const AVRational us{1, 1000000};
    size_t a = 0, b = 0;
    bool collisionFound = false;
    for (size_t i = 0; i + 1 < truth.frameTicks.size() && !collisionFound; ++i) {
        for (size_t j = i + 1; j < truth.frameTicks.size(); ++j) {
            if (truth.frameTicks[i] == truth.frameTicks[j]) continue;
            if (av_rescale_q(truth.frameTicks[i], tb, us) ==
                av_rescale_q(truth.frameTicks[j], tb, us)) {
                a = i;
                b = j;
                collisionFound = true;
                break;
            }
        }
    }
    CHECK(collisionFound);
    if (!collisionFound) {
        std::fprintf(stderr,
                     "FAIL: fixture has no distinct-tick same-microsecond pair; "
                     "the container did not preserve a fine timebase\n");
        return 1;
    }

    // Decode the real fixture through Demuxer + VideoDecoder.
    Demuxer demux;
    CHECK(demux.open(fixture));
    CHECK(demux.isOpen());
    const auto& info = demux.info();
    CHECK(info.videoIndex >= 0);
    CHECK(info.video != nullptr);
    // (c1) demuxer-exposed video stream timebase equals the container's
    CHECK(info.video->timebaseNum == truth.tbNum);
    CHECK(info.video->timebaseDen == truth.tbDen);

    VideoDecoder vdec;
    CHECK(vdec.open(demux.ctx(), info.videoIndex));
    // decoder packet timebase == container stream timebase
    CHECK(vdec.timebase().num == truth.tbNum);
    CHECK(vdec.timebase().den == truth.tbDen);

    struct Decoded {
        int64_t ptsTicks;
        int64_t ptsUs;
        int tbNum, tbDen, ptsSource;
        uint64_t frameIndex;
    };
    std::vector<Decoded> decoded;
    Packet pkt;
    auto drain = [&](DecodedVideoFrame& df) {
        while (vdec.receiveFrame(df)) {
            decoded.push_back({df.ptsTicks, df.ptsUs, df.tbNum, df.tbDen,
                               df.ptsSource, df.frameIndex});
        }
    };
    while (demux.readPacket(pkt)) {
        if (pkt.streamIndex != info.videoIndex) continue;
        vdec.sendPacket(pkt.av);
        DecodedVideoFrame df;
        drain(df);
    }
    vdec.sendPacket(nullptr); // flush delayed frames
    DecodedVideoFrame df;
    drain(df);

    // (a) every container tick is decoded verbatim, in order
    CHECK(decoded.size() == truth.frameTicks.size());
    if (decoded.size() != truth.frameTicks.size()) {
        std::fprintf(stderr,
                     "anvil_timestamp_tests: decode count %zu != container "
                     "frame count %zu; skipping frame-level comparisons\n",
                     decoded.size(), truth.frameTicks.size());
        return 1;
    }
    for (size_t i = 0; i < decoded.size() && i < truth.frameTicks.size(); ++i) {
        CHECK(decoded[i].ptsTicks == truth.frameTicks[i]);
        CHECK(decoded[i].frameIndex == i);
    }

    // (b) the colliding pair has DISTINCT native ticks but IDENTICAL ptsUs
    CHECK(decoded[a].ptsTicks != decoded[b].ptsTicks);
    CHECK(decoded[a].ptsUs == decoded[b].ptsUs);
    CHECK(decoded[a].ptsUs ==
          av_rescale_q(truth.frameTicks[a], tb, us)); // µs value itself derived

    // (c) decoded timebase provenance equals the container timebase
    for (const auto& f : decoded) {
        CHECK(f.tbNum == truth.tbNum);
        CHECK(f.tbDen == truth.tbDen);
    }

    // (d) displayed timestamp came from AVFrame::pts (fixture has real pts)
    for (const auto& f : decoded) CHECK(f.ptsSource == 1);

    // (e) exact native-tick selection distinguishes the colliding frames:
    // each tick selects exactly its own decode index, and the shared µs value
    // is incapable of doing so.
    std::vector<std::pair<int64_t, uint64_t>> candidates;
    for (const auto& f : decoded) candidates.emplace_back(f.ptsTicks, f.frameIndex);
    uint64_t selected = UINT64_MAX;
    CHECK(anvil::selectFrameByTicks(candidates, decoded[a].ptsTicks, selected) ==
          anvil::TickSelectionOutcome::Match);
    CHECK(selected == decoded[a].frameIndex);
    CHECK(anvil::selectFrameByTicks(candidates, decoded[b].ptsTicks, selected) ==
          anvil::TickSelectionOutcome::Match);
    CHECK(selected == decoded[b].frameIndex);
    CHECK(selected != decoded[a].frameIndex);
    // the µs domain cannot address either frame unambiguously — this is the
    // lossiness under repair
    size_t sameUs = 0;
    for (const auto& f : decoded)
        if (f.ptsUs == decoded[a].ptsUs) ++sameUs;
    CHECK(sameUs >= 2);

    if (g_failures == 0) {
        std::printf("anvil_timestamp_tests: OK (unit + fixture: ticks %lld/%lld "
                    "distinct at %d/%d, collide at %lld us)\n",
                    (long long)decoded[a].ptsTicks, (long long)decoded[b].ptsTicks,
                    truth.tbNum, truth.tbDen, (long long)decoded[a].ptsUs);
        return 0;
    }
    std::fprintf(stderr, "anvil_timestamp_tests: %d failure(s)\n", g_failures);
    return 1;
}
