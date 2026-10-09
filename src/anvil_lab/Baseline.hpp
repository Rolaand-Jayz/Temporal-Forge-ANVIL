// Baseline.hpp — permanent ANVIL baseline identity and fail-closed verifier.
//
// The baseline identity is pinned by SOURCE IDENTITY, never by a moving
// reference ("main", "HEAD", "latest", or a floating tag are insufficient):
// an exact commit SHA plus the SHA-256 of every implementation file that
// defines the baseline, plus the exact canonical configuration and its
// hash. Verification fails closed on any mismatch, and reproduction of the
// baseline after later changes proceeds by checking out the pinned commit
// (documented in the baseline record itself).
#pragma once
#include <string>
#include <vector>

#include "Json.hpp"

namespace anvil_lab {

// Reads and structurally validates a baseline definition
// (exhibitions/*/BASELINE.json). The definition must pin:
//   pinned_commit            — 40-hex git commit
//   implementation.files     — {repo-relative path: sha256}
//   config                   — canonical semantic configuration object
//   config_hash              — sha256 over the canonical config string
struct BaselineDef {
    JsonValue json;
    std::string pinnedCommit;
    std::string configHash;
};

bool loadBaseline(const std::string& path, BaselineDef& out, std::string& err);

// Canonical config string used for hashing: jsonDump of the semantic
// config fields (selection scope/paths deliberately excluded — they are
// experiment scope, not pipeline identity).
std::string canonicalConfigString(const JsonValue& manifestConfig);
std::string sha256StringHex(const std::string& text);

struct VerifyReport {
    bool ok = false;
    std::vector<std::string> problems;
    std::vector<std::string> notes;
};

// Verifies the CURRENT worktree still contains the pinned baseline
// implementation (file hashes) and that the pinned commit exists in the
// repository. Fails closed on any difference: a future merged candidate
// cannot silently redefine the baseline, and any regenerated baseline run
// from a changed tree is rejected.
VerifyReport verifyBaselineTree(const BaselineDef& def, const std::string& repoRoot);

// Verifies a run manifest really is the canonical baseline configuration:
// config hash equality (fail closed) + provenance sanity. The run's git
// SHA may differ from the pinned commit when the anvil implementation tree
// is byte-identical (e.g. the exhibition branch), which
// verifyBaselineTree already proved; provenance is recorded either way.
VerifyReport verifyRunIsBaseline(const BaselineDef& def, const JsonValue& manifest,
                                 const std::string& repoRoot);

// File sha256 (reuses the anvil FIPS 180-4 implementation).
bool sha256FileHexLab(const std::string& path, std::string& out);

} // namespace anvil_lab
