// Core.cpp — ANVIL core type implementation.
#include "Core.hpp"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

namespace anvil {

MotionPrecision motionPrecisionFromScale(int32_t motionScale) {
    switch (motionScale) {
        case 1: return MotionPrecision::Integer;
        case 2: return MotionPrecision::HalfPel;
        case 4: return MotionPrecision::QuarterPel;
        case 8: case 16: case 32: return MotionPrecision::SubQuarter;
        default: return MotionPrecision::Unknown;
    }
}

const char* sideInfoStateName(SideInfoState s) {
    switch (s) {
        case SideInfoState::Available: return "available";
        case SideInfoState::Unsupported: return "unsupported";
        case SideInfoState::Ambiguous: return "ambiguous";
        case SideInfoState::EstimatorOnly: return "estimator_only";
    }
    return "?";
}

const char* sampleGeometryStateName(SampleGeometryState s) {
    switch (s) {
        case SampleGeometryState::Unknown: return "unknown";
        case SampleGeometryState::Estimated: return "estimated";
        case SampleGeometryState::Known: return "known";
    }
    return "?";
}

std::vector<uint64_t> WindowConfig::windowFor(uint64_t target) const {
    std::vector<uint64_t> out;
    const int64_t t = static_cast<int64_t>(target);
    for (int d = -past; d <= future; ++d) {
        const int64_t idx = t + d;
        if (idx >= 0) out.push_back(static_cast<uint64_t>(idx));
    }
    return out;
}

std::vector<BlockMotion> normalizeCodecMv(const Observation& f) {
    std::vector<BlockMotion> out;
    out.reserve(f.codecMotionVectors.size());
    for (const Observation::RawMv& r : f.codecMotionVectors) {
        BlockMotion b;
        b.frameIndex = f.frameIndex;
        b.refFrameIndex = -1; // not provable from side data
        b.temporalDistance = 0;
        b.direction = r.source < 0 ? RefDirection::Past : RefDirection::Future;
        b.dstX = r.dstX;
        b.dstY = r.dstY;
        b.blockW = r.w;
        b.blockH = r.h;
        b.mvX = r.mvX;
        b.mvY = r.mvY;
        b.precision = motionPrecisionFromScale(r.motionScale);
        b.ambiguous = true;
        b.source = CorrespondenceSource::CodecMv;
        out.push_back(b);
    }
    return out;
}

} // namespace anvil
