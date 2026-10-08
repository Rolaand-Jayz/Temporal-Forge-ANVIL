// anvil_refinement_tests.cpp — coarse/refinement resolution-hierarchy tests.
//
// Evaluator finding (review 4209751753): with an exhaustive coarse search,
// refineCorrespondence's +/-1 residual re-scan could only repeat candidates
// the coarse stage had already evaluated, so strict < guaranteed refinement
// was a no-op for every interior winner. The fix makes the coarse stage an
// even-lattice PRIOR (searchStep=2 by default); refinement is the only stage
// that can select odd offsets.
//
// Fixture: g(x,y) = (x mod 16) + 14*(y mod 16) + (x/16). For any full
// 16-aligned block and integer displacement candidate (dx,dy), with
// obs = target shifted by the true displacement (s,0) inside the shifted
// region:
//
//   SAD(dx,dy) = 16*saw(dx-s) + 224*saw(dy) + col(dx,s)
//   saw(d) = 2*|d|*(16-|d|)  (per 16-sample period; |d| taken mod 16)
//   col(dx,s) <= 256        (block-column ramp, breaks period-16 aliasing)
//
// (each 16x16 block contains exactly one period of the x-sawtooth per row and
// of the y-sawtooth per column; the terms are separable). The landscape is
// construction-exact at the truth: SAD 0 uniquely at the true displacement
// for the cases tested (verified numerically) (the bare 16-periodic texture alone would alias +8 with -8 —
// both give saw SAD 0 — which is what the column ramp disambiguates),
// increasing in sawtooth distance otherwise. saw distances: d=1 -> 30,
// d=2 -> 56, d=8 -> 128; the y term is weighted 14x, so any dy!=0 candidate
// (numerically measured minimum 6496 across the tested shifts — per-pixel
// interference makes the landscape not exactly separable) is strictly
// dominated by every dy=0 candidate (<= 16*128 + 256 = 2304).
#include <cstdint>
#include <cstdio>
#include <vector>

#include "anvil/Core.hpp"
#include "anvil/Reconstruct.hpp"

using namespace anvil;

static int failures = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

// Deterministic textured frame (no RNG): 16-periodic position encoding plus
// a per-block-column ramp (x/16) that makes the texture aperiodic at the
// frame scale, so the +8 boundary displacement cannot alias with -8.
// Values span 0..228 with no wrap or clamp over the whole frame.
static Observation makeTexturedFrame(uint64_t idx, int w, int h) {
    Observation o;
    o.width = w;
    o.height = h;
    o.avPixelFormat = AV_PIX_FMT_YUV420P;
    o.frameIndex = idx;
    o.planeCount = 1;
    o.plane[0].assign(size_t(h) * w, 0);
    o.linesize[0] = w;
    for (int y = 0; y < h; ++y)
        for (int x = 0; x < w; ++x)
            o.plane[0][size_t(y) * w + x] =
                uint8_t((x % 16) + 14 * (y % 16) + (x / 16));
    return o;
}

// obs(x,y) = target(x-sx, y-sy) inside the frame, `fill` where the source
// coordinate falls outside. A block whose comparison window stays in the
// shifted region therefore matches target content at displacement (+sx,+sy).
static Observation shiftedCopy(const Observation& t, uint64_t idx,
                               int sx, int sy, uint8_t fill) {
    Observation o = t;
    o.frameIndex = idx;
    for (int y = 0; y < t.height; ++y)
        for (int x = 0; x < t.width; ++x) {
            const int srcX = x - sx, srcY = y - sy;
            o.plane[0][size_t(y) * t.width + x] =
                (srcX >= 0 && srcX < t.width && srcY >= 0 && srcY < t.height)
                    ? t.plane[0][size_t(srcY) * t.width + srcX]
                    : fill;
        }
    return o;
}

// Inline block SAD recomputation (mirrors the estimator's in-bounds luma
// SAD); only called with provably in-bounds windows in this file.
static uint32_t sadAt(const Observation& target, const Observation& obs,
                      int bx, int by, int dx, int dy, int bw, int bh) {
    uint32_t sad = 0;
    for (int y = 0; y < bh; ++y)
        for (int x = 0; x < bw; ++x) {
            const int tv =
                target.plane[0][size_t(by + y) * target.width + bx + x];
            const int ov = obs.plane[0][size_t(by + y + dy) * obs.width
                                        + bx + x + dx];
            sad += uint32_t(tv > ov ? tv - ov : ov - tv);
        }
    return sad;
}

static const BlockMotion* findBlock(const std::vector<BlockMotion>& blocks,
                                    int dstX, int dstY) {
    for (const BlockMotion& b : blocks)
        if (b.dstX == dstX && b.dstY == dstY) return &b;
    return nullptr;
}

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

static bool sameBlockVector(const std::vector<BlockMotion>& a,
                            const std::vector<BlockMotion>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); ++i)
        if (!sameBlockMotion(a[i], b[i])) return false;
    return true;
}

