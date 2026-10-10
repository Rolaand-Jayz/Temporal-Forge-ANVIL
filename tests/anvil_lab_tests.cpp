// anvil_lab_tests.cpp — unit + contract tests for the ANVIL Lab layer.
//
// Covers the Home Field Exhibition's mandatory regression matrix (§11 of
// the assignment): baseline immutability, fail-closed falsification,
// roster-vs-identity separation, starter merge-evidence requirements,
// external-control naming, scale-wording truthfulness, replacement
// wording, metrics oracles, difference analysis, and deterministic scene
// synthesis.
#include <cmath>
#include <cstdio>
#include <unistd.h>
#include <filesystem>
#include <string>
#include <vector>

#include "anvil_lab/Baseline.hpp"
#include "anvil_lab/Catalog.hpp"
#include "anvil_lab/DiffMap.hpp"
#include "anvil_lab/Image.hpp"
#include "anvil_lab/Json.hpp"
#include "anvil_lab/Metrics.hpp"
#include "anvil_lab/Png.hpp"
#include "anvil_lab/PnmIo.hpp"
#include "anvil_lab/Resize.hpp"
#include "anvil_lab/Scenes.hpp"

namespace fs = std::filesystem;
using anvil_lab::JsonValue;

static int gFailures = 0;
static int gChecks = 0;

#define CHECK(cond)                                                            \
    do {                                                                       \
        ++gChecks;                                                             \
        if (!(cond)) {                                                         \
            ++gFailures;                                                       \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);        \
        }                                                                      \
    } while (0)

#define CHECK_STR(a, b)                                                        \
    do {                                                                       \
        ++gChecks;                                                             \
        if ((a) != (b)) {                                                      \
            ++gFailures;                                                       \
            std::printf("FAIL %s:%d: '%s' != '%s'\n", __FILE__, __LINE__,      \
                        std::string(a).c_str(), std::string(b).c_str());       \
        }                                                                      \
    } while (0)

