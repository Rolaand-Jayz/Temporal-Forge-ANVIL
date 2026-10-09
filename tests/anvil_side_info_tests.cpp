// anvil_side_info_tests.cpp — side-information normalization stage contract
// (review 4209763208 / pack capability D: replaceable, bypassable stage).
//
// The stage under test delegates normalization to Core's normalizeCodecMv()
// and adds a truthful bypass control. These tests pin: delegation identity,
// counts, precision/direction/ambiguity semantics, purity, determinism, and
// the isolation property that normalize vs bypass differ only in whether a
// normalized prior is produced.
#include <cassert>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

#include "anvil/Core.hpp"
#include "anvil/SideInfoNormalize.hpp"

using namespace anvil;

static int failures = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

// --- fixtures ---------------------------------------------------------------

static Observation makeFrame(uint64_t idx) {
    Observation o;
    o.width = 64;
    o.height = 64;
    o.avPixelFormat = AV_PIX_FMT_YUV420P;
    o.ptsUs = int64_t(idx) * 33367;
    o.ptsTicks = int64_t(idx) * 3003;
    o.frameIndex = idx;
    o.keyframe = (idx % 10 == 0);
    o.bFrame = (idx % 3 == 2);
    o.planeCount = 1;
    o.plane[0].assign(size_t(64) * 64, uint8_t(idx & 0xffu));
    o.linesize[0] = 64;
    o.color.pixelFormat = AV_PIX_FMT_YUV420P;
    o.color.bitDepth = 8;
    return o;
}

// Hand-crafted raw MV side data varying every field: dst geometry (in-frame,
// negative, far out-of-frame), motion values and signs, source direction
// sign, and motion scales 1/2/4/8/0/16/32. Field order mirrors
// Observation::RawMv (Core.hpp): dstX, dstY, mvX, mvY, w, h, source, scale.
static std::vector<Observation::RawMv> makeRawSet() {
    return {
        {0,     0,    1.5f,   -2.0f,  16,  16, -1, 4},  // quarter-pel, past
        {16,    0,    0.5f,    0.0f,  16,  16,  1, 2},  // half-pel, future
        {32,    16,  -3.25f,   4.75f,  8,   8, -1, 1},  // integer, negative mv
        {48,    48,   0.0f,    0.0f,  16,  16,  1, 8},  // sub-quarter, zero mv
        {100,  100,  12.0f,   -7.5f,   4,   4, -1, 0},  // out-of-frame, scale 0
        {-40,   60,   0.25f,   0.125f, 32,  32,  1, 16},// negative dst, scale 16
        {2000, 3000, 100.0f, -100.0f, 255, 255, -1, 32},// far out-of-frame
        {0,     0,    0.0f,    0.0f,   0,   0,  0, 0},  // degenerate, source 0
    };
}

// --- comparison helpers (field-exact, no float tolerance: pure transport) ---

static bool sameBlockMotion(const BlockMotion& a, const BlockMotion& b) {
    return a.frameIndex == b.frameIndex
        && a.refFrameIndex == b.refFrameIndex
        && a.temporalDistance == b.temporalDistance
        && a.direction == b.direction
        && a.dstX == b.dstX && a.dstY == b.dstY
        && a.blockW == b.blockW && a.blockH == b.blockH
        && a.mvX == b.mvX && a.mvY == b.mvY
        && a.precision == b.precision
        && a.frameType == b.frameType
        && a.intra == b.intra && a.skip == b.skip
        && a.ambiguous == b.ambiguous
        && a.source == b.source;
}

static bool deepEqualObservation(const Observation& a, const Observation& b) {
    if (a.width != b.width || a.height != b.height) return false;
    if (a.avPixelFormat != b.avPixelFormat) return false;
    if (a.ptsUs != b.ptsUs || a.ptsTicks != b.ptsTicks) return false;
    if (a.tbNum != b.tbNum || a.tbDen != b.tbDen
        || a.ptsSource != b.ptsSource) return false;
    if (a.frameIndex != b.frameIndex) return false;
    if (a.keyframe != b.keyframe || a.bFrame != b.bFrame) return false;
    if (a.planeCount != b.planeCount) return false;
    for (int p = 0; p < 4; ++p) {
        if (a.plane[p] != b.plane[p]) return false;
        if (a.linesize[p] != b.linesize[p]) return false;
    }
    if (a.codecMotionVectors.size() != b.codecMotionVectors.size()) return false;
    for (size_t i = 0; i < a.codecMotionVectors.size(); ++i) {
        const Observation::RawMv& x = a.codecMotionVectors[i];
        const Observation::RawMv& y = b.codecMotionVectors[i];
        if (x.dstX != y.dstX || x.dstY != y.dstY) return false;
        if (x.mvX != y.mvX || x.mvY != y.mvY) return false;
        if (x.w != y.w || x.h != y.h) return false;
        if (x.source != y.source || x.motionScale != y.motionScale) return false;
    }
    if (a.color.range != b.color.range
        || a.color.primaries != b.color.primaries
        || a.color.transfer != b.color.transfer
        || a.color.matrix != b.color.matrix
        || a.color.chromaLocation != b.color.chromaLocation
        || a.color.pixelFormat != b.color.pixelFormat
        || a.color.bitDepth != b.color.bitDepth
        || a.color.hasMasteringDisplay != b.color.hasMasteringDisplay
        || a.color.hasContentLightLevel != b.color.hasContentLightLevel)
        return false;
    return true;
}