// The designated interior block: full 16x16 at (32,16) in a 64x64 frame, so
// every coarse candidate in [-8,+8]^2 and every +/-1 residual window is
// in-bounds (x range [24,55], y range [8,39]) — no ~0u penalty involved.
static const int kBx = 32, kBy = 16;

// --- 1. Odd true displacement: coarse is an imperfect even prior, ----------
//        refinement alone reaches the truth.
static void testOddDisplacementNeedsRefinement(int trueShift) {
    // Case setup: obs = target shifted +trueShift px in x (0 in y).
    const Observation target = makeTexturedFrame(0, 64, 64);
    const Observation obs = shiftedCopy(target, 1, trueShift, 0, 60);

    // (a) Coarse with DEFAULTS (blockSize 16, radius 8, step 2): the prior
    // lattice is {-8,-6,...,+8}, so the winner for the textured block must be
    // an even offset — and, because the truth is odd, necessarily imperfect.
    // NOTE: this assertion deliberately FAILS at fd6fd9b4, where the coarse
    // search was exhaustive over [-8,+8] and returned the true odd
    // displacement itself — that is exactly the subsumption defect being
    // repaired; here it documents the two stages now resolve differently.
    const auto coarse = estimateCorrespondence(target, obs);
    const BlockMotion* cb = findBlock(coarse, kBx, kBy);
    CHECK(cb != nullptr);
    if (!cb) return;
    CHECK(cb->source == CorrespondenceSource::ImageEstimate);
    CHECK(cb->refFrameIndex == 1); // proven by construction
    CHECK(cb->precision == MotionPrecision::Integer);
    const int coarseMvX = static_cast<int>(cb->mvX);
    CHECK(coarseMvX % 2 == 0);          // even-lattice prior
    CHECK(coarseMvX != trueShift);      // deliberately imperfect on odd truth
    CHECK(cb->mvY == 0.0f);

    // (b) Refinement (default residualRadius 1) reaches the TRUE odd
    // displacement exactly: SAD 0 is unique and strict < lands on it.
    const auto refined = refineCorrespondence(target, obs, coarse);
    const BlockMotion* rb = findBlock(refined, kBx, kBy);
    CHECK(rb != nullptr);
    if (!rb) return;
    CHECK(rb->mvX == static_cast<float>(trueShift));
    CHECK(rb->mvY == 0.0f);
    CHECK(rb->precision == MotionPrecision::Integer);
    CHECK(rb->source == CorrespondenceSource::ImageEstimate);
    CHECK(rb->refFrameIndex == 1);

    // (c) Control: residualRadius=0 evaluates only the coarse candidate, so
    // strict < can never fire — the whole coarse field is returned unchanged.
    const auto control = refineCorrespondence(target, obs, coarse, 0);
    CHECK(sameBlockVector(control, coarse));

    // (d) Refinement strictly lowered the measured residual: recompute the
    // block SAD inline at both displacements.
    const uint32_t sadCoarse =
        sadAt(target, obs, kBx, kBy, coarseMvX, 0, 16, 16);
    const uint32_t sadRefined =
        sadAt(target, obs, kBx, kBy, trueShift, 0, 16, 16);
    CHECK(sadRefined == 0);         // exact match at the true displacement
    CHECK(sadRefined < sadCoarse);  // refinement strictly improved
}

static void testOddDisplacementPlus1AndPlus3() {
    testOddDisplacementNeedsRefinement(1);
    testOddDisplacementNeedsRefinement(3);
}

// --- 2. Even true displacement: coarse is already the optimum, --------------
//        refinement must leave it unchanged.
static void testEvenDisplacementStaysAtCoarseOptimum() {
    const Observation target = makeTexturedFrame(0, 64, 64);
    const Observation obs = shiftedCopy(target, 1, 4, 0, 60);

    const auto coarse = estimateCorrespondence(target, obs);
    const BlockMotion* cb = findBlock(coarse, kBx, kBy);
    CHECK(cb != nullptr);
    if (!cb) return;
    CHECK(cb->mvX == 4.0f); // +4 is ON the even lattice: coarse is exact

    const auto refined = refineCorrespondence(target, obs, coarse);
    const BlockMotion* rb = findBlock(refined, kBx, kBy);
    CHECK(rb != nullptr);
    if (!rb) return;
    CHECK(rb->mvX == 4.0f && rb->mvY == 0.0f); // no drift off the optimum

    // Residual proof: nothing within +/-1 beats SAD 0.
    CHECK(sadAt(target, obs, kBx, kBy, 3, 0, 16, 16)
          > sadAt(target, obs, kBx, kBy, 4, 0, 16, 16));
    CHECK(sadAt(target, obs, kBx, kBy, 5, 0, 16, 16)
          > sadAt(target, obs, kBx, kBy, 4, 0, 16, 16));
}

