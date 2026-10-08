// SideInfoNormalize.cpp — side-information normalization stage implementation.
//
// Normalize delegates to normalizeCodecMv() (Core.cpp): the ambiguous /
// precision semantics of raw codec MVs keep a single owner and are not
// reimplemented here. Bypass is the truthful control required by pack
// capability D (review 4209763208): the raw side info is deliberately not
// interpreted, so the codec arm downstream receives nothing while
// estimate/oracle arms and the raw observation data stay untouched.
#include "SideInfoNormalize.hpp"

namespace anvil {

const char* sideInfoNormalizationModeName(SideInfoNormalizationMode m) {
    switch (m) {
        case SideInfoNormalizationMode::Normalize: return "normalize";
        case SideInfoNormalizationMode::Bypass: return "bypass";
    }
    return "?";
}

SideInfoNormalizationResult normalizeSideInfo(
    const Observation& frame, SideInfoNormalizationMode mode) {
    SideInfoNormalizationResult r;
    r.mode = mode;
    // Raw count is reported in every mode, including bypass: the raw side
    // information stays on the observation and must remain auditable.
    r.rawCount = frame.codecMotionVectors.size();

    if (mode == SideInfoNormalizationMode::Bypass) {
        r.state = r.rawCount > 0 ? "bypassed" : "not_applicable";
        // normalized stays empty; usableCount stays 0. Raw entries are NOT
        // reinterpreted, re-labeled, or moved into the prior.
        return r;
    }
    if (mode != SideInfoNormalizationMode::Normalize) {
        // Out-of-contract control value (invalid enum cast): fail closed.
        // No interpretation of the raw data, no normalized prior; rawCount
        // above keeps the audit trail. State cannot honestly claim
        // "normalized" or "bypassed", so it records that the stage did not
        // apply.
        r.state = "not_applicable";
        return r;
    }
    if (r.rawCount == 0) {
        r.state = "not_applicable";
        return r;
    }

    r.normalized = normalizeCodecMv(frame);
    // "Usable" mirrors the consumer contract applied by the runner
    // (Runner.cpp): a normalized entry is usable only when its reference
    // identity is proven. Raw exported vectors carry direction only, so this
    // is 0 today; it is computed, not hard-coded, so the stage keeps
    // reporting truthfully if proven-reference disambiguation is ever added
    // upstream of this stage.
    for (const BlockMotion& b : r.normalized)
        if (!b.ambiguous && b.refFrameIndex >= 0) ++r.usableCount;
    r.state = "normalized";
    return r;
}

} // namespace anvil
