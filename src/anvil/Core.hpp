// Core.hpp — ANVIL successor core types: observation, correspondence,
// confidence/visibility, sample geometry, temporal window.
//
// Guardrails implemented here:
// - Missing side information is represented explicitly (SideInfoState), never
//   fabricated.
// - Confidence is a backend-neutral C(x) in [0,1] with a known/unknown state;
//   visibility is separate and never conflated with confidence.
// - Sample geometry (sampling-grid phase) is known | estimated | unknown.
//   No artificial jitter is synthesized anywhere.
// - Correspondence preserves provenance: codec_mv vs oracle vs image_estimate.
//   An image-estimated vector is NEVER relabeled as a codec MV.
#pragma once
#include <cstdint>
#include <optional>
#include <string>
#include <vector>

#include "ColorMeta.hpp"

namespace anvil {

// One decoded observation frame: source-format planes copied verbatim plus
// all decode-time metadata. Planes are the decoder's pixel layout — the
// successor does not resample on ingest (artificial resampling is not new
// source information).
struct Observation {
    int width = 0;
    int height = 0;
    int avPixelFormat = AV_PIX_FMT_NONE;
    ColorMeta color;

    int64_t ptsUs = -1;
    int64_t ptsTicks = -1;
    uint64_t frameIndex = 0;   // monotonic decode counter
    bool keyframe = false;
    bool bFrame = false;

    std::vector<uint8_t> plane[4];
    int linesize[4] = {0, 0, 0, 0};
    int planeCount = 0;

    // Raw codec-exported motion vectors as delivered by the decoder (empty
    // when the codec/decoder did not export them). Successor code must not
    // invent entries here.
    struct RawMv {
        int16_t dstX = 0;
        int16_t dstY = 0;
        float mvX = 0.0f;
        float mvY = 0.0f;
        uint8_t w = 0;
        uint8_t h = 0;
        int8_t source = 0; // <0 past ref, >0 future ref
        // The decoder's motion_scale divisor (motion_x / motion_scale =
        // source pixels). Carried verbatim so precision classification is
        // justified by actual metadata instead of assumed.
        int32_t motionScale = 0;
    };
    std::vector<RawMv> codecMotionVectors;
};

// --- Correspondence ---------------------------------------------------------

// Direction of a correspondence relative to the frame it was exported from.
enum class RefDirection : uint8_t { Past = 0, Future = 1 };

enum class MotionPrecision : uint8_t {
    Unknown = 0, Integer = 1, HalfPel = 2, QuarterPel = 3, SubQuarter = 4
};

// Provenance of a correspondence entry. This is a load-bearing distinction:
// codec-encoded MVs, oracle-provided ground truth, and image-estimated flow
// are never merged silently.
enum class CorrespondenceSource : uint8_t {
    CodecMv = 0,       // exported from the encoded bitstream
    Oracle = 1,        // injected ground truth for controlled experiments
    ImageEstimate = 2, // computed by a deterministic image estimator
};

// Normalized block correspondence between observation frame `refFrameIndex`
// and the frame it was exported from (dst block at (dstX,dstY)).
struct BlockMotion {
    uint64_t frameIndex = 0;    // frame the vector belongs to
    int64_t refFrameIndex = -1; // referenced frame (decode counter); -1 unknown
    int temporalDistance = 0;   // signed frames: ref - frame (0 unknown)
    RefDirection direction = RefDirection::Past;

    int16_t dstX = 0, dstY = 0;
    uint16_t blockW = 0, blockH = 0;
    float mvX = 0.0f, mvY = 0.0f; // ref-position delta in source pixels

    MotionPrecision precision = MotionPrecision::Unknown;
    char frameType = '?';      // I/P/B as reported, '?' unknown
    bool intra = false;
    bool skip = false;
    bool ambiguous = false;    // true when reference identity could not be proven

    CorrespondenceSource source = CorrespondenceSource::CodecMv;
};

// Explicit per-frame state of codec side information.
enum class SideInfoState : uint8_t {
    Available = 0,     // codec exported usable MV side data this frame
    Unsupported = 1,   // codec/decoder cannot export MVs (proven capability)
    Ambiguous = 2,     // data present but reference identity unprovable
    EstimatorOnly = 3, // no codec data; only an image estimate is allowed
};

const char* sideInfoStateName(SideInfoState s);

// Maps the decoder's AVMotionVector::motion_scale divisor to the declared
// precision: 1 = Integer, 2 = HalfPel, 4 = QuarterPel, 8/16/32 = SubQuarter.
// Scale 0 or unmappable values yield Unknown — precision is only claimed
// when actual metadata justifies it (review 4202855070).
MotionPrecision motionPrecisionFromScale(int32_t motionScale);

// Normalizes raw codec MVs of frame f into BlockMotion entries with provenance
// CodecMv. Reference frame identity is NOT provable from the exported side
// data (direction only), so every entry is marked ambiguous unless the
// reference index can be proven by other means; ambiguous vectors are never
// applied by the accumulator.
std::vector<BlockMotion> normalizeCodecMv(const Observation& f);

// --- Confidence / visibility ------------------------------------------------

// Backend-neutral confidence value with an explicit known/unknown state.
struct Confidence {
    float value = 0.0f;        // [0,1] when state == Known
    bool known = false;
    static Confidence unknown() { return {}; }
    static Confidence withValue(float v) { return {v, true}; }
};

// Visibility is a separate per-sample validity signal (e.g. occlusion or
// out-of-bounds after warp). It is NOT confidence, NOT a codec residual, and
// NOT any FSR mask.
enum class Visibility : uint8_t {
    Invalid = 0,
    Valid = 1,
    Unknown = 2,
};

// --- Sample geometry ---------------------------------------------------------

enum class SampleGeometryState : uint8_t {
    Unknown = 0,   // sampling-grid phase not known; no estimate forced
    Estimated = 1, // estimated by a deterministic method, labeled as estimate
    Known = 2,     // provided (oracle / controlled acquisition)
};

struct SampleGeometry {
    SampleGeometryState state = SampleGeometryState::Unknown;
    // Fractional sampling-grid offsets in pixels, valid only when state !=
    // Unknown. Never synthesized from motion vectors (fract(mean(mv)) is not
    // a valid phase estimator).
    float phaseX = 0.0f, phaseY = 0.0f;
};

const char* sampleGeometryStateName(SampleGeometryState s);

// --- Temporal window ---------------------------------------------------------

// Past/future temporal window around a target frame. {0,0} is the
// single-frame control.
struct WindowConfig {
    int past = 0;   // number of past frames included (>= 0)
    int future = 0; // number of future frames included (>= 0)

    // Frame decode-counter indices for target t, ordered ascending,
    // target included. Negative indices are dropped (stream start).
    std::vector<uint64_t> windowFor(uint64_t target) const;
};

} // namespace anvil
