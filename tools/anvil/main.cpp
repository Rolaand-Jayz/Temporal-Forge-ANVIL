// main.cpp — anvil_runner: deterministic headless/offline ANVIL pipeline CLI.
//
// Exit codes: 0 success, 1 runtime failure, 2 usage error.
#include <cstring>
#include <filesystem>
#include <iostream>
#include <string>
#include <vector>

#include "anvil/Runner.hpp"

namespace fs = std::filesystem;

namespace {

void usage() {
    std::cout
        << "anvil_runner — Temporal Forge / ANVIL successor build-ready runner\n"
        << "\n"
        << "Usage:\n"
        << "  anvil_runner --input FILE --output-dir DIR [options]\n"
        << "\n"
        << "Options:\n"
        << "  --start-frame N          first target frame (decode counter, default 0)\n"
        << "  --frame-count N          number of target frames (default 1)\n"
        << "  --past N                 past window size (default 0 = single-frame control)\n"
        << "  --future N               future window size (default 0)\n"
        << "  --correspondence MODE    codec | estimate | oracle | none (default estimate)\n"
        << "  --visibility MODE        valid | oracle (default valid)\n"
        << "  --geometry MODE          unknown | oracle (default unknown)\n"
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

bool parseNumber(const std::string& s, long long& out) {
    try {
        size_t pos = 0;
        out = std::stoll(s, &pos);
        return pos == s.size();
    } catch (...) {
        return false;
    }
}

std::vector<int64_t> parseCutList(const std::string& s) {
    std::vector<int64_t> out;
    size_t pos = 0;
    while (pos <= s.size()) {
        const size_t comma = s.find(',', pos);
        const std::string tok =
            s.substr(pos, comma == std::string::npos ? std::string::npos : comma - pos);
        long long v = 0;
        if (!tok.empty() && parseNumber(tok, v)) out.push_back(v);
        if (comma == std::string::npos) break;
        pos = comma + 1;
    }
    return out;
}

} // namespace

int main(int argc, char** argv) {
    anvil::RunConfig cfg;
    bool haveInput = false, haveOutput = false;

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
            haveInput = next(cfg.inputPath);
        } else if (arg == "--output-dir") {
            haveOutput = next(cfg.outputDir);
        } else if (arg == "--start-frame" || arg == "--frame-count") {
            std::string v;
            long long n = 0;
            if (!next(v) || !parseNumber(v, n)) return 2;
            if (arg == "--start-frame") cfg.startFrame = n;
            else cfg.frameCount = n;
        }
        else if (arg == "--past") { std::string v; if (!next(v)) return 2; cfg.past = std::stoi(v); }
        else if (arg == "--future") { std::string v; if (!next(v)) return 2; cfg.future = std::stoi(v); }
        else if (arg == "--correspondence") { if (!next(cfg.correspondenceMode)) return 2; }
        else if (arg == "--visibility") { if (!next(cfg.visibilityMode)) return 2; }
        else if (arg == "--geometry") { if (!next(cfg.geometryMode)) return 2; }
        else if (arg == "--no-accumulate") { cfg.accumulateEnabled = false; }
        else if (arg == "--no-color-convert") { cfg.colorConvertEnabled = false; }
        else if (arg == "--cut-frames") { std::string v; if (!next(v)) return 2; cfg.forcedCutFrames = parseCutList(v); }
        else if (arg == "--auto-scene-cut") { cfg.autoSceneCut = true; }
        else if (arg == "--auto-cut-threshold") { std::string v; if (!next(v)) return 2; cfg.autoCutThreshold = std::stod(v); }
        else if (arg == "--oracle-dir") { if (!next(cfg.oracleDir)) return 2; }
        else if (arg == "--dump-dir") { if (!next(cfg.dumpDir)) return 2; }
        else if (arg == "--dump-stages") { if (!next(cfg.dumpStages)) return 2; }
        else if (arg == "--ground-truth") {
            std::string v;
            if (!next(v)) return 2;
            const size_t eq = v.find('=');
            if (eq == std::string::npos || eq == 0 || eq + 1 >= v.size()) {
                std::cerr << "--ground-truth expects FRAME=PATH\n";
                return 2;
            }
            long long frame = 0;
            if (!parseNumber(v.substr(0, eq), frame) || frame < 0) {
                std::cerr << "--ground-truth frame must be a non-negative integer\n";
                return 2;
            }
            if (!cfg.groundTruth.emplace(frame, v.substr(eq + 1)).second) {
                std::cerr << "duplicate ground truth for frame " << frame << "\n";
                return 2;
            }
        }
        else if (arg == "--seed") { std::string v; if (!next(v)) return 2; cfg.seed = std::stoull(v); }
        else {
            std::cerr << "unknown option: " << arg << "\n";
            return 2;
        }
    }

    if (!haveInput || !haveOutput) {
        usage();
        return 2;
    }

    const anvil::RunResult res = anvil::runPipeline(cfg);
    if (!res.ok) {
        std::cerr << "anvil_runner: error: " << res.error << "\n";
        return 1;
    }
    for (const std::string& f : res.outputFiles)
        std::cout << f << "\n";
    return 0;
}
