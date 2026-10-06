// Core.cpp — ANVIL core type implementation.
#include "Core.hpp"

extern "C" {
#include <libavutil/frame.h>
#include <libavutil/pixfmt.h>
}

namespace anvil {

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

} // namespace anvil
