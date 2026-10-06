// anvil_codec_tests.cpp — codec side-information capability tests.
//
// The probe MEASURES encoder/decoder/MV-export reality for H.264, HEVC, and
// AV1 in this FFmpeg build. Assertions enforce honesty of the capability
// record, not any particular capability outcome: a codec may legitimately be
// unsupported, provided the record says so explicitly.
#include <cstdio>
#include <string>

extern "C" {
#include <libavcodec/avcodec.h>
}

#include "anvil/CodecProbe.hpp"

using namespace anvil;

static int failures = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

int main() {
    auto caps = probeCodecCapabilities();
    CHECK(caps.size() == 3);

    bool sawH264 = false, sawHevc = false, sawAv1 = false;
    for (const auto& c : caps) {
        CHECK(!c.codec.empty());
        CHECK(!c.note.empty() || c.mvExportProven);
        // Truthfulness invariant: mv_export_proven=true requires an observed
        // probe frame carrying MV side data.
        if (c.mvExportProven) CHECK(c.probeMvFrames > 0 && c.probeTotalFrames > 0);
        if (c.mvExportProven)
            CHECK(c.probeMvFrames <= c.probeTotalFrames);
        if (c.codec == "h264") sawH264 = true;
        if (c.codec == "hevc") sawHevc = true;
        if (c.codec == "av1") sawAv1 = true;
        std::printf("codec=%s encoder=%s decoder=%d mv_export=%d (%d/%d frames) note=%s\n",
                    c.codec.c_str(), c.encoder.c_str(), int(c.decoderAvailable),
                    int(c.mvExportProven), c.probeMvFrames, c.probeTotalFrames,
                    c.note.c_str());
    }
    CHECK(sawH264 && sawHevc && sawAv1);

    // H.264 software decode with AV_CODEC_FLAG2_EXPORT_MVS is a documented
    // FFmpeg export path; if the build ships an h264 decoder, MV export is
    // expected to be proven. This is the contract's "proven or marked
    // unsupported" — a failure here means the capability record lied.
    for (const auto& c : caps) {
        if (c.codec == "h264" && c.decoderAvailable && !c.encoderAvailable) {
            // no encoder: record must say so; skip expectation
            continue;
        }
        if (c.codec == "h264" && c.decoderAvailable && c.encoderAvailable) {
            CHECK(c.mvExportProven);
        }
    }

    if (failures) {
        std::printf("%d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("anvil_codec_tests: all checks passed\n");
    return 0;
}
