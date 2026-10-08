// anvil_output_backend_tests.cpp — output backend boundary contract tests.
//
// Covers the evaluator repair for execution-pack capability D: the output
// stage must be independently replaceable/bypassable. The "pnm" backend must
// reproduce the runner's former inline serialization byte-for-byte; the
// "null" backend is the legitimate bypass (zero artifacts); the factory fails
// closed on unknown identities; everything is deterministic.
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <unistd.h>

#include "anvil/OutputBackend.hpp"
#include "anvil/Pnm.hpp"

using namespace anvil;
namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond)                                                     \
    do {                                                                \
        if (!(cond)) {                                                  \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond); \
            ++failures;                                                 \
        }                                                               \
    } while (0)

// --- helpers -----------------------------------------------------------------

static fs::path tempDir(const std::string& tag) {
    const fs::path root = fs::temp_directory_path()
        / ("anvil_output_backend_tests_" + std::to_string(::getpid()));
    const fs::path d = root / tag;
    fs::remove_all(d);
    fs::create_directories(d);
    return d;
}

static size_t entryCount(const fs::path& d) {
    size_t n = 0;
    for (auto it = fs::directory_iterator(d); it != fs::directory_iterator(); ++it) ++n;
    return n;
}

static bool readFileBytes(const fs::path& p, std::vector<uint8_t>& out) {
    std::ifstream f(p, std::ios::binary);
    if (!f) return false;
    f.seekg(0, std::ios::end);
    const std::streamoff n = f.tellg();
    if (n < 0) return false;
    f.seekg(0, std::ios::beg);
    out.resize(static_cast<size_t>(n));
    f.read(reinterpret_cast<char*>(out.data()), n);
    return f.good() || f.eof();
}

static bool bytesEqual(const std::vector<uint8_t>& a, const std::vector<uint8_t>& b) {
    return a.size() == b.size() && std::equal(a.begin(), a.end(), b.begin());
}

// Deterministic ramp fill (no RNG) so repeated constructions agree.
static std::vector<uint8_t> ramp(size_t n, uint8_t seed) {
    std::vector<uint8_t> v(n);
    for (size_t i = 0; i < n; ++i) v[i] = static_cast<uint8_t>(i * 37u + seed);
    return v;
}

// Planar 4:2:0 observation with deterministic plane content and exact
// stride-padded linesizes.
static Observation makePlanar420(AVPixelFormat fmt, int bitDepth, int w, int h) {
    Observation o;
    o.width = w;
    o.height = h;
    o.avPixelFormat = static_cast<int>(fmt);
    o.color.pixelFormat = static_cast<int>(fmt);
    o.color.bitDepth = bitDepth;
    o.planeCount = 3;
    const int cw = (w + 1) / 2, ch = (h + 1) / 2; // AV_CEIL_RSHIFT halves
    const int bps = bitDepth > 8 ? 2 : 1;
    o.linesize[0] = w * bps + 8;  // deliberately padded, like decoder linesizes
    o.linesize[1] = cw * bps + 4;
    o.linesize[2] = cw * bps + 4;
    o.plane[0] = ramp(static_cast<size_t>(o.linesize[0]) * h, 11);
    o.plane[1] = ramp(static_cast<size_t>(o.linesize[1]) * ch, 29);
    o.plane[2] = ramp(static_cast<size_t>(o.linesize[2]) * ch, 47);
    return o;
}

static std::string headerOf(const std::vector<uint8_t>& bytes) {
    // PNM headers end at the LF terminating maxval (third text line).
    int lfs = 0;
    for (size_t i = 0; i < bytes.size(); ++i) {
        if (bytes[i] == '\n' && ++lfs == 3) return std::string(bytes.begin(), bytes.begin() + i + 1);
    }
    return {};
}

static bool headerIs(const fs::path& p, const char* expected) {
    std::vector<uint8_t> bytes;
    if (!readFileBytes(p, bytes)) return false;
    return headerOf(bytes) == expected;
}

// --- 1. factory + polymorphic substitution -----------------------------------

static void testFactory() {
    std::string err;
    auto pnm = makeOutputBackend("pnm", err);
    CHECK(pnm != nullptr);
    CHECK(err.empty());
    CHECK(std::string(pnm->id()) == "pnm");
    CHECK(std::string(pnm->outputFormatLabel())
          == "ppm_or_depth_preserving_pgm_planes");

    auto nullB = makeOutputBackend("null", err);
    CHECK(nullB != nullptr);
    CHECK(err.empty());
    CHECK(std::string(nullB->id()) == "null");
    CHECK(std::string(nullB->outputFormatLabel()) == "null_backend_no_output");

    // Substitution without restructuring: both concrete backends are usable
    // solely through the abstract interface.
    OutputBackend* base = nullB.get();
    BackendWriteResult viaBase = base->writeFrame("/nonexistent-dir-unused",
                                                  BackendFrameInput{});
    CHECK(viaBase.ok);
    CHECK(viaBase.filesWritten.empty());

    for (const char* bad : {"fsr", "", "PNM", "pnm "}) {
        err.clear();
        auto unknown = makeOutputBackend(bad, err);
        CHECK(unknown == nullptr);
        CHECK(!err.empty());
    }
}

