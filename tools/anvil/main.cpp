// main.cpp — anvil_runner: deterministic headless/offline ANVIL pipeline CLI.
//
// Exit codes: 0 success, 1 runtime failure, 2 configuration/usage error.
//
// Configuration parsing is strict (review 4202854436): every numeric field
// uses from_chars over the whole token (no silent prefix consumption, no
// overflow), every enum/mode value is validated, every list token must be
// well-formed, and a configuration error exits 2 BEFORE any experiment
// output is produced. No stoi/stod/stoull exceptions are possible.
#include <charconv>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <optional>
#include <string>
#include <vector>

#include "anvil/Runner.hpp"

namespace fs = std::filesystem;

namespace {

int configError(const std::string& message) {
    std::cerr << "anvil_runner: configuration error: " << message << "\n";
    return 2;
}

// Strict integer parse: whole token, no overflow, no leading/trailing junk.
bool parseIntStrict(const std::string& s, long long& out) {
    if (s.empty()) return false;
    const char* b = s.data();
    const char* e = s.data() + s.size();
    auto [p, ec] = std::from_chars(b, e, out);
    return ec == std::errc{} && p == e;
}

// Strict non-negative integer parse (frame indices, sizes, seed).
bool parseUintStrict(const std::string& s, unsigned long long& out) {
    if (s.empty() || s[0] == '-') return false;
    const char* b = s.data();
    const char* e = s.data() + s.size();
    auto [p, ec] = std::from_chars(b, e, out);
    return ec == std::errc{} && p == e;
}

// Strict float parse: whole token, finite (NaN/Inf are not configurations).
bool parseDoubleStrict(const std::string& s, double& out) {
    if (s.empty()) return false;
    const char* b = s.data();
    const char* e = s.data() + s.size();
    auto [p, ec] = std::from_chars(b, e, out);
    if (ec != std::errc{} || p != e) return false;
    return std::isfinite(out);
}

bool isKnownMode(const std::string& v,
                 std::initializer_list<const char*> allowed) {
    for (const char* a : allowed)
        if (v == a) return true;
    return false;
}

void usage() {
    std::cout
        << "anvil_runner — Temporal Forge / ANVIL successor build-ready runner\n"
        << "\n"
        << "Usage:\n"
        << "  anvil_runner --input FILE --output-dir DIR [options]\n"
        << "\n"
        << "Options:\n"
        << "  --start-frame N          first target frame (decode counter, default 0)\n"
        << "  --start-pts-us N         exact target timestamp in microseconds\n"
        << "                           (exact match; no nearest fallback; mutually\n"
        << "                           exclusive with an explicit --start-frame)\n"
        << "  --frame-count N          number of target frames (default 1)\n"
        << "  --past N                 past window size (default 0 = single-frame control)\n"
        << "  --future N               future window size (default 0)\n"
        << "  --correspondence MODE    codec | estimate | oracle | none (default estimate)\n"
        << "  --visibility MODE        valid | oracle (default valid)\n"
        << "  --geometry MODE          unknown | estimate | oracle (default unknown)\n"
        << "  --no-accumulate          bypass the accumulate stage (passthrough)\n"
        << "  --no-color-convert       bypass explicit working-space conversion\n"
        << "  --cut-frames LIST        comma-separated forced scene-cut frame indices\n"
        << "  --auto-scene-cut         enable deterministic luma-diff cut detection\n"
        << "  --auto-cut-threshold X   mean luma diff threshold (default 12)\n"
        << "  --oracle-dir DIR         oracle fixture directory\n"
        << "  --dump-dir DIR           intermediate dump directory\n"
        << "  --dump-stages LIST       comma-separated stage names or 'all'\n"
        << "  --ground-truth F=PATH    attach same-res or higher-res reference truth (repeatable)\n"
        << "  --seed N                 recorded in manifest; pipeline is deterministic\n";
}

} // namespace

