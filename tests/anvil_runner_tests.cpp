// anvil_runner_tests.cpp — end-to-end CLI integration tests for anvil_runner.
//
// Scenarios: single-frame control, past/future window, deterministic replay
// byte-equality, stage bypass, oracle injection, codec mode graceful run,
// manifest integrity, scene-cut observability, missing-input error handling.
// Usage: anvil_runner_tests <runner> <sdr.mp4> <hdr.mp4> <nometa.mp4>
#include <cstdlib>
#include <sys/wait.h>
#include <fstream>
#include <iostream>
#include <string>
#include <sys/stat.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vector>

#include <filesystem>

namespace fs = std::filesystem;

static int failures = 0;
#define CHECK(cond)                                                        \
    do {                                                                   \
        if (!(cond)) {                                                     \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #cond);    \
            ++failures;                                                    \
        }                                                                  \
    } while (0)

static bool fileExists(const fs::path& p) { return fs::exists(p); }

static bool fileEquals(const fs::path& a, const fs::path& b) {
    std::ifstream fa(a, std::ios::binary), fb(b, std::ios::binary);
    if (!fa || !fb) return false;
    std::string sa((std::istreambuf_iterator<char>(fa)), std::istreambuf_iterator<char>());
    std::string sb((std::istreambuf_iterator<char>(fb)), std::istreambuf_iterator<char>());
    return sa == sb;
}