// --- 2. pnm RGB path: P6 bytes identical to writePpm --------------------------

static void testPnmRgbPath() {
    const int w = 5, h = 3, stride = w * 3 + 4; // padded stride exercised
    std::vector<uint8_t> rgb = ramp(static_cast<size_t>(stride) * h, 3);
    Observation o = makePlanar420(AV_PIX_FMT_YUV420P, 8, w, h);
    o.frameIndex = 3;

    PnmOutputBackend backend;
    BackendFrameInput in;
    in.frameIndex = 3;
    in.observation = &o;
    in.rgb = &rgb;
    in.rgbStride = stride;

    const fs::path dir = tempDir("rgb");
    BackendWriteResult r = backend.writeFrame(dir.string(), in);
    CHECK(r.ok);
    CHECK(r.error.empty());
    CHECK(r.filesWritten.size() == 1);
    const fs::path written = r.filesWritten[0];
    CHECK(fs::path(written).filename() == "frame_3.ppm");

    std::vector<uint8_t> got;
    CHECK(readFileBytes(written, got));
    CHECK(headerOf(got) == "P6\n5 3\n255\n");
    CHECK(got.size() == 11 + static_cast<size_t>(w) * 3 * h); // header + payload

    // Byte-equal to the shared writer invoked directly on the same raster.
    const fs::path ref = tempDir("rgb_ref") / "frame_3.ppm";
    CHECK(writePpm(ref.string(), w, h, rgb.data(), stride));
    std::vector<uint8_t> refBytes;
    CHECK(readFileBytes(ref, refBytes));
    CHECK(bytesEqual(got, refBytes));
}

// --- 3. planar 8-bit YUV420P: 3 files, ceil-halved chroma dims ----------------

static void testPlanar8Bit() {
    const int w = 5, h = 3; // odd dims: chroma must be ceil(5/2)=3 x ceil(3/2)=2
    Observation o = makePlanar420(AV_PIX_FMT_YUV420P, 8, w, h);

    PnmOutputBackend backend;
    BackendFrameInput in;
    in.frameIndex = 9;
    in.observation = &o;

    const fs::path dir = tempDir("planar8");
    BackendWriteResult r = backend.writeFrame(dir.string(), in);
    CHECK(r.ok);
    CHECK(r.error.empty());
    CHECK(r.filesWritten.size() == 3);
    CHECK(fs::path(r.filesWritten[0]).filename() == "frame_9_y.pgm");
    CHECK(fs::path(r.filesWritten[1]).filename() == "frame_9_u.pgm");
    CHECK(fs::path(r.filesWritten[2]).filename() == "frame_9_v.pgm");

    const char* expectedHeaders[3] = {"P5\n5 3\n255\n", "P5\n3 2\n255\n", "P5\n3 2\n255\n"};
    for (int p = 0; p < 3; ++p) {
        std::vector<uint8_t> got;
        CHECK(readFileBytes(r.filesWritten[p], got));
        CHECK(headerOf(got) == expectedHeaders[p]);
        // Byte-equal to the shared writer invoked directly on the same plane.
        const int cw = (w + 1) / 2, ch = (h + 1) / 2;
        const int pw = p == 0 ? w : cw;
        const int ph = p == 0 ? h : ch;
        const fs::path ref = tempDir("planar8_ref")
            / ("frame_9_" + std::string(p == 0 ? "y" : p == 1 ? "u" : "v") + ".pgm");
        CHECK(writePgm(ref.string(), pw, ph, o.plane[p].data(),
                       o.linesize[p], o.color.bitDepth));
        std::vector<uint8_t> refBytes;
        CHECK(readFileBytes(ref, refBytes));
        CHECK(bytesEqual(got, refBytes));
    }
}

// --- 4. planar 10-bit YUV420P10LE: maxval 1023, writePgm-equal ----------------

static void testPlanar10Bit() {
    const int w = 4, h = 2;
    Observation o = makePlanar420(AV_PIX_FMT_YUV420P10LE, 10, w, h);

    PnmOutputBackend backend;
    BackendFrameInput in;
    in.frameIndex = 0;
    in.observation = &o;

    const fs::path dir = tempDir("planar10");
    BackendWriteResult r = backend.writeFrame(dir.string(), in);
    CHECK(r.ok);
    CHECK(r.filesWritten.size() == 3);

    CHECK(headerIs(r.filesWritten[0], "P5\n4 2\n1023\n"));
    CHECK(headerIs(r.filesWritten[1], "P5\n2 1\n1023\n"));
    CHECK(headerIs(r.filesWritten[2], "P5\n2 1\n1023\n"));

    const int cw = (w + 1) / 2, ch = (h + 1) / 2;
    const int pw[3] = {w, cw, cw}, ph[3] = {h, ch, ch};
    for (int p = 0; p < 3; ++p) {
        const fs::path ref = tempDir("planar10_ref")
            / ("f" + std::to_string(p) + ".pgm");
        CHECK(writePgm(ref.string(), pw[p], ph[p], o.plane[p].data(),
                       o.linesize[p], o.color.bitDepth));
        std::vector<uint8_t> got, refBytes;
        CHECK(readFileBytes(r.filesWritten[p], got));
        CHECK(readFileBytes(ref, refBytes));
        CHECK(bytesEqual(got, refBytes));
    }
}