// --- 3. Boundary true displacement: coarse winner is pinned to the ----------
//        lattice edge; refinement may reach +/-9 but must stay at 8.
static void testBoundaryDisplacementUnchanged() {
    const Observation target = makeTexturedFrame(0, 64, 64);
    const Observation obs = shiftedCopy(target, 1, 8, 0, 60);

    const auto coarse = estimateCorrespondence(target, obs);
    const BlockMotion* cb = findBlock(coarse, kBx, kBy);
    CHECK(cb != nullptr);
    if (!cb) return;
    CHECK(cb->mvX == 8.0f); // unique SAD 0 at the +8 lattice edge

    const auto refined = refineCorrespondence(target, obs, coarse);
    const BlockMotion* rb = findBlock(refined, kBx, kBy);
    CHECK(rb != nullptr);
    if (!rb) return;
    // The +/-1 window here includes +9 — a candidate the coarse lattice can
    // never contain — and it still loses to the exact match at +8.
    CHECK(rb->mvX == 8.0f && rb->mvY == 0.0f);
    CHECK(sadAt(target, obs, kBx, kBy, 9, 0, 16, 16)
          > sadAt(target, obs, kBx, kBy, 8, 0, 16, 16));
}

// --- 4. Fail-closed: malformed inputs return the coarse field unchanged -----
//        instead of crashing or inventing motion.
static void testRefinementFailsClosed() {
    const Observation target = makeTexturedFrame(0, 64, 64);
    const Observation obs = shiftedCopy(target, 1, 1, 0, 60);
    const auto coarse = estimateCorrespondence(target, obs);
    CHECK(!coarse.empty());

    // Empty target plane: refinement must bail out and echo the coarse field.
    Observation emptyTarget = target;
    emptyTarget.plane[0].clear();
    const auto noTarget = refineCorrespondence(emptyTarget, obs, coarse);
    CHECK(sameBlockVector(noTarget, coarse));

    // Empty reference plane: same fail-closed echo.
    Observation emptyObs = obs;
    emptyObs.plane[0].clear();
    const auto noObs = refineCorrespondence(target, emptyObs, coarse);
    CHECK(sameBlockVector(noObs, coarse));

    // Blocks whose reference identity is not this obs frame, or that are
    // ambiguous, must be passed through untouched; only proven blocks refine.
    std::vector<BlockMotion> mixed = coarse;
    BlockMotion foreign;
    foreign.frameIndex = 0;
    foreign.refFrameIndex = 99; // != obs.frameIndex (1)
    foreign.dstX = 0; foreign.dstY = 0; foreign.blockW = 16; foreign.blockH = 16;
    foreign.mvX = 5.0f; foreign.mvY = -5.0f;
    foreign.source = CorrespondenceSource::ImageEstimate;
    BlockMotion ambiguous = foreign;
    ambiguous.refFrameIndex = 1; // right frame, but unproven identity
    ambiguous.ambiguous = true;
    mixed.push_back(foreign);
    mixed.push_back(ambiguous);

    const auto refinedMixed = refineCorrespondence(target, obs, mixed);
    CHECK(refinedMixed.size() == mixed.size());
    if (refinedMixed.size() == mixed.size()) {
        // The appended foreign/ambiguous entries are byte-identical.
        CHECK(sameBlockMotion(refinedMixed[refinedMixed.size() - 2], foreign));
        CHECK(sameBlockMotion(refinedMixed[refinedMixed.size() - 1], ambiguous));
        // The proven coarse field refined exactly as it does alone.
        const auto refinedOnly = refineCorrespondence(target, obs, coarse);
        for (size_t i = 0; i < coarse.size(); ++i)
            CHECK(sameBlockMotion(refinedMixed[i], refinedOnly[i]));
    }
}

// --- 5. Determinism: repeated estimation+refinement are identical -----------
static void testRefinementDeterminism() {
    const Observation target = makeTexturedFrame(0, 64, 64);
    const Observation obs = shiftedCopy(target, 1, 1, 0, 60);

    const auto coarse1 = estimateCorrespondence(target, obs);
    const auto refined1 = refineCorrespondence(target, obs, coarse1);
    const auto coarse2 = estimateCorrespondence(target, obs);
    const auto refined2 = refineCorrespondence(target, obs, coarse2);

    CHECK(sameBlockVector(coarse1, coarse2));
    CHECK(sameBlockVector(refined1, refined2));
    CHECK(!coarse1.empty());
}

// searchStep < 1 must fail closed (empty field), never loop or fabricate a
// lattice — pinned here because the runner only ever uses the default.
static void testSearchStepFailsClosed() {
    Observation target = makeTexturedFrame(0, 64, 64);
    Observation obs = makeTexturedFrame(1, 64, 64);
    for (int step : {0, -1, -7}) {
        CHECK(estimateCorrespondence(target, obs, 16, 8, step).empty());
    }
    // step=1 is the documented degeneration to the exhaustive scan
    CHECK(!estimateCorrespondence(target, obs, 16, 8, 1).empty());
}


int main() {
    testSearchStepFailsClosed();
    testOddDisplacementPlus1AndPlus3();
    testEvenDisplacementStaysAtCoarseOptimum();
    testBoundaryDisplacementUnchanged();
    testRefinementFailsClosed();
    testRefinementDeterminism();
    if (failures) {
        std::printf("%d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("anvil_refinement_tests: all checks passed\n");
    return 0;
}