static bool sameNormalizationResult(const SideInfoNormalizationResult& a,
                                    const SideInfoNormalizationResult& b) {
    if (a.mode != b.mode || a.state != b.state) return false;
    if (a.rawCount != b.rawCount || a.usableCount != b.usableCount) return false;
    if (a.normalized.size() != b.normalized.size()) return false;
    for (size_t i = 0; i < a.normalized.size(); ++i)
        if (!sameBlockMotion(a.normalized[i], b.normalized[i])) return false;
    return true;
}

// Recount of the documented consumer contract: a normalized entry is usable
// only when its reference identity is proven.
static size_t usableUnderConsumerContract(
    const std::vector<BlockMotion>& prior) {
    size_t usable = 0;
    for (const BlockMotion& b : prior)
        if (!b.ambiguous && b.refFrameIndex >= 0) ++usable;
    return usable;
}

// --- 1. normalize: delegation identity, counts, precision, ambiguity -------

static void testNormalizeDelegatesAndCountsEntries() {
    Observation f = makeFrame(7);
    f.codecMotionVectors = makeRawSet();
    const size_t n = f.codecMotionVectors.size();

    const auto res = normalizeSideInfo(f, SideInfoNormalizationMode::Normalize);

    CHECK(res.mode == SideInfoNormalizationMode::Normalize);
    CHECK(std::string(sideInfoNormalizationModeName(res.mode)) == "normalize");
    CHECK(res.state == "normalized");
    CHECK(res.rawCount == n);
    CHECK(res.normalized.size() == n); // 1:1, out-of-frame entries included

    // Identical to a direct normalizeCodecMv call on the same input.
    const auto direct = normalizeCodecMv(f);
    CHECK(direct.size() == n);
    for (size_t i = 0; i < n; ++i)
        CHECK(sameBlockMotion(res.normalized[i], direct[i]));

    for (const BlockMotion& b : res.normalized) {
        CHECK(b.ambiguous); // reference identity stays unproven: unchanged
        CHECK(b.source == CorrespondenceSource::CodecMv);
        CHECK(b.frameIndex == 7);
        CHECK(b.refFrameIndex == -1);
    }

    // Precision mapping is carried per entry via motionPrecisionFromScale.
    CHECK(res.normalized[0].precision == MotionPrecision::QuarterPel);  // 4
    CHECK(res.normalized[1].precision == MotionPrecision::HalfPel);     // 2
    CHECK(res.normalized[2].precision == MotionPrecision::Integer);     // 1
    CHECK(res.normalized[3].precision == MotionPrecision::SubQuarter);  // 8
    CHECK(res.normalized[4].precision == MotionPrecision::Unknown);     // 0
    CHECK(res.normalized[5].precision == MotionPrecision::SubQuarter);  // 16
    CHECK(res.normalized[6].precision == MotionPrecision::SubQuarter);  // 32
    CHECK(res.normalized[7].precision == MotionPrecision::Unknown);     // 0

    // Direction follows the raw source sign; verbatim transport of geometry
    // and motion, including out-of-frame coordinates.
    CHECK(res.normalized[0].direction == RefDirection::Past);
    CHECK(res.normalized[1].direction == RefDirection::Future);
    CHECK(res.normalized[2].direction == RefDirection::Past);
    CHECK(res.normalized[3].direction == RefDirection::Future);
    CHECK(res.normalized[4].dstX == 100 && res.normalized[4].dstY == 100);
    CHECK(res.normalized[5].dstX == -40 && res.normalized[5].dstY == 60);
    CHECK(res.normalized[6].dstX == 2000 && res.normalized[6].dstY == 3000);
    CHECK(res.normalized[6].blockW == 255 && res.normalized[6].blockH == 255);
    CHECK(res.normalized[6].mvX == 100.0f && res.normalized[6].mvY == -100.0f);

    // usableCount correct under the documented consumer contract.
    CHECK(res.usableCount == usableUnderConsumerContract(res.normalized));
    CHECK(res.usableCount == 0); // raw export carries no proven ref identity
}

// --- 2. bypass: no prior, rawCount preserved -------------------------------

