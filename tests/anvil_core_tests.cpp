// anvil_core_tests.cpp — ANVIL successor core contract tests (pure CPU).
#include <cassert>
#include <cstdio>
#include <cstdint>
#include <string>
#include <vector>

#include "anvil/Core.hpp"
#include "anvil/JsonWriter.hpp"
#include "anvil/Manifest.hpp"
#include "anvil/Reconstruct.hpp"
#include "anvil/Sha256.hpp"

using namespace anvil;

static int failures = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

static void testWindowConfig() {
    WindowConfig single{0, 0};
    auto w = single.windowFor(7);
    CHECK(w.size() == 1 && w[0] == 7);

    WindowConfig sym{2, 2};
    w = sym.windowFor(3);
    CHECK(w.size() == 5 && w[0] == 1 && w[4] == 5);

    // window clamps at stream start
    w = sym.windowFor(0);
    CHECK(w.size() == 3 && w[0] == 0);

    WindowConfig asym{1, 3};
    w = asym.windowFor(2);
    CHECK(w.size() == 5 && w[0] == 1 && w[4] == 5);
}

static void testConfidenceAndGeometry() {
    Confidence c = Confidence::unknown();
    CHECK(!c.known);
    c = Confidence::withValue(0.75f);
    CHECK(c.known && c.value == 0.75f);
    CHECK(std::string(sideInfoStateName(SideInfoState::Unsupported)) == "unsupported");
    CHECK(std::string(sampleGeometryStateName(SampleGeometryState::Unknown)) == "unknown");
}

static void testJsonWriter() {
    JsonWriter w;
    w.beginObject();
    w.kv("a", int64_t(1));
    w.kv("s", std::string("x\"y"));
    w.array("arr");
    w.value(1);
    w.value(2);
    w.endArray();
    w.key("n"), w.null();
    w.kv("b", true);
    w.endObject();
    CHECK(w.str() == R"({"a":1,"s":"x\"y","arr":[1,2],"n":null,"b":true})");
}

static void testManifestRoundTripNames() {
    bool ok = false;
    CHECK(stageName(stageFromName("accumulate", ok)) == std::string("accumulate") && ok);
    stageFromName("bogus", ok);
    CHECK(!ok);
}

static Observation makeObs(uint64_t idx, int w, int h, uint8_t fill, int shiftX = 0) {
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
                uint8_t((x + shiftX >= 0 && x + shiftX < w) ? fill : 0);
    return o;
}

static void testEstimatorIsLabeledAndCorrect() {
    // Target has a bright block at x=32..47; obs shifted 4 px right.
    Observation target = makeObs(0, 64, 64, 128);
    for (int y = 0; y < 16; ++y)
        for (int x = 32; x < 48; ++x)
            target.plane[0][size_t(y) * 64 + x] = 250;
    Observation obs = target;
    obs.frameIndex = 1;
    // shift content +4 px in x
    std::vector<uint8_t> shifted(size_t(64) * 64, 60);
    for (int y = 0; y < 64; ++y)
        for (int x = 4; x < 64; ++x)
            shifted[size_t(y) * 64 + x] = target.plane[0][size_t(y) * 64 + (x - 4)];
    obs.plane[0] = shifted;

    auto blocks = estimateCorrespondence(target, obs, 16, 8);
    CHECK(!blocks.empty());
    bool foundBright = false;
    for (const BlockMotion& b : blocks) {
        CHECK(b.source == CorrespondenceSource::ImageEstimate);
        CHECK(b.refFrameIndex == 1); // proven by construction
        CHECK(!b.ambiguous);
        if (b.dstX == 32 && b.dstY == 0) {
            foundBright = true;
            CHECK(b.mvX == 4.0f); // obs content sits at +4 -> mv +4
        }
    }
    CHECK(foundBright);
}

static void testAccumulateSingleFrameIsIdentity() {
    Observation target = makeObs(0, 32, 32, 200);
    auto res = accumulate(target, {}, {}, {});
    CHECK(res.frame.plane[0] == target.plane[0]);
    CHECK(res.totalSamples == 0);
}

static void testAccumulateIncludesTargetSample() {
    Observation target = makeObs(0, 32, 32, 100);
    target.color.bitDepth = 8;
    Observation obs = makeObs(1, 32, 32, 200);
    obs.color.bitDepth = 8;
    FlowField zero(static_cast<size_t>(32 * 32 * 2), 0.0f);
    std::vector<Visibility> valid(static_cast<size_t>(32 * 32), Visibility::Valid);
    auto res = accumulate(target, {obs}, {zero}, {valid});
    CHECK(res.frame.plane[0][0] == 150);
    CHECK(res.validSamples == uint64_t(32 * 32));
    CHECK(res.totalSamples == uint64_t(32 * 32));
}

