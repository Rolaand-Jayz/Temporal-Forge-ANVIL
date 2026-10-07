// Runner.hpp — ANVIL deterministic headless/offline pipeline runner.
//
// Integrates: decode (reused Qt-free FFmpeg shell, software mode) → temporal
// window selection (past/future, cut-aware) → correspondence (codec |
// estimate | oracle, provenance-preserving) → visibility → sample geometry →
// explicit color handling → confidence-weighted accumulate → non-FSR output.
// Every stage is bypassable; every consequential stage is dumpable; all
// observable behavior lands in the run manifest.
#pragma once
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "Manifest.hpp"

namespace anvil {

struct RunConfig {
    std::string inputPath;
    std::string outputDir;   // required; manifest.json + frames written here
    int64_t startFrame = 0;
    // Exact-timestamp selection: when set, the target sequence starts at the
    // frame whose pts_us equals this value EXACTLY (no nearest fallback).
    // No match, or an ambiguous duplicate timestamp, is a hard error.
    // Mutually exclusive with an explicit --start-frame.
    std::optional<int64_t> startPtsUs;
    int64_t frameCount = 1;
    int past = 0;
    int future = 0;
    // correspondence: codec | estimate | oracle | none
    std::string correspondenceMode = "estimate";
    // refinement: none | local (deterministic +/-1 residual SAD refinement)
    std::string refinementMode = "none";
    // visibility: valid | oracle
    std::string visibilityMode = "valid";
    // confidence: unit | estimate | oracle
    std::string confidenceMode = "unit";
    // geometry: unknown | oracle. "estimate" is rejected until a real
    // estimator exists; a requested experimental arm may never be a no-op.
    std::string geometryMode = "unknown";
    // Exact per-neighbor ablations, keyed by {target, reference}.
    std::vector<std::pair<int64_t, int64_t>> excludedNeighbors;
    bool accumulateEnabled = true;
    bool colorConvertEnabled = true;
    std::vector<int64_t> forcedCutFrames;
    bool autoSceneCut = false;
    double autoCutThreshold = 12.0;
    std::string oracleDir; // empty = no oracles
    std::string dumpDir;   // empty = no dumps
    std::string dumpStages; // comma list of stage names to dump
    // HR ground truth, mapped frame -> file path (reference evidence only;
    // validated + hashed, never fed into reconstruction).
    std::map<int64_t, std::string> groundTruth;
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