static std::string slurp(const fs::path& p) {
    std::ifstream f(p, std::ios::binary);
    return std::string((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
}

static int runRunner(const std::string& runner, std::vector<std::string> args,
                     const fs::path& outDir) {
    std::error_code ec;
    fs::create_directories(outDir, ec);
    std::string cmd = "\"" + runner + "\"";
    for (const auto& a : args) cmd += " " + a;
    cmd += " > /dev/null 2>&1";
    const int status = std::system(cmd.c_str());
    return WIFEXITED(status) ? WEXITSTATUS(status) : -1;
}

int main(int argc, char** argv) {
    if (argc != 5) {
        std::printf("usage: anvil_runner_tests <runner> <sdr> <hdr> <nometa>\n");
        return 2;
    }
    const std::string runner = argv[1];
    const std::string fixture = argv[2];
    const std::string hdrFixture = argv[3];
    const std::string nometaFixture = argv[4];
    if (!fileExists(fixture) || !fileExists(hdrFixture) || !fileExists(nometaFixture))
        return 77; // deterministic skip without fixtures
    fs::path tmp = fs::temp_directory_path() / ("anvil_runner_tests_" + std::to_string(::getpid()));
    fs::create_directories(tmp);

    // 1. single-frame control
    const int rc1 = runRunner(runner,
        {"--input", fixture, "--output-dir", tmp / "single", "--frame-count", "2",
         "--past", "0", "--future", "0"}, tmp / "single");
    CHECK(rc1 == 0);
    CHECK(fileExists(tmp / "single" / "manifest.json"));
    // full SDR metadata -> explicit YUV->RGB conversion -> PPM output
    CHECK(fileExists(tmp / "single" / "frame_0.ppm"));
    std::string m1 = slurp(tmp / "single" / "manifest.json");
    CHECK(m1.find("\"past\": 0") != std::string::npos ||
          m1.find("\"past\":0") != std::string::npos);
    CHECK(m1.find("git_sha") != std::string::npos);
    CHECK(m1.find("input_sha256") != std::string::npos);
    CHECK(m1.find("codec_capabilities") != std::string::npos);
    CHECK(m1.find("\"h264\"") != std::string::npos);

    // 2. past/future window
    const int rc2 = runRunner(runner,
        {"--input", fixture, "--output-dir", tmp / "window", "--start-frame", "3",
         "--frame-count", "2", "--past", "2", "--future", "2",
         "--correspondence", "estimate"}, tmp / "window");
    CHECK(rc2 == 0);
    CHECK(fileExists(tmp / "window" / "frame_3.ppm"));
    std::string m2 = slurp(tmp / "window" / "manifest.json");
    CHECK(m2.find("\"start_frame\":3") != std::string::npos ||
          m2.find("\"start_frame\": 3") != std::string::npos);

    // 3. deterministic replay: same args, two dirs, byte-identical frames
    const int rc3a = runRunner(runner,
        {"--input", fixture, "--output-dir", tmp / "rep1", "--start-frame", "2",
         "--frame-count", "3", "--past", "1", "--future", "1"}, tmp / "rep1");
    const int rc3b = runRunner(runner,
        {"--input", fixture, "--output-dir", tmp / "rep2", "--start-frame", "2",
         "--frame-count", "3", "--past", "1", "--future", "1"}, tmp / "rep2");
    CHECK(rc3a == 0 && rc3b == 0);
    for (const char* f : {"frame_2.ppm", "frame_3.ppm", "frame_4.ppm"}) {
        CHECK(fileEquals(tmp / "rep1" / f, tmp / "rep2" / f));
    }

    // 4. stage bypass: accumulate disabled still yields passthrough output
    const int rc4 = runRunner(runner,
        {"--input", fixture, "--output-dir", tmp / "bypass", "--frame-count", "1",
         "--past", "2", "--no-accumulate"}, tmp / "bypass");
    CHECK(rc4 == 0);
    CHECK(fileExists(tmp / "bypass" / "frame_0.ppm"));

    // 5. oracle injection
    fs::create_directories(tmp / "oracle");
    {
        std::ofstream f(tmp / "oracle" / "correspondence_1.txt");
        f << "# oracle: identity correspondence for frame 1\n";
        for (int by = 0; by < 64; by += 16)
            for (int bx = 0; bx < 64; bx += 16)
                f << "1 0 0 " << bx << " " << by << " 16 16 0 0 3 P 0 0\n";
    }
    const int rc5 = runRunner(runner,
        {"--input", fixture, "--output-dir", tmp / "oracle_out", "--start-frame", "1",
         "--frame-count", "1", "--past", "1", "--correspondence", "oracle",
         "--oracle-dir", tmp / "oracle"}, tmp / "oracle_out");
    CHECK(rc5 == 0);
    std::string m5 = slurp(tmp / "oracle_out" / "manifest.json");
    CHECK(m5.find("oracle_used") != std::string::npos);
    CHECK(m5.find("\"oracle\"") != std::string::npos);

    // 6. codec mode: must run and record side-info truthfully (h264 fixture
    //    may or may not export MVs — both outcomes are valid, none may crash).
    const int rc6 = runRunner(runner,
        {"--input", fixture, "--output-dir", tmp / "codec", "--frame-count", "3",
         "--correspondence", "codec"}, tmp / "codec");
    CHECK(rc6 == 0);
    std::string m6 = slurp(tmp / "codec" / "manifest.json");
    CHECK(m6.find("side_info_") != std::string::npos ||
          m6.find("available") != std::string::npos);

    // 7. dumps + scene cut observability
    const int rc7 = runRunner(runner,
        {"--input", fixture, "--output-dir", tmp / "dump", "--frame-count", "4",
         "--past", "1", "--cut-frames", "2", "--dump-dir", tmp / "dumps",
         "--dump-stages", "accumulate"}, tmp / "dump");
    CHECK(rc7 == 0);
    std::string m7 = slurp(tmp / "dump" / "manifest.json");
    CHECK(m7.find("window_reset") != std::string::npos);
    CHECK(m7.find("dump_files") != std::string::npos);

    // 7b. HDR (PQ) input: planes preserved, no silent transfer conversion
    const int rc7b = runRunner(runner,
        {"--input", hdrFixture, "--output-dir", tmp / "hdr", "--frame-count", "1",
         "--past", "1"}, tmp / "hdr");
    CHECK(rc7b == 0);
    std::string m7b = slurp(tmp / "hdr" / "manifest.json");
    CHECK(m7b.find("no transfer conversion performed") != std::string::npos);
    CHECK(fileExists(tmp / "hdr" / "frame_0_y.pgm"));
    CHECK(!fileExists(tmp / "hdr" / "frame_0.ppm"));

    // 7c. unspecified color metadata: recorded as unknown, planes preserved
    const int rc7c = runRunner(runner,
        {"--input", nometaFixture, "--output-dir", tmp / "nometa",
         "--frame-count", "1"}, tmp / "nometa");
    CHECK(rc7c == 0);
    std::string m7c = slurp(tmp / "nometa" / "manifest.json");
    CHECK(m7c.find("color metadata incomplete") != std::string::npos);
    CHECK(m7c.find("color_unknown_metadata") != std::string::npos);

    // 8. missing input -> graceful error
    const int rc8 = runRunner(runner,
        {"--input", tmp / "nonexistent.mp4", "--output-dir", tmp / "err"}, tmp / "err");
    CHECK(rc8 == 1);

    std::error_code ec;
    fs::remove_all(tmp, ec);
    if (failures) {
        std::printf("%d check(s) FAILED\n", failures);
        return 1;
    }
    std::printf("anvil_runner_tests: all checks passed\n");
    return 0;
}
