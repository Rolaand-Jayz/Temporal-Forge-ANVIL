// Shared source-frame adapter for offline ANVIL and the opt-in live player.
// No pixel resampling, fabricated motion, or timestamp rounding.
#pragma once

#include "anvil/Core.hpp"
#include "media/VideoDecoder.hpp"

namespace anvil {
inline Observation observationFromDecoded(const temporal_forge::DecodedVideoFrame& d) {
    Observation o;
    o.width = d.width;
    o.height = d.height;
    o.avPixelFormat = d.avFormat;
    o.color.range = d.colorRange;
    o.color.matrix = d.colorSpace;
    o.color.transfer = d.colorTransfer;
    o.color.primaries = d.colorPrimaries;
    o.color.chromaLocation = d.chromaLocation;
    o.color.pixelFormat = d.avFormat;
    o.color.bitDepth = d.bitDepth;
    o.color.hasMasteringDisplay = d.hasMasteringDisplay;
    o.color.hasContentLightLevel = d.hasContentLightLevel;
    o.ptsUs = d.ptsUs;
    o.ptsTicks = d.ptsTicks;
    o.tbNum = d.tbNum;
    o.tbDen = d.tbDen;
    o.ptsSource = d.ptsSource;
    o.frameIndex = d.frameIndex;
    o.keyframe = d.keyframe;
    o.bFrame = d.bFrame;
    o.planeCount = d.planes;
    for (int p = 0; p < d.planes && p < 4; ++p) {
        o.plane[p] = d.plane[p];
        o.linesize[p] = d.linesize[p];
    }
    o.codecMotionVectors.reserve(d.motionVectors.size());
    for (const auto& mv : d.motionVectors) {
        Observation::RawMv r;
        r.dstX = mv.dstX;
        r.dstY = mv.dstY;
        r.mvX = mv.mvX;
        r.mvY = mv.mvY;
        r.w = mv.w;
        r.h = mv.h;
        r.source = mv.source;
        r.motionScale = mv.motionScale;
        o.codecMotionVectors.push_back(r);
    }
    return o;
}
} // namespace anvil
