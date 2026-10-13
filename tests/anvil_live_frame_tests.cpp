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

    std::cout << "ANVIL live pre-FSR frame contract passed\n";
}