static void testAccumulateAverageWithOracleFlow() {
    // Two-frame window, oracle flow: obs is target shifted +4 px; mv +4.
    Observation target = makeObs(0, 64, 64, 100);
    Observation obs = target;
    obs.frameIndex = 1;
    std::vector<uint8_t> shifted(size_t(64) * 64, 200);
    for (int y = 0; y < 64; ++y)
        for (int x = 4; x < 64; ++x)
            shifted[size_t(y) * 64 + x] = target.plane[0][size_t(y) * 64 + (x - 4)];
    obs.plane[0] = shifted;

    BlockMotion b;
    b.frameIndex = 0;
    b.refFrameIndex = 1;
    b.dstX = 0; b.dstY = 0; b.blockW = 64; b.blockH = 64;
    b.mvX = 4.0f; b.mvY = 0.0f;
    b.source = CorrespondenceSource::Oracle;
    auto flow = buildFlowField(64, 64, {b}, 1);
    auto res = accumulate(target, {obs}, {flow}, {});

    CHECK(res.totalSamples == uint64_t(64 * 64));
    // pixels x>=4: target(100) + obs(shifted back to 100) = 100
    // pixels x<4: obs samples out of bounds -> invalid; keep target 100
    bool all100 = true;
    for (size_t i = 4; i < 64 * 64; ++i)
        if (res.frame.plane[0][i] != 100) { all100 = false; break; }
    CHECK(all100);
}

static void testAmbiguousBlocksNeverEnterFlow() {
    BlockMotion amb;
    amb.frameIndex = 0;
    amb.refFrameIndex = -1;
    amb.ambiguous = true;
    amb.dstX = 0; amb.dstY = 0; amb.blockW = 32; amb.blockH = 32;
    amb.mvX = 99.0f; amb.mvY = 99.0f;
    amb.source = CorrespondenceSource::CodecMv;
    auto flow = buildFlowField(32, 32, {amb}, -1);
    bool allZero = true;
    for (float f : flow) if (f != 0.0f) { allZero = false; break; }
    CHECK(allZero); // ambiguous codec vectors are rejected
}

static void testProvenanceDetection() {
    // In a git checkout these must succeed or be nullopt — never crash.
    auto sha = detectGitSha();
    auto dirty = detectGitDirty();
    if (sha) CHECK(sha->size() == 40);
    if (dirty) CHECK(*dirty == "true" || *dirty == "false");
}

static void testGitShaAcceptsAnyHex() {
    // Regression (review 4202350957): a valid 40-hex SHA whose first char is
    // not '0' must be accepted. In a git checkout, detectGitSha() must equal
    // `git rev-parse HEAD` exactly. Outside a checkout it stays nullopt.
    auto sha = detectGitSha();
    if (!sha) {
        std::printf("note: not a git checkout; git-sha test skipped\n");
        return;
    }
    CHECK(sha->size() == 40);
    for (char c : *sha) {
        const bool hex = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f')
            || (c >= 'A' && c <= 'F');
        CHECK(hex);
    }
    FILE* p = popen("git rev-parse HEAD", "r");
    CHECK(p != nullptr);
    if (p) {
        char buf[64] = {};
        const bool ok = std::fgets(buf, sizeof(buf), p) != nullptr;
        pclose(p);
        if (ok) {
            std::string expected(buf);
            while (!expected.empty()
                   && (expected.back() == '\n' || expected.back() == '\r'))
                expected.pop_back();
            CHECK(*sha == expected); // manifest SHA == git rev-parse HEAD
        }
    }
}

static Observation makeObsWxH(uint64_t idx, int w, int h, uint8_t fill,
                              int shiftX = 0) {
    return makeObs(idx, w, h, fill, shiftX);
}

