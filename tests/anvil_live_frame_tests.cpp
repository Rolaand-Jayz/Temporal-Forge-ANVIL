// Exercise the real decoded-frame to ANVIL temporal stage, without GPU or Qt.
#include "anvil/LiveFrameProcessor.hpp"
#include <libavutil/pixfmt.h>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#define REQUIRE(cond) do { if (!(cond)) { \
    std::cerr << "CHECK FAILED: " << #cond << " at " << __LINE__ << "\n"; \
    return EXIT_FAILURE; } } while (false)
#include <cmath>
#include <cstdint>
#include <iostream>

using namespace anvil;
using temporal_forge::DecodedVideoFrame;

static DecodedVideoFrame frame(uint64_t index, int width=32, int height=32) {
    DecodedVideoFrame d;
    d.width=width; d.height=height;
    d.avFormat=AV_PIX_FMT_YUV420P;
    d.colorRange=AVCOL_RANGE_MPEG;
    d.colorSpace=AVCOL_SPC_BT709;
    d.colorPrimaries=AVCOL_PRI_BT709;
    d.colorTransfer=AVCOL_TRC_BT709;
    d.bitDepth=8; d.planes=3;
    d.frameIndex=index;
    d.ptsUs=static_cast<int64_t>(index)*33'333;
    d.ptsTicks=static_cast<int64_t>(index);
    d.tbNum=1; d.tbDen=30; d.ptsSource=1;
    for(int p=0;p<3;++p) {
        const int w=p==0?width:width/2;
        const int h=p==0?height:height/2;
        d.linesize[p]=w;
        d.plane[p].resize(static_cast<size_t>(w)*h);
        for(int y=0;y<h;++y)
            for(int x=0;x<w;++x)
                d.plane[p][static_cast<size_t>(y)*w+x] =
                    static_cast<uint8_t>(p==0 ? 48+((x*7+y*3)%100) : 128);
    }
    return d;
}
int main(int argc, char** argv) {
    // The controlled falsifier must exit nonzero even with NDEBUG enabled.
    if (argc > 1 && std::strcmp(argv[1], "--negative-control") == 0)
        REQUIRE(false);

    // No source mutation on initial frame or unsupported formats.
    LiveFrameProcessor cpu(20'000.0);
    auto a=frame(0);
    const auto aRaw=a.plane[0];
    const auto aResult=cpu.process(a, false);
    REQUIRE(aResult.status == LiveFrameStatus::Primed);
    REQUIRE(a.plane[0] == aRaw);

    auto b=frame(1);
    const auto bPts=b.ptsUs;
    const auto bTicks=b.ptsTicks;
    const auto bIndex=b.frameIndex;
    const auto bRaw=b.plane[0];
    auto bResult=cpu.process(b, false);
    REQUIRE(bResult.status == LiveFrameStatus::Reconstructed);
    REQUIRE(bResult.usableSamples>0);
    REQUIRE(b.ptsUs==bPts && b.ptsTicks==bTicks && b.frameIndex==bIndex);
    REQUIRE(b.width==32 && b.height==32 && b.planes==3);
    REQUIRE(b.plane[0]==bRaw); // identical source observations remain identical.

    // A PTS discontinuity cannot reuse stale history.
    auto c=frame(2);
    c.ptsUs=b.ptsUs;
    REQUIRE(cpu.process(c,false).status==LiveFrameStatus::Discontinuity);
    auto d=frame(3);
    REQUIRE(cpu.process(d,true).status==LiveFrameStatus::Primed);

    // Decoder-provided GPU frames are never copied or rewritten by CPU stage.
    auto hw=frame(4);
    hw.hwFrame=true;
    auto original=hw.plane[0];
    REQUIRE(cpu.process(hw,false).status==LiveFrameStatus::Unsupported);
    REQUIRE(hw.plane[0]==original);

    // Malformed layout is rejected, not overread.
    auto invalid=frame(5);
    invalid.plane[0].resize(20);
    REQUIRE(cpu.process(invalid,false).status==LiveFrameStatus::Unsupported);
    REQUIRE(invalid.plane[0].size()==20);

    // Unsupported changes clear raw history; next valid sample primes anew.
    auto after=frame(6);
    REQUIRE(cpu.process(after,false).status==LiveFrameStatus::Primed);

    // Stage missed deadline: preserve unmodified decoder pixels and disable
    // further ANVIL work for this session, even across seek/reset.
    LiveFrameProcessor slow(1e-9);
    auto slowFrame=frame(0);
    auto slowPixels=slowFrame.plane[0];
    const auto slowResult=slow.process(slowFrame,false);
    REQUIRE(slowResult.status==LiveFrameStatus::OverBudget);
    REQUIRE(slowFrame.plane[0]==slowPixels);
    REQUIRE(slow.disabledForBudget());
    slow.reset();
    REQUIRE(slow.disabledForBudget());
    REQUIRE(slow.process(slowFrame,false).status==LiveFrameStatus::OverBudget);

    // --- PR8-F0001 regression: the UI's stage surface must distinguish
    // decoder-owned pixels from post-ANVIL pixels with DISTINCT content, so
    // a native compare pane can never label reconstructed pixels SOURCE.
    // QML binds to the stageName() tokens; keep them stable.
    REQUIRE(std::strcmp(LiveStageTracker::stageName(LiveUserStage::Off), "off") == 0);
    REQUIRE(std::strcmp(LiveStageTracker::stageName(LiveUserStage::Standby), "standby") == 0);
    REQUIRE(std::strcmp(LiveStageTracker::stageName(LiveUserStage::Reconstructing), "reconstructing") == 0);
    REQUIRE(std::strcmp(LiveStageTracker::stageName(LiveUserStage::Bypassed), "bypassed") == 0);

    LiveStageTracker disabledTracker(false);
    REQUIRE(disabledTracker.stage()==LiveUserStage::Off);
    disabledTracker.observe(LiveFrameStatus::Reconstructed); // never happens when off
    REQUIRE(disabledTracker.stage()==LiveUserStage::Off);

    // A moving high-contrast diagonal: consecutive frames have genuinely
    // different pixels, unlike the fixed pattern above.
    const auto moving = [](uint64_t index) {
        DecodedVideoFrame d = frame(index);
        const int w = d.width;
        for(int y=0;y<d.height;++y)
            for(int x=0;x<w;++x)
                d.plane[0][static_cast<size_t>(y)*w+x] =
                    static_cast<uint8_t>(((x + y + static_cast<int>(index)*8) % 64 < 32) ? 200 : 40);
        return d;
    };

    LiveFrameProcessor distinct(20'000.0);
    LiveStageTracker tracker(true);
    REQUIRE(tracker.stage()==LiveUserStage::Standby);
    auto m0=moving(0);
    const auto m0Result=distinct.process(m0,false);
    REQUIRE(m0Result.status==LiveFrameStatus::Primed);
    tracker.observe(m0Result.status);
    REQUIRE(tracker.stage()==LiveUserStage::Standby); // priming keeps SOURCE truthful

    auto m1=moving(1);
    const auto m1Luma=m1.plane[0]; // the decoder's own pixels
    const auto m1Result=distinct.process(m1,false);
    REQUIRE(m1Result.status==LiveFrameStatus::Reconstructed);
    tracker.observe(m1Result.status);
    REQUIRE(tracker.stage()==LiveUserStage::Reconstructing);
    // The pane pixels are post-ANVIL: a bare SOURCE label would be false.
    // If reconstruction ever becomes pixel-preserving for distinct content,
    // this fails and the UI contract must be re-examined, not silenced.
    REQUIRE(m1.plane[0]!=m1Luma);

    // Per-frame bypass reasons return pixel ownership to the decoder for
    // that frame; the stage must say so instead of coasting on the last
    // success. A discontinuity replaces (not clears) history, so the next
    // in-sequence frame resumes reconstruction.
    auto m2=moving(2);
    m2.ptsUs=m1.ptsUs; // force a discontinuity
    const auto m2Result=distinct.process(m2,false);
    REQUIRE(m2Result.status==LiveFrameStatus::Discontinuity);
    tracker.observe(m2Result.status);
    REQUIRE(tracker.stage()==LiveUserStage::Standby);
    auto m3=moving(3);
    const auto m3Original=moving(3);
    const auto m3Result=distinct.process(m3,false);
    REQUIRE(m3Result.status==LiveFrameStatus::Reconstructed);
    tracker.observe(m3Result.status);
    REQUIRE(tracker.stage()==LiveUserStage::Reconstructing);
    // Still replacing pixels after the resume: compare against an
    // unprocessed twin of the same frame.
    REQUIRE(m3.plane[0]!=m3Original.plane[0]);
    // A seek (explicit history reset) primes again: decoder-owned frame,
    // stage honestly back to standby.
    auto m5=moving(5);
    const auto m5Result=distinct.process(m5,true);
    REQUIRE(m5Result.status==LiveFrameStatus::Primed);
    tracker.observe(m5Result.status);
    REQUIRE(tracker.stage()==LiveUserStage::Standby);

    // Budget bypass returns ownership of the pixels to the decoder and the
    // status surface must say so instead of silently reverting to "source".
    LiveFrameProcessor budget(1e-9);
    LiveStageTracker budgetTracker(true);
    auto b0=moving(3);
    const auto b0Luma=b0.plane[0];
    const auto b0Result=budget.process(b0,false);
    REQUIRE(b0Result.status==LiveFrameStatus::OverBudget);
    REQUIRE(b0.plane[0]==b0Luma); // decoder pixels preserved: SOURCE truthful again
    budgetTracker.observe(b0Result.status);
    REQUIRE(budgetTracker.stage()==LiveUserStage::Bypassed);
    budgetTracker.observe(LiveFrameStatus::Reconstructed); // stage is dead; cannot resurrect
    REQUIRE(budgetTracker.stage()==LiveUserStage::Bypassed);

    // Publication-order ownership: delayed/failed GPUs do not change the
    // provenance of a reconstructed image still displayed by the player.
    LivePublicationTracker published(true);
    REQUIRE(published.stage()==LiveUserStage::Standby);
    published.submit(0, LiveUserStage::Reconstructing);
    REQUIRE(published.stage()==LiveUserStage::Standby); // GPU pending
    REQUIRE(published.retire(0, true)==LiveUserStage::Reconstructing);
    published.submit(1, LiveUserStage::Standby); // unsupported/discontinuity
    REQUIRE(published.stage()==LiveUserStage::Reconstructing);
    REQUIRE(published.retire(1, false)==LiveUserStage::Reconstructing); // failed
    published.submit(1, LiveUserStage::Bypassed); // budget miss
    REQUIRE(published.stage()==LiveUserStage::Reconstructing);
    REQUIRE(published.retire(1, false)==LiveUserStage::Reconstructing);
    published.submit(1, LiveUserStage::Bypassed);
    REQUIRE(published.retire(1, true)==LiveUserStage::Bypassed);
    published.submit(0, LiveUserStage::Reconstructing);
    REQUIRE(published.retire(0, false)==LiveUserStage::Bypassed); // scaler fail
    REQUIRE(published.publishDirect(LiveUserStage::Standby)==LiveUserStage::Standby);
    REQUIRE(published.publishDirect(LiveUserStage::Reconstructing)==LiveUserStage::Reconstructing);
    published.submit(0, LiveUserStage::Standby);
    published.clearPending(); // teardown/reconfigure
    REQUIRE(published.retire(0, true)==LiveUserStage::Reconstructing);
    std::cout << "ANVIL live pre-FSR frame contract passed\n";
}
