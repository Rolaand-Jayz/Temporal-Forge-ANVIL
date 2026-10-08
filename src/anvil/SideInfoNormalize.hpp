// SideInfoNormalize.hpp — explicit side-information normalization stage.
//
// Capability D of the successor pack requires side-info normalization to be
// an independently replaceable/bypassable stage, not an internal step of the
// codec correspondence arm (review 4209763208). This stage owns the
// raw -> normalized-prior boundary:
//
//   raw codec side info (Observation::codecMotionVectors)
//     -> normalizeSideInfo() -> normalized prior (BlockMotion)
//     -> coarse correspondence
//
// Normalize delegates to Core's normalizeCodecMv(): ambiguous/precision
// semantics stay owned there and are unchanged. Bypass is a truthful control,
// not an algorithm switch: the raw side information is deliberately left
// uninterpreted, so the normalized prior is empty and a codec-arm consumer
// receives nothing, while the raw data on the observation and the
// estimate/oracle correspondence arms are untouched. A bypassed frame is
// never reported as normalized; the state field carries which of the three
// outcomes actually happened ("normalized" | "bypassed" | "not_applicable").
//
// The stage is a pure function: it never mutates the input, is deterministic,
// and performs no I/O.
#pragma once
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "Core.hpp"

namespace anvil {

// Control of the stage. Normalize produces the normalized prior; Bypass
// deliberately produces none.
enum class SideInfoNormalizationMode : uint8_t { Normalize = 0, Bypass = 1 };

// Stage identity for manifests/timing/capture consumers.
constexpr const char* kSideInfoNormalizationStageName =
    "side_info_normalization";

// Stable short names: "normalize" | "bypass".
const char* sideInfoNormalizationModeName(SideInfoNormalizationMode m);

struct SideInfoNormalizationResult {
    SideInfoNormalizationMode mode = SideInfoNormalizationMode::Normalize;
    // "normalized"   - raw entries were normalized into `normalized`
    // "bypassed"     - raw entries exist but were deliberately not interpreted
    // "not_applicable" - no raw entries (or an out-of-contract control value:
    //                    fail-closed refusal, see below)
    std::string state;
    // The normalized prior handed to coarse correspondence. Empty under
    // Bypass: the raw side info is intentionally not interpreted.
    std::vector<BlockMotion> normalized;
    // Raw entries the decoder exported this frame (mirrors
    // Observation::codecMotionVectors.size()). Reported in every mode so a
    // bypass stays auditable against the raw data, which is never dropped.
    size_t rawCount = 0;
    // Entries of `normalized` usable by downstream correspondence under the
    // consumer contract used by the runner: reference identity proven
    // (!ambiguous && refFrameIndex >= 0). 0 whenever the prior is empty.
    size_t usableCount = 0;
};

// Normalize (or truthfully bypass) the raw codec side information of `frame`.
// Deterministic and pure: same input + mode always yields an identical
// result; `frame` is never modified.
SideInfoNormalizationResult normalizeSideInfo(
    const Observation& frame, SideInfoNormalizationMode mode);

} // namespace anvil