static void testPartialEdgeTilesAreCovered() {
    // Regression (review 4202350977): partial 16x16 edge tiles must be
    // estimated with clipped geometry instead of leaving pixels uncovered
    // (which degenerated into fabricated zero-motion blending).
    // 66x65: right column tiles are 2 wide, bottom row tiles 1 tall.
    Observation target = makeObs(0, 66, 65, 100);
    for (int y = 0; y < 16; ++y)
        for (int x = 40; x < 56; ++x)
            target.plane[0][size_t(y) * 66 + x] = 250;
    Observation obs = target;
    obs.frameIndex = 1;
    std::vector<uint8_t> shifted(size_t(66) * 65, 60);
    for (int y = 0; y < 65; ++y)
        for (int x = 4; x < 66; ++x)
            shifted[size_t(y) * 66 + x] = target.plane[0][size_t(y) * 66 + (x - 4)];
    obs.plane[0] = shifted;

    auto blocks = estimateCorrespondence(target, obs);
    CHECK(!blocks.empty());
    bool sawClippedW = false, sawClippedH = false;
    for (const BlockMotion& b : blocks) {
        if (b.blockW < 16) sawClippedW = true;
        if (b.blockH < 16) sawClippedH = true;
    }
    CHECK(sawClippedW); // 66 % 16 == 2
    CHECK(sawClippedH); // 65 % 16 == 1

    std::vector<uint8_t> covered;
    buildFlowField(66, 65, blocks, 1, &covered);
    size_t uncovered = 0;
    for (uint8_t c : covered) if (!c) ++uncovered;
    CHECK(uncovered == 0); // every pixel has proven correspondence

    // The bright block's clipped interior is recovered exactly.
    std::vector<BlockMotion> bright;
    for (const BlockMotion& b : blocks)
        if (b.dstY < 16 && b.dstX + int(b.blockW) > 40 && b.dstX < 56)
            bright.push_back(b);
    auto flow = buildFlowField(66, 65, bright, 1);
    auto res = accumulate(target, {obs}, {flow}, {});
    bool brightOk = true;
    for (int y = 0; y < 16 && brightOk; ++y)
        for (int x = 44; x < 52; ++x)
            if (res.frame.plane[0][size_t(y) * 66 + x] != 250) { brightOk = false; break; }
    CHECK(brightOk);
}

static void testEdgeTilesOn1920x1080() {
    // Regression (review 4202350977): 1920x1080 leaves 8-row bottom tiles;
    // they must be estimated and covered, not dropped.
    const int w = 1920, h = 1080;
    Observation target = makeObs(0, w, h, 100);
    Observation obs = target;
    obs.frameIndex = 1;
    auto blocks = estimateCorrespondence(target, obs);
    bool sawBottomRow = false;
    for (const BlockMotion& b : blocks)
        if (b.dstY == h - 8 && b.blockH == 8) sawBottomRow = true;
    CHECK(sawBottomRow);
    std::vector<uint8_t> covered;
    buildFlowField(w, h, blocks, 1, &covered);
    size_t uncovered = 0;
    for (uint8_t c : covered) if (!c) ++uncovered;
    CHECK(uncovered == 0);
}

static void testSha256KnownAnswers() {
    // FIPS 180-4 vectors — provenance hashes must be real SHA-256.
    CHECK(sha256Hex(nullptr, 0)
          == "e3b0c44298fc1c149afbf4c8996fb92427ae41e4649b934ca495991b7852b855");
    const std::string abc = "abc";
    CHECK(sha256Hex(reinterpret_cast<const uint8_t*>(abc.data()), abc.size())
          == "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad");
    const std::string twoBlocks =
        "abcdbcdecdefdefgefghfghighijhijkijkljklmklmnlmnomnopnopq";
    CHECK(sha256Hex(reinterpret_cast<const uint8_t*>(twoBlocks.data()),
                    twoBlocks.size())
          == "248d6a61d20638b8e5c026930c3e6039a33ce45964ff2167f6ecedd419db06c1");
    const std::string x1000(1000, 'x'); // multi-chunk file path coverage
    CHECK(sha256Hex(reinterpret_cast<const uint8_t*>(x1000.data()), x1000.size())
          == "44f8354494a5ba03ba1792a8d3e9c534c47a9181980fde7a3f44b06ef2ae7c7f");
}

int main() {
    testWindowConfig();
    testConfidenceAndGeometry();
    testJsonWriter();
    testManifestRoundTripNames();
    testEstimatorIsLabeledAndCorrect();
    testAccumulateSingleFrameIsIdentity();
    testAccumulateIncludesTargetSample();
    testAccumulateAverageWithOracleFlow();
    testAmbiguousBlocksNeverEnterFlow();
    testProvenanceDetection();
    testGitShaAcceptsAnyHex();
    testPartialEdgeTilesAreCovered();
    testEdgeTilesOn1920x1080();
    testSha256KnownAnswers();
    if (failures) {
        std::printf("%d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("anvil_core_tests: all checks passed\n");
    return 0;
}