static void testBypassProducesNoNormalizedPrior() {
    Observation f = makeFrame(3);
    f.codecMotionVectors = makeRawSet();
    const size_t n = f.codecMotionVectors.size();

    const auto res = normalizeSideInfo(f, SideInfoNormalizationMode::Bypass);

    CHECK(res.mode == SideInfoNormalizationMode::Bypass);
    CHECK(std::string(sideInfoNormalizationModeName(res.mode)) == "bypass");
    CHECK(res.state == "bypassed");
    CHECK(res.normalized.empty()); // raw side info deliberately not interpreted
    CHECK(res.rawCount == n);      // raw evidence still reported for audit
    CHECK(res.usableCount == 0);
}

// --- 3. empty raw side info: not_applicable in both modes ------------------

static void testEmptyRawSideInfoIsNotApplicable() {
    Observation f = makeFrame(5);
    CHECK(f.codecMotionVectors.empty());
    for (const auto mode : {SideInfoNormalizationMode::Normalize,
                            SideInfoNormalizationMode::Bypass}) {
        const auto res = normalizeSideInfo(f, mode);
        CHECK(res.state == "not_applicable");
        CHECK(res.normalized.empty());
        CHECK(res.rawCount == 0);
        CHECK(res.usableCount == 0);
    }
}

// --- 4. purity: the input observation is never mutated ----------------------

static void testStageNeverMutatesItsInput() {
    Observation f = makeFrame(9);
    f.codecMotionVectors = makeRawSet();
    const Observation before = f; // deep copy
    for (const auto mode : {SideInfoNormalizationMode::Normalize,
                            SideInfoNormalizationMode::Bypass}) {
        const auto res = normalizeSideInfo(f, mode);
        (void)res;
        CHECK(deepEqualObservation(f, before));
    }
}

// --- 5. determinism: identical results across repeated invocations ----------

static void testStageIsDeterministic() {
    for (uint64_t idx = 0; idx < 4; ++idx) {
        Observation f = makeFrame(11 + idx);
        const auto raw = makeRawSet();
        f.codecMotionVectors.assign(raw.begin(),
                                    raw.begin() + int(idx + 1)); // 1..4 entries
        // Vary the scale across Unknown/Integer/Half/Quarter on the first
        // entry.
        static const int32_t kScales[] = {0, 1, 2, 4};
        f.codecMotionVectors[0].motionScale = kScales[idx];
        for (const auto mode : {SideInfoNormalizationMode::Normalize,
                                SideInfoNormalizationMode::Bypass}) {
            const auto a = normalizeSideInfo(f, mode);
            const auto b = normalizeSideInfo(f, mode);
            CHECK(sameNormalizationResult(a, b));
        }
    }
}

// --- 6. isolation: modes differ only in normalized-prior presence -----------

static void testBypassDiffersOnlyInNormalizedPriorPresence() {
    Observation f = makeFrame(13);
    f.codecMotionVectors = makeRawSet();
    const Observation untouched = f;

    const auto norm = normalizeSideInfo(f, SideInfoNormalizationMode::Normalize);
    const auto pass = normalizeSideInfo(f, SideInfoNormalizationMode::Bypass);

    CHECK(norm.rawCount == pass.rawCount); // identical raw view in both arms
    CHECK(norm.rawCount == f.codecMotionVectors.size());
    CHECK(norm.state == "normalized" && pass.state == "bypassed");
    CHECK(norm.normalized.size() == norm.rawCount);
    CHECK(pass.normalized.empty()); // the only data difference between arms
    CHECK(pass.usableCount == 0);
    CHECK(norm.usableCount == usableUnderConsumerContract(norm.normalized));

    // And the raw observation is left identical by both controls.
    CHECK(deepEqualObservation(f, untouched));
}

// --- interface contract + fail-closed guards --------------------------------

static void testInterfaceContractAndFailClosed() {
    CHECK(std::string(kSideInfoNormalizationStageName)
          == "side_info_normalization");

    SideInfoNormalizationResult def;
    CHECK(def.mode == SideInfoNormalizationMode::Normalize);
    CHECK(def.rawCount == 0 && def.usableCount == 0);
    CHECK(def.normalized.empty());

    // Out-of-contract control value (invalid enum cast) fails closed: no
    // interpretation of the raw data, no prior, rawCount still reported.
    Observation f = makeFrame(17);
    f.codecMotionVectors = makeRawSet();
    const auto invalid = static_cast<SideInfoNormalizationMode>(200);
    const auto res = normalizeSideInfo(f, invalid);
    CHECK(res.normalized.empty());
    CHECK(res.usableCount == 0);
    CHECK(res.rawCount == f.codecMotionVectors.size());
    CHECK(res.state == "not_applicable");
}

int main() {
    testNormalizeDelegatesAndCountsEntries();
    testBypassProducesNoNormalizedPrior();
    testEmptyRawSideInfoIsNotApplicable();
    testStageNeverMutatesItsInput();
    testStageIsDeterministic();
    testBypassDiffersOnlyInNormalizedPriorPresence();
    testInterfaceContractAndFailClosed();
    if (failures) {
        std::printf("%d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("anvil_side_info_tests: all checks passed\n");
    return 0;
}