// --- 5. null backend: the bypass ----------------------------------------------

static void testNullBackend() {
    const fs::path dir = tempDir("null");
    Observation o = makePlanar420(AV_PIX_FMT_YUV420P, 8, 4, 2);

    std::string err;
    auto backend = makeOutputBackend("null", err);
    CHECK(err.empty());
    CHECK(backend != nullptr);

    BackendFrameInput in;
    in.frameIndex = 12;
    in.observation = &o;
    BackendWriteResult r = backend->writeFrame(dir.string(), in);
    CHECK(r.ok);
    CHECK(r.error.empty());
    CHECK(r.filesWritten.empty());
    CHECK(entryCount(dir) == 0); // zero output artifacts, directory untouched
}

// --- 6. fail-closed on missing/unusable inputs --------------------------------

static void testFailClosed() {
    PnmOutputBackend backend;

    // Null observation.
    BackendFrameInput in;
    in.frameIndex = 4;
    const fs::path dir = tempDir("failclosed");
    BackendWriteResult r = backend.writeFrame(dir.string(), in);
    CHECK(!r.ok);
    CHECK(!r.error.empty());
    CHECK(r.filesWritten.empty());
    CHECK(entryCount(dir) == 0);

    // RGB absent and a format the planar evidence path cannot address: the
    // error must name the offending pixel format.
    Observation rgb24;
    rgb24.width = 4;
    rgb24.height = 4;
    rgb24.avPixelFormat = AV_PIX_FMT_RGB24;
    rgb24.color.pixelFormat = AV_PIX_FMT_RGB24;
    rgb24.planeCount = 1;
    BackendFrameInput in2;
    in2.frameIndex = 5;
    in2.observation = &rgb24;
    r = backend.writeFrame(dir.string(), in2);
    CHECK(!r.ok);
    CHECK(r.error.find("rgb24") != std::string::npos);
    CHECK(r.error.find("as planar PGM evidence") != std::string::npos);
    CHECK(r.filesWritten.empty());
    CHECK(entryCount(dir) == 0);

    // Supported format but every plane empty: no output plane available.
    Observation empty = makePlanar420(AV_PIX_FMT_YUV420P, 8, 4, 2);
    for (int p = 0; p < 4; ++p) empty.plane[p].clear();
    BackendFrameInput in3;
    in3.frameIndex = 6;
    in3.observation = &empty;
    r = backend.writeFrame(dir.string(), in3);
    CHECK(!r.ok);
    CHECK(r.error.find("no output plane available for frame 6") != std::string::npos);
    CHECK(entryCount(dir) == 0);
}

// --- 7. determinism: repeated writes are byte-identical ------------------------

static void testDeterminism() {
    Observation o = makePlanar420(AV_PIX_FMT_YUV420P10LE, 10, 4, 2);
    const int w = 4, h = 2, stride = w * 3 + 3;
    std::vector<uint8_t> rgb = ramp(static_cast<size_t>(stride) * h, 5);

    PnmOutputBackend backend;
    BackendFrameInput in;
    in.frameIndex = 2;
    in.observation = &o;

    const fs::path a = tempDir("det_a");
    BackendWriteResult ra = backend.writeFrame(a.string(), in);
    CHECK(ra.ok);
    const fs::path b = tempDir("det_b");
    BackendWriteResult rb = backend.writeFrame(b.string(), in);
    CHECK(rb.ok);
    CHECK(ra.filesWritten.size() == rb.filesWritten.size());
    for (size_t i = 0; i < ra.filesWritten.size(); ++i) {
        std::vector<uint8_t> ba, bb;
        CHECK(readFileBytes(ra.filesWritten[i], ba));
        CHECK(readFileBytes(rb.filesWritten[i], bb));
        CHECK(bytesEqual(ba, bb));
    }

    // RGB path: repeated writes byte-identical too.
    in.rgb = &rgb;
    in.rgbStride = stride;
    const fs::path c = tempDir("det_c");
    BackendWriteResult rc1 = backend.writeFrame(c.string(), in);
    const fs::path d = tempDir("det_d");
    BackendWriteResult rc2 = backend.writeFrame(d.string(), in);
    CHECK(rc1.ok && rc2.ok);
    std::vector<uint8_t> bc, bd;
    CHECK(readFileBytes(rc1.filesWritten[0], bc));
    CHECK(readFileBytes(rc2.filesWritten[0], bd));
    CHECK(bytesEqual(bc, bd));
}

int main() {
    testFactory();
    testPnmRgbPath();
    testPlanar8Bit();
    testPlanar10Bit();
    testNullBackend();
    testFailClosed();
    testDeterminism();
    if (failures) {
        std::printf("%d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("anvil_output_backend_tests: all checks passed\n");
    return 0;
}
