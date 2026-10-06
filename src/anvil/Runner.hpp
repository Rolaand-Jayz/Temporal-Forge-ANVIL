// Runner.hpp — ANVIL deterministic headless/offline pipeline runner.
//
// Integrates: decode (reused Qt-free FFmpeg shell, software mode) → temporal
// window selection (past/future, cut-aware) → correspondence (codec |
// estimate | oracle, provenance-preserving) → visibility → sample geometry →
// explicit color handling → confidence-weighted accumulate → non-FSR output.
// Every stage is bypassable; every consequential stage is dumpable; all
// observable behavior lands in the run manifest.
#pragma once
#include <string>
#include <vector>

#include "Manifest.hpp"

namespace anvil {

struct RunConfig {
    std::string inputPath;
    std::string outputDir;   // required; manifest.json + frames written here
    int64_t startFrame = 0;
    int64_t frameCount = 1;
    int past = 0;
    int future = 0;
    // correspondence: codec | estimate | oracle | none
    std::string correspondenceMode = "estimate";
    // visibility: valid | oracle
    std::string visibilityMode = "valid";
    // geometry: unknown | oracle
    std::string geometryMode = "unknown";
    bool accumulateEnabled = true;
    bool colorConvertEnabled = true;
    std::vector<int64_t> forcedCutFrames;
    bool autoSceneCut = false;
    double autoCutThreshold = 12.0;
    std::string oracleDir; // empty = no oracles
    std::string dumpDir;   // empty = no dumps
    std::string dumpStages; // comma list of stage names to dump
    uint64_t seed = 0;      // recorded; no stochastic component exists
};

struct RunResult {
    bool ok = false;
    std::string error;
    Manifest manifest;
    std::vector<std::string> outputFiles;
};

RunResult runPipeline(const RunConfig& config);

} // namespace anvil