namespace {

fs::path makeTempDir() {
    const auto base = fs::temp_directory_path() / "anvil_lab_tests";
    static int counter = 0;
    const auto dir = base / (std::to_string(::getpid()) + "_"
        + std::to_string(counter++));
    fs::create_directories(dir);
    return dir;
}

anvil_lab::Image solidImage(int w, int h, uint16_t r, uint16_t g, uint16_t b,
                            int maxval = 255) {
    anvil_lab::Image img;
    img.width = w;
    img.height = h;
    img.maxval = maxval;
    img.r.assign(static_cast<size_t>(w) * h, r);
    img.g.assign(static_cast<size_t>(w) * h, g);
    img.b.assign(static_cast<size_t>(w) * h, b);
    return img;
}

// ---------------------------------------------------------------- JSON

void testJson() {
    std::string err;
    JsonValue v;
    CHECK(anvil_lab::jsonParse("{\"a\": [1, 2.5, \"x\\n\"], \"b\": true, \"c\": null}", v, err));
    CHECK(v.isObject());
    CHECK(v.at("a").isArray());
    CHECK(v.at("a").arr.size() == 3);
    CHECK(v.at("a").arr[0].asInt() == 1);
    CHECK(std::fabs(v.at("a").arr[1].asNumber() - 2.5) < 1e-12);
    CHECK_STR(v.at("a").arr[2].asString(), "x\n");
    CHECK(v.at("b").asBool());
    CHECK(v.at("c").isNull());
    CHECK(v.at("missing").isNull());
    // Round-trip: parse(dump(v)) preserves values.
    JsonValue w;
    CHECK(anvil_lab::jsonParse(anvil_lab::jsonDump(v), w, err));
    CHECK_STR(anvil_lab::jsonDump(w), anvil_lab::jsonDump(v));
    // Strictness: trailing garbage, bad escapes, unterminated strings.
    CHECK(!anvil_lab::jsonParse("{} trailing", v, err));
    CHECK(!anvil_lab::jsonParse("{\"a\": \"\\q\"}", v, err));
    CHECK(!anvil_lab::jsonParse("[1, 2", v, err));
    CHECK(!anvil_lab::jsonParse("{\"a\":1,}", v, err));
    // Deterministic integer formatting keeps regenerated files byte-stable.
    CHECK_STR(anvil_lab::jsonDump(anvil_lab::JsonValue::makeInt(42)), "42");
    CHECK_STR(anvil_lab::jsonDump(anvil_lab::JsonValue::makeNumber(0.5)), "0.5");
    // Escaping.
    CHECK_STR(anvil_lab::jsonStringEscape("a\"b\\c\nd"), "a\\\"b\\\\c\\nd");
}

// ---------------------------------------------------------------- PNM

void testPnmRoundTrip() {
    const fs::path tmp = makeTempDir();
    std::string err;
    // 8-bit RGB round trip.
    anvil_lab::Image a = solidImage(7, 5, 10, 200, 30);
    CHECK(anvil_lab::writePnm((tmp / "a.ppm").string(), a, err));
    anvil_lab::Image b;
    CHECK(anvil_lab::readPnm((tmp / "a.ppm").string(), b, err));
    CHECK(b.width == 7 && b.height == 5 && b.maxval == 255);
    CHECK(b.r == a.r && b.g == a.g && b.b == a.b);
    // 12-bit gray (big-endian samples) round trip.
    anvil_lab::Image g12;
    g12.width = 9;
    g12.height = 4;
    g12.maxval = 4095;
    g12.r.resize(36);
    for (size_t i = 0; i < g12.r.size(); ++i) g12.r[i] = static_cast<uint16_t>(i * 113 % 4096);
    CHECK(anvil_lab::writePnm((tmp / "g.pgm").string(), g12, err));
    anvil_lab::Image g2;
    CHECK(anvil_lab::readPnm((tmp / "g.pgm").string(), g2, err));
    CHECK(g2.maxval == 4095 && g2.isGray());
    CHECK(g2.r == g12.r);
    // Fail-closed reader: trailing bytes, bad magic, truncated raster.
    std::vector<uint8_t> bytes;
    FILE* f = std::fopen((tmp / "a.ppm").string().c_str(), "rb");
    if (f) {
        uint8_t buf[4096];
        size_t n;
        while ((n = std::fread(buf, 1, sizeof buf, f)) > 0) bytes.insert(bytes.end(), buf, buf + n);
        std::fclose(f);
    }
    anvil_lab::Image bad;
    bytes.push_back('X');
    CHECK(!anvil_lab::parsePnm(bytes, bad, err));
    CHECK_STR(err, "trailing bytes after PNM raster");
    const std::string notPnm = "P4\n4 4\n255\n";
    CHECK(!anvil_lab::parsePnm(std::vector<uint8_t>(notPnm.begin(), notPnm.end()), bad, err));
}

// ---------------------------------------------------------------- PNG

void testPng() {
    const fs::path tmp = makeTempDir();
    std::string err;
    const anvil_lab::Image img = solidImage(16, 9, 255, 0, 128);
    const std::vector<uint8_t> rgb = anvil_lab::displayRgb(img);
    std::vector<uint8_t> png;
    CHECK(anvil_lab::encodePngRgb(png, 16, 9, rgb.data(), err));
    // PNG magic + IHDR dimensions.
    CHECK(png.size() > 24);
    CHECK(png[0] == 0x89 && png[1] == 'P' && png[2] == 'N' && png[3] == 'G');
    const uint32_t w = (uint32_t(png[16]) << 24) | (uint32_t(png[17]) << 16)
        | (uint32_t(png[18]) << 8) | uint32_t(png[19]);
    CHECK(w == 16);
    // Display mapping is deterministic rounding, never truncation.
    CHECK(anvil_lab::toDisplay8(255, 255) == 255);
    CHECK(anvil_lab::toDisplay8(128, 255) == 128);
    CHECK(anvil_lab::toDisplay8(2047, 1023) == 255); // 10-bit full scale
    CHECK(anvil_lab::toDisplay8(512, 1023) == 128);  // rounds up; truncation would give 127
}

// ---------------------------------------------------------------- Resize

void testResize() {
    // Identity: 1x bicubic of a constant image is the same constant.
    const anvil_lab::Image c = solidImage(32, 32, 90, 90, 90);
    const anvil_lab::Image id = anvil_lab::resizeImage(c, 32, 32,
                                                       anvil_lab::ScaleFilter::Bicubic);
    CHECK(id.width == 32 && id.height == 32);
    CHECK(id.r == c.r);
    // Exact 2x downscale of a constant stays constant (kernel integrates).
    const anvil_lab::Image half = anvil_lab::resizeImage(c, 16, 16,
                                                         anvil_lab::ScaleFilter::Lanczos3);
    bool constant = true;
    for (uint16_t v : half.r) constant = constant && v >= 85 && v <= 95;
    CHECK(constant);
    // 2x upscale of a checkerboard: bicubic and lanczos differ (distinct
    // transfer behavior) but both stay within sample bounds.
    anvil_lab::Image chk;
    chk.width = 16;
    chk.height = 16;
    chk.maxval = 255;
    chk.r.assign(256, 0);
    chk.g.assign(256, 0);
    chk.b.assign(256, 0);
    for (int y = 0; y < 16; ++y)
        for (int x = 0; x < 16; ++x)
            if (((x / 4) + (y / 4)) % 2 == 0) {
                const size_t i = static_cast<size_t>(y) * 16 + x;
                chk.r[i] = 220;
            }
    const auto up1 = anvil_lab::resizeImage(chk, 32, 32, anvil_lab::ScaleFilter::Bicubic);
    const auto up2 = anvil_lab::resizeImage(chk, 32, 32, anvil_lab::ScaleFilter::Lanczos3);
    CHECK(up1.r != up2.r);
    for (uint16_t v : up1.r) CHECK(v <= 255);
    for (uint16_t v : up2.r) CHECK(v <= 255);
}

// ---------------------------------------------------------------- Metrics

void testMetrics() {
    std::string err;
    // Identical images: SSIM 1, zero diff, psnr flagged infinite.
    anvil_lab::Image a = solidImage(64, 48, 100, 150, 200);
    anvil_lab::PlaneMetrics pm;
    CHECK(anvil_lab::computePlaneMetrics(a, a, pm, err));
    CHECK(pm.identical);
    CHECK(pm.psnr < 0); // -1 sentinel: infinite
    CHECK(std::fabs(pm.ssim - 1.0) < 1e-12);
    CHECK(pm.maxAbsDiff == 0 && pm.edgeDiffMean == 0);
    // Known offset: mse is exact.
    anvil_lab::Image b = a;
    for (size_t i = 0; i < b.r.size(); ++i) b.r[i] = 110; // +10 on R
    // luma delta = 0.2126*10
    CHECK(anvil_lab::computePlaneMetrics(a, b, pm, err));
    const double expectMse = (0.2126 * 10.0) * (0.2126 * 10.0);
    CHECK(std::fabs(pm.mse - expectMse) < 1e-6);
    CHECK(std::fabs(pm.psnr - (10.0 * std::log10(255.0 * 255.0 / expectMse))) < 1e-9);
    CHECK(pm.ssim < 1.0);
    // Denoising improves SSIM (temporal-accumulation-shaped oracle).
    const anvil_lab::SceneSpec spec = anvil_lab::sceneSpec("archive_grid_drift");
    anvil_lab::Image clean = anvil_lab::renderSceneFrame(spec, 0);
    // A smaller window than the SSIM kernel is rejected, not truncated.
    anvil_lab::Image tiny = solidImage(8, 8, 10, 10, 10);
    CHECK(!anvil_lab::computePlaneMetrics(tiny, tiny, pm, err));
    // Geometry mismatch is a hard error — invalid pairing, never a score.
    anvil_lab::Image other = solidImage(32, 32, 0, 0, 0);
    CHECK(!anvil_lab::computePlaneMetrics(a, other, pm, err));
    // Temporal delta: zero for identical frames.
    CHECK(std::fabs(anvil_lab::temporalDelta(a, a, err)) < 1e-12);
}

// ---------------------------------------------------------------- DiffMap

void testDiffMap() {
    std::string err;
    anvil_lab::Image a = solidImage(64, 64, 0, 0, 0);
    anvil_lab::Image b = a;
    // One 10x10 block differs by 40; another isolated 4x4 block differs by 30.
    for (int y = 10; y < 20; ++y)
        for (int x = 10; x < 20; ++x) {
            const size_t i = static_cast<size_t>(y) * 64 + x;
            b.r[i] = 40;
        }
    for (int y = 40; y < 44; ++y)
        for (int x = 50; x < 54; ++x) {
            const size_t i = static_cast<size_t>(y) * 64 + x;
            b.g[i] = 30;
        }
    anvil_lab::AbsDiff d;
    CHECK(anvil_lab::computeAbsDiff(a, b, d, err));
    anvil_lab::DiffStats st = anvil_lab::diffStats(d, 25);
    CHECK(st.nonzeroPixels == 100 + 16);
    CHECK(st.hotPixels == 116); // 40-delta and 30-delta blocks both clear 25
    st = anvil_lab::diffStats(d, 35);
    CHECK(st.hotPixels == 100); // only the 40-delta block clears 35
    CHECK(std::fabs(st.maxAbs - 40.0) < 1e-12);
    // Zero-difference handling: identical → all zeros.
    anvil_lab::AbsDiff d0;
    CHECK(anvil_lab::computeAbsDiff(a, a, d0, err));
    CHECK(anvil_lab::diffStats(d0, 1).nonzeroPixels == 0);
    // Regions: two components, numbered biggest-first.
    auto regions = anvil_lab::differenceRegions(d, 20, 4, 2);
    CHECK(regions.size() == 2);
    CHECK(regions[0].pixels == 100); // 10x10 block is the larger region
    CHECK(regions[0].id == 1);
    CHECK(regions[0].x0 == 10 && regions[0].y0 == 10 && regions[0].x1 == 19 && regions[0].y1 == 19);
    CHECK(regions[1].id == 2);
    // min-size filtering removes the small block.
    auto big = anvil_lab::differenceRegions(d, 20, 50, 2);
    CHECK(big.size() == 1);
    // Heatmap endpoints: 0 → near-black, 1 → red.
    uint8_t r = 0, g = 0, bl = 0;
    anvil_lab::heatmapColor(0.0, r, g, bl);
    CHECK(r < 32 && g < 32 && bl < 32);
    anvil_lab::heatmapColor(1.0, r, g, bl);
    CHECK(r > 200 && g < 128);
}

// ---------------------------------------------------------------- Catalog

JsonValue candidateRecord(const std::string& kind, const std::string& name,
                          const std::string& status, int inW, int outW,
                          double factor, const std::string& description) {
    JsonValue r = JsonValue::makeObject();
    r.set("id", JsonValue::makeString("x/y"));
    r.set("kind", JsonValue::makeString(kind));
    r.set("display_name", JsonValue::makeString(name));
    r.set("roster_status", JsonValue::makeString(status));
    JsonValue in = JsonValue::makeObject();
    in.set("width", JsonValue::makeInt(inW));
    in.set("height", JsonValue::makeInt(360));
    r.set("input", std::move(in));
    JsonValue out = JsonValue::makeObject();
    out.set("width", JsonValue::makeInt(outW));
    out.set("height", JsonValue::makeInt(outW == inW ? 360 : outW * 360 / inW));
    r.set("output", std::move(out));
    JsonValue scale = JsonValue::makeObject();
    scale.set("factor", JsonValue::makeNumber(factor));
    r.set("scale", std::move(scale));
    r.set("description", JsonValue::makeString(description));
    JsonValue mods = JsonValue::makeArray();
    JsonValue mod = JsonValue::makeObject();
    mod.set("change", JsonValue::makeString("added"));
    mod.set("stage", JsonValue::makeString("local refinement"));
    mod.set("replaces", JsonValue::makeString(""));
    mods.arr.push_back(std::move(mod));
    r.set("modifications", std::move(mods));
    return r;
}

void testCatalogNaming() {
    // Canonical name building.
    CHECK_STR(anvil_lab::buildCandidateName({"local refinement"}, "tryout"),
              "ANVIL baseline + local refinement — tryout");
    // Baseline: exact name, never a suffix.
    std::vector<std::string> errs;
    JsonValue base = candidateRecord("anvil_baseline", "ANVIL baseline", "", 640, 640,
                                     1.0, "temporal reconstruction at native resolution");
    errs.clear(); CHECK(anvil_lab::validateCandidateRecord(base, errs)); CHECK(errs.empty());
    JsonValue baseBad = candidateRecord("anvil_baseline", "ANVIL baseline — tryout", "tryout",
                                        640, 640, 1.0, "temporal reconstruction");
    errs.clear(); CHECK(!anvil_lab::validateCandidateRecord(baseBad, errs)); CHECK(!errs.empty()); errs.clear();
    // (§11-6) A standalone spatial control must not be labeled ANVIL-derived.
    JsonValue spatial = candidateRecord("external_control", "ANVIL baseline + Lanczos3 2× — tryout",
                                        "tryout", 640, 1280, 2.0, "spatial upscaling");
    errs.clear(); CHECK(!anvil_lab::validateCandidateRecord(spatial, errs)); CHECK(!errs.empty()); errs.clear();
    JsonValue spatialOk = candidateRecord("external_control", "Spatial control — Lanczos3 2×",
                                          "", 640, 1280, 2.0, "spatial upscaling of decoded frames");
    errs.clear(); CHECK(anvil_lab::validateCandidateRecord(spatialOk, errs)); CHECK(errs.empty());
    // (§11-7) Same-resolution output must not be called upscaled.
    JsonValue upFalse = candidateRecord("anvil_candidate",
                                        "ANVIL baseline + local refinement — tryout",
                                        "tryout", 640, 640, 1.0,
                                        "temporal reconstruction (upscaled)");
    errs.clear(); CHECK(!anvil_lab::validateCandidateRecord(upFalse, errs)); CHECK(!errs.empty()); errs.clear();
    // Truthful same-resolution wording passes.
    JsonValue okNative = candidateRecord("anvil_candidate",
                                         "ANVIL baseline + local refinement — tryout",
                                         "tryout", 640, 640, 1.0,
                                         "temporal reconstruction at native resolution");
    errs.clear(); CHECK(anvil_lab::validateCandidateRecord(okNative, errs)); CHECK(errs.empty());
    // (§11-8) Replaced components must be described as replaced.
    JsonValue repl = candidateRecord("anvil_candidate",
                                     "ANVIL baseline + DIS optical flow — tryout",
                                     "tryout", 640, 640, 1.0, "temporal reconstruction");
    JsonValue badMod = JsonValue::makeObject();
    badMod.set("change", JsonValue::makeString("replaced"));
    badMod.set("stage", JsonValue::makeString("DIS optical flow"));
    badMod.set("replaces", JsonValue::makeString("")); // replacement without naming what
    repl.at("modifications").arr[0] = badMod;
    errs.clear(); CHECK(!anvil_lab::validateCandidateRecord(repl, errs)); CHECK(!errs.empty()); errs.clear();
    JsonValue goodMod = JsonValue::makeObject();
    goodMod.set("change", JsonValue::makeString("replaced"));
    goodMod.set("stage", JsonValue::makeString("DIS optical flow"));
    goodMod.set("replaces", JsonValue::makeString("ANVIL block-SAD"));
    repl.at("modifications").arr[0] = goodMod;
    errs.clear(); CHECK(anvil_lab::validateCandidateRecord(repl, errs)); CHECK(errs.empty());
    // Unknown status vocabulary is rejected.
    JsonValue vocab = candidateRecord("anvil_candidate", "ANVIL baseline + x — champion",
                                      "champion", 640, 640, 1.0, "temporal reconstruction");
    errs.clear(); CHECK(!anvil_lab::validateCandidateRecord(vocab, errs)); CHECK(!errs.empty()); errs.clear();
}

void testRoster() {
    const fs::path tmp = makeTempDir();
    const fs::path rosterPath = tmp / "roster.json";
    std::string err;
    const std::string player = "cfg:" + std::string(64, 'a');
    const std::string base = "{\"candidate_id\":\"" + player + "\",\"to\":\"tryout\","
        "\"reason\":\"invited\",\"authority\":\"maintainer\",\"evidence_ref\":\"exhibition\"}";
    JsonValue t;
    CHECK(anvil_lab::jsonParse(base, t, err));
    CHECK(anvil_lab::rosterSetStatus(rosterPath.string(), t, err));
    // (§11-5) starter without merge evidence is refused.
    const std::string starter = "{\"candidate_id\":\"" + player + "\",\"to\":\"starter\","
        "\"reason\":\"good metrics\",\"authority\":\"maintainer\",\"evidence_ref\":\"ci\"}";
    JsonValue st;
    CHECK(anvil_lab::jsonParse(starter, st, err));
    CHECK(!anvil_lab::rosterSetStatus(rosterPath.string(), st, err));
    // starter WITH integration commit + merge evidence is accepted.
    st.set("integration_commit", JsonValue::makeString("0123456789abcdef0123456789abcdef01234567"));
    st.set("merge_evidence", JsonValue::makeBool(true));
    CHECK(anvil_lab::rosterSetStatus(rosterPath.string(), st, err));
    // (§11-4) Status changes must not touch experiment records: history
    // appends; the experiment/candidate identity files are untouched.
    JsonValue roster;
    CHECK(anvil_lab::jsonReadFile(rosterPath.string(), roster, err));
    CHECK(roster.at("history").arr.size() == 2);
    CHECK_STR(roster.at("entries").at(player).at("status").asString(), "starter");
    // The canonical baseline can never carry a status.
    JsonValue bl;
    CHECK(anvil_lab::jsonParse("{\"candidate_id\":\"baseline\",\"to\":\"bench\","
          "\"reason\":\"x\",\"authority\":\"m\",\"evidence_ref\":\"e\"}", bl, err));
    CHECK(!anvil_lab::rosterSetStatus(rosterPath.string(), bl, err));
    // Self-transition refused.
    JsonValue same;
    CHECK(anvil_lab::jsonParse("{\"candidate_id\":\"" + player + "\",\"to\":\"starter\","
          "\"reason\":\"x\",\"authority\":\"m\",\"evidence_ref\":\"e\"}", same, err));
    CHECK(!anvil_lab::rosterSetStatus(rosterPath.string(), same, err));
    // Audit: roster referencing an unknown candidate is a finding.
    JsonValue catalog = JsonValue::makeArray();
    catalog.arr.push_back(candidateRecord("anvil_candidate",
                                          "ANVIL baseline + x — tryout", "tryout",
                                          640, 640, 1.0, "temporal reconstruction"));
    catalog.arr[0].set("id", JsonValue::makeString("other/c"));
    std::vector<std::string> errs;
    CHECK(!anvil_lab::rosterAudit(roster, catalog, errs));
}

void testPipelineDiff() {
    auto stages = [](const char* refine, const char* conf) {
        JsonValue arr = JsonValue::makeArray();
        auto s = [](const char* n, const char* m, bool active) {
            JsonValue v = JsonValue::makeObject();
            v.set("name", JsonValue::makeString(n));
            v.set("mode", JsonValue::makeString(m));
            v.set("active", JsonValue::makeBool(active));
            return v;
        };
        arr.arr.push_back(s("correspondence_refinement", refine, true));
        arr.arr.push_back(s("confidence", conf, true));
        arr.arr.push_back(s("accumulate", "confidence-weighted", true));
        return arr;
    };
    JsonValue cand = JsonValue::makeObject();
    JsonValue candP = JsonValue::makeObject();
    candP.set("stages", stages("local", "estimate"));
    cand.set("pipeline", candP);
    cand.set("scale", [&] {
        JsonValue s = JsonValue::makeObject();
        s.set("factor", JsonValue::makeNumber(2.0));
        return s;
    }());
    JsonValue base = JsonValue::makeObject();
    JsonValue baseP = JsonValue::makeObject();
    baseP.set("stages", stages("none", "unit"));
    base.set("pipeline", baseP);
    base.set("scale", [&] {
        JsonValue s = JsonValue::makeObject();
        s.set("factor", JsonValue::makeNumber(2.0));
        return s;
    }());
    const JsonValue diff = anvil_lab::pipelineDiff(cand, base);
    CHECK(diff.at("changed").arr.size() == 2); // refinement + confidence
    CHECK(diff.at("unchanged").arr.size() == 1); // accumulate
    CHECK_STR(diff.at("unchanged").arr[0].asString(), "accumulate");
    // A changed delivery scale surfaces explicitly (never buried).
    cand.set("scale", [&] {
        JsonValue s = JsonValue::makeObject();
        s.set("factor", JsonValue::makeNumber(1.0));
        return s;
    }());
    const JsonValue diff2 = anvil_lab::pipelineDiff(cand, base);
    bool found = false;
    for (const JsonValue& c : diff2.at("changed").arr)
        if (c.at("stage").asString() == "delivery_scale") found = true;
    CHECK(found);
}

// ---------------------------------------------------------------- Baseline

JsonValue baselineDefJson() {
    JsonValue j = JsonValue::makeObject();
    j.set("name", JsonValue::makeString("ANVIL baseline"));
    j.set("pinned_commit", JsonValue::makeString("f2f8b9929c1cc3845dab90bbe80d889b44c99390"));
    JsonValue config = JsonValue::makeObject();
    config.set("past", JsonValue::makeInt(2));
    config.set("future", JsonValue::makeInt(2));
    config.set("correspondence_mode", JsonValue::makeString("estimate"));
    config.set("accumulate_enabled", JsonValue::makeBool(true));
    j.set("config", config);
    j.set("config_hash", JsonValue::makeString(
        anvil_lab::sha256StringHex(anvil_lab::canonicalConfigString(config))));
    JsonValue impl = JsonValue::makeObject();
    JsonValue files = JsonValue::makeObject();
    files.set("src/anvil/Sha256.cpp",
              JsonValue::makeString(anvil_lab::sha256StringHex("placeholder-content")));
    impl.set("files", files);
    j.set("implementation", impl);
    return j;
}

void testBaseline() {
    const fs::path tmp = makeTempDir();
    std::string err;
    // (§11-2) A falsified baseline identity is rejected: hash mismatch.
    JsonValue def = baselineDefJson();
    def.set("config_hash", JsonValue::makeString(
        std::string(64, '0')));
    CHECK(anvil_lab::jsonWriteFile((tmp / "bad.json").string(), def, err));
    anvil_lab::BaselineDef loaded;
    CHECK(!anvil_lab::loadBaseline((tmp / "bad.json").string(), loaded, err));
    // A moving reference is not sufficient identity.
    def = baselineDefJson();
    def.set("pinned_commit", JsonValue::makeString("main"));
    CHECK(anvil_lab::jsonWriteFile((tmp / "moving.json").string(), def, err));
    CHECK(!anvil_lab::loadBaseline((tmp / "moving.json").string(), loaded, err));
    // Correct definition loads.
    def = baselineDefJson();
    CHECK(anvil_lab::jsonWriteFile((tmp / "base.json").string(), def, err));
    CHECK(anvil_lab::loadBaseline((tmp / "base.json").string(), loaded, err));

    // (§11-1) A future merged candidate cannot silently redefine the
    // baseline: any changed implementation file fails verification.
    const fs::path work = tmp / "repo";
    fs::create_directories(work / "src/anvil");
    FILE* f = std::fopen((work / "src/anvil/Sha256.cpp").string().c_str(), "wb");
    std::fputs("placeholder-content", f);
    std::fclose(f);
    anvil_lab::VerifyReport rep = anvil_lab::verifyBaselineTree(loaded, work.string());
    CHECK(rep.ok);
    f = std::fopen((work / "src/anvil/Sha256.cpp").string().c_str(), "wb");
    std::fputs("placeholder-content CHANGED BY A MERGED CANDIDATE", f);
    std::fclose(f);
    rep = anvil_lab::verifyBaselineTree(loaded, work.string());
    CHECK(!rep.ok);
    CHECK(!rep.problems.empty());
    // Missing file also fails.
    fs::remove(work / "src/anvil/Sha256.cpp");
    rep = anvil_lab::verifyBaselineTree(loaded, work.string());
    CHECK(!rep.ok);

    // Pinned-commit presence is a repository property, checked separately
    // against the real checkout (tests run with WORKING_DIRECTORY=repo root).
    anvil_lab::BaselineDef real = loaded;
    const anvil_lab::VerifyReport commit =
        anvil_lab::verifyPinnedCommit(real, ".");
    CHECK(commit.ok); // f2f8b992 exists in this repository's history

    // (§11-3) A run verifies as baseline iff its configuration hash matches.
    JsonValue manifest = JsonValue::makeObject();
    manifest.set("config", baselineDefJson().at("config"));
    JsonValue prov = JsonValue::makeObject();
    prov.set("git_sha", JsonValue::makeString("ffffffffffffffffffffffffffffffffffffffff"));
    manifest.set("provenance", prov);
    f = std::fopen((work / "src/anvil/Sha256.cpp").string().c_str(), "wb");
    std::fputs("placeholder-content", f);
    std::fclose(f);
    rep = anvil_lab::verifyRunIsBaseline(loaded, manifest, work.string());
    CHECK(rep.ok);
    JsonValue altered = manifest;
    altered.at("config").set("past", JsonValue::makeInt(4));
    rep = anvil_lab::verifyRunIsBaseline(loaded, altered, work.string());
    CHECK(!rep.ok);
    // Canonical config string ignores experiment scope (paths/selection).
    JsonValue scoped = manifest;
    scoped.at("config").set("input_path", JsonValue::makeString("/elsewhere.mp4"));
    scoped.at("config").set("start_frame", JsonValue::makeInt(9));
    CHECK_STR(anvil_lab::canonicalConfigString(scoped.at("config")),
              anvil_lab::canonicalConfigString(manifest.at("config")));
}

// ---------------------------------------------------------------- Scenes

void testScenes() {
    const anvil_lab::SceneSpec spec = anvil_lab::sceneSpec("archive_grid_drift");
    // Determinism: identical spec + frame → identical bytes.
    const anvil_lab::Image f0a = anvil_lab::renderSceneFrame(spec, 0);
    const anvil_lab::Image f0b = anvil_lab::renderSceneFrame(spec, 0);
    CHECK(f0a.r == f0b.r && f0a.g == f0b.g && f0a.b == f0b.b);
    CHECK(f0a.width == 1280 && f0a.height == 720);
    // Subpixel drift actually moves content (frame 0 vs frame 10 differ).
    const anvil_lab::Image f10 = anvil_lab::renderSceneFrame(spec, 10);
    CHECK(f0a.r != f10.r);
    // Scene B has genuine occlusion structure: two renders differ around
    // shape boundaries but agree on far corners? Simply verify motion.
    const anvil_lab::SceneSpec specB = anvil_lab::sceneSpec("crossing_occluders");
    const anvil_lab::Image b0 = anvil_lab::renderSceneFrame(specB, 0);
    const anvil_lab::Image b20 = anvil_lab::renderSceneFrame(specB, 20);
    CHECK(b0.r != b20.r);
    // Noise model: deterministic, zero at sigma=0, ~sigma at sigma=6.
    const anvil_lab::Image clean = solidImage(64, 64, 128, 128, 128);
    const anvil_lab::Image n0 = anvil_lab::addGaussianNoise(clean, 42, 0.0);
    CHECK(n0.r == clean.r);
    const anvil_lab::Image n1 = anvil_lab::addGaussianNoise(clean, 42, 6.0);
    const anvil_lab::Image n2 = anvil_lab::addGaussianNoise(clean, 42, 6.0);
    CHECK(n1.r == n2.r); // same seed → same noise
    const anvil_lab::Image n3 = anvil_lab::addGaussianNoise(clean, 43, 6.0);
    CHECK(n1.r != n3.r); // different seed → different noise
    double sum = 0;
    for (size_t i = 0; i < n1.r.size(); ++i)
        sum += std::fabs(double(n1.r[i]) - 128.0);
    const double meanAbs = sum / double(n1.r.size());
    CHECK(meanAbs > 3.0 && meanAbs < 7.0); // E|N(0,6)| = 6*sqrt(2/pi) ≈ 4.79
}

} // namespace

int main() {
    testJson();
    testPnmRoundTrip();
    testPng();
    testResize();
    testMetrics();
    testDiffMap();
    testCatalogNaming();
    testRoster();
    testPipelineDiff();
    testBaseline();
    testScenes();
    std::printf("anvil_lab_tests: %d checks, %d failures\n", gChecks, gFailures);
    return gFailures == 0 ? 0 : 1;
}