int main(int argc, char** argv) {
    anvil::RunConfig cfg;
    bool haveInput = false, haveOutput = false;
    bool haveStartFrame = false, haveStartPts = false;
    // Validates dump-stages tokens against the real stage vocabulary.
    bool dumpStagesOk = true;

    for (int i = 1; i < argc; ++i) {
        const std::string arg = argv[i];
        auto next = [&](std::string& dst) -> bool {
            if (i + 1 >= argc) return false;
            dst = argv[++i];
            return true;
        };
        if (arg == "--help" || arg == "-h") {
            usage();
            return 0;
        } else if (arg == "--input") {
            if (!next(cfg.inputPath)) return configError("--input requires a value");
            haveInput = true;
        } else if (arg == "--output-dir") {
            if (!next(cfg.outputDir)) return configError("--output-dir requires a value");
            haveOutput = true;
        } else if (arg == "--start-frame") {
            std::string v;
            long long n = 0;
            if (!next(v) || !parseIntStrict(v, n) || n < 0)
                return configError("--start-frame requires a non-negative integer, got '"
                                   + v + "'");
            haveStartFrame = true;
            cfg.startFrame = n;
        } else if (arg == "--start-pts-us") {
            std::string v;
            unsigned long long n = 0;
            if (!next(v) || !parseUintStrict(v, n))
                return configError("--start-pts-us requires a non-negative integer");
            haveStartPts = true;
            cfg.startPtsUs = static_cast<int64_t>(n);
        } else if (arg == "--frame-count") {
            std::string v;
            long long n = 0;
            if (!next(v) || !parseIntStrict(v, n) || n < 1)
                return configError("--frame-count requires an integer >= 1");
            cfg.frameCount = n;
        } else if (arg == "--past") {
            std::string v;
            long long n = 0;
            if (!next(v) || !parseIntStrict(v, n) || n < 0)
                return configError("--past requires a non-negative integer");
            cfg.past = static_cast<int>(n);
        } else if (arg == "--future") {
            std::string v;
            long long n = 0;
            if (!next(v) || !parseIntStrict(v, n) || n < 0)
                return configError("--future requires a non-negative integer");
            cfg.future = static_cast<int>(n);
        } else if (arg == "--correspondence") {
            if (!next(cfg.correspondenceMode)) return configError("--correspondence requires a value");
            if (!isKnownMode(cfg.correspondenceMode, {"codec", "estimate", "oracle", "none"}))
                return configError("unknown correspondence mode '" + cfg.correspondenceMode
                                   + "' (codec | estimate | oracle | none)");
        } else if (arg == "--visibility") {
            if (!next(cfg.visibilityMode)) return configError("--visibility requires a value");
            if (!isKnownMode(cfg.visibilityMode, {"valid", "oracle"}))
                return configError("unknown visibility mode '" + cfg.visibilityMode
                                   + "' (valid | oracle)");
        } else if (arg == "--geometry") {
            if (!next(cfg.geometryMode)) return configError("--geometry requires a value");
            if (!isKnownMode(cfg.geometryMode, {"unknown", "estimate", "oracle"}))
                return configError("unknown geometry mode '" + cfg.geometryMode
                                   + "' (unknown | estimate | oracle)");
        } else if (arg == "--no-accumulate") {
            cfg.accumulateEnabled = false;
        } else if (arg == "--no-color-convert") {
            cfg.colorConvertEnabled = false;
        } else if (arg == "--cut-frames") {
            std::string v;
            if (!next(v)) return configError("--cut-frames requires a value");
            cfg.forcedCutFrames.clear();
            size_t pos = 0;
            while (pos <= v.size()) {
                const size_t comma = v.find(',', pos);
                const std::string tok = v.substr(
                    pos, comma == std::string::npos ? std::string::npos : comma - pos);
                long long n = 0;
                if (tok.empty() || !parseIntStrict(tok, n) || n < 0)
                    return configError("--cut-frames entries must be non-negative "
                                       "integers; rejected token '" + tok + "'");
                cfg.forcedCutFrames.push_back(n);
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
        } else if (arg == "--auto-scene-cut") {
            cfg.autoSceneCut = true;
        } else if (arg == "--auto-cut-threshold") {
            std::string v;
            double d = 0;
            if (!next(v) || !parseDoubleStrict(v, d) || d < 0)
                return configError("--auto-cut-threshold requires a finite non-negative number");
            cfg.autoCutThreshold = d;
        } else if (arg == "--oracle-dir") {
            if (!next(cfg.oracleDir)) return configError("--oracle-dir requires a value");
        } else if (arg == "--dump-dir") {
            if (!next(cfg.dumpDir)) return configError("--dump-dir requires a value");
        } else if (arg == "--dump-stages") {
            if (!next(cfg.dumpStages)) return configError("--dump-stages requires a value");
            // Validate tokens now: a typo must fail configuration instead of
            // silently skipping a requested capture.
            dumpStagesOk = true;
            size_t pos = 0;
            while (pos <= cfg.dumpStages.size()) {
                const size_t comma = cfg.dumpStages.find(',', pos);
                const std::string tok = cfg.dumpStages.substr(
                    pos, comma == std::string::npos ? std::string::npos : comma - pos);
                bool ok = false;
                if (tok == "all") ok = true;
                else {
                    bool conv = false;
                    anvil::stageFromName(tok, conv);
                    ok = conv;
                }
                if (!ok) {
                    return configError("unknown dump stage '" + tok + "'");
                }
                if (comma == std::string::npos) break;
                pos = comma + 1;
            }
            (void)dumpStagesOk;
        } else if (arg == "--ground-truth") {
            std::string v;
            if (!next(v)) return configError("--ground-truth requires FRAME=PATH");
            const size_t eq = v.find('=');
            if (eq == std::string::npos || eq == 0 || eq + 1 >= v.size())
                return configError("--ground-truth expects FRAME=PATH");
            long long frame = 0;
            if (!parseIntStrict(v.substr(0, eq), frame) || frame < 0)
                return configError("--ground-truth frame must be a non-negative integer");
            if (!cfg.groundTruth.emplace(frame, v.substr(eq + 1)).second)
                return configError("duplicate ground truth for frame "
                                   + std::to_string(frame));
        } else if (arg == "--seed") {
            std::string v;
            unsigned long long n = 0;
            if (!next(v) || !parseUintStrict(v, n))
                return configError("--seed requires a non-negative integer");
            cfg.seed = n;
        } else {
            return configError("unknown option '" + arg + "'");
        }
    }

    if (!haveInput || !haveOutput) {
        usage();
        return 2;
    }
    if (haveStartFrame && haveStartPts)
        return configError("--start-frame and --start-pts-us are mutually exclusive");

    const anvil::RunResult res = anvil::runPipeline(cfg);
    if (!res.ok) {
        std::cerr << "anvil_runner: error: " << res.error << "\n";
        return 1;
    }
    for (const std::string& f : res.outputFiles)
        std::cout << f << "\n";
    return 0;
}
