// Baseline.cpp — permanent baseline identity verification (fail-closed).
#include "Baseline.hpp"

#include <algorithm>
#include <cstdlib>
#include <filesystem>

#include "anvil/Sha256.hpp"

namespace anvil_lab {

namespace {

bool isHex(const std::string& s, size_t len) {
    if (s.size() != len) return false;
    for (char c : s) {
        const bool ok = (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
        if (!ok) return false;
    }
    return true;
}

// git commit SHA-1 (40 hex); digest fields are SHA-256 (64 hex).
bool isCommitSha(const std::string& s) { return isHex(s, 40); }
bool isSha256Hex(const std::string& s) { return isHex(s, 64); }

} // namespace

std::string sha256StringHex(const std::string& text) {
    return anvil::sha256Hex(reinterpret_cast<const uint8_t*>(text.data()), text.size());
}

bool sha256FileHexLab(const std::string& path, std::string& out) {
    return anvil::sha256FileHex(path, out);
}

bool loadBaseline(const std::string& path, BaselineDef& out, std::string& err) {
    if (!jsonReadFile(path, out.json, err)) return false;
    const std::string name = out.json.at("name").asString();
    if (name != "ANVIL baseline") {
        err = "baseline name must be exactly 'ANVIL baseline'";
        return false;
    }
    out.pinnedCommit = out.json.at("pinned_commit").asString();
    if (!isCommitSha(out.pinnedCommit)) {
        err = "pinned_commit must be a 40-hex git SHA (a moving reference is not "
              "sufficient baseline identity)";
        return false;
    }
    const JsonValue& files = out.json.at("implementation").at("files");
    if (!files.isObject() || files.obj.empty()) {
        err = "implementation.files must map paths to sha256 digests";
        return false;
    }
    for (const auto& kv : files.obj) {
        if (!isSha256Hex(kv.second.asString())) {
            err = "implementation.files['" + kv.first + "'] is not a sha256 digest";
            return false;
        }
    }
    out.configHash = out.json.at("config_hash").asString();
    if (!isSha256Hex(out.configHash)) {
        err = "config_hash must be a sha256 digest";
        return false;
    }
    const std::string recomputed = sha256StringHex(canonicalConfigString(out.json.at("config")));
    if (recomputed != out.configHash) {
        err = "config_hash does not match the recorded canonical configuration";
        return false;
    }
    return true;
}

std::string canonicalConfigString(const JsonValue& manifestConfig) {
    // Semantic pipeline identity: stage modes, windows, switches, ablations,
    // seed, backend. Selection scope (start frame/timestamps, frame count)
    // and filesystem paths (input/output/dump/oracle dirs) are experiment
    // scope, not pipeline identity, and are excluded deliberately.
    JsonValue c = JsonValue::makeObject();
    static const char* const kCopy[] = {
        "past", "future",
        "side_info_normalization_mode", "correspondence_mode",
        "refinement_mode", "visibility_mode", "confidence_mode",
        "geometry_mode", "accumulate_enabled", "color_convert_enabled",
        "forced_cut_frames", "auto_scene_cut", "auto_cut_threshold",
        "excluded_neighbors", "seed", "output_backend",
    };
    for (const char* key : kCopy)
        if (manifestConfig.has(key))
            c.set(key, manifestConfig.at(key));
    return jsonDump(c);
}

VerifyReport verifyBaselineTree(const BaselineDef& def, const std::string& repoRoot) {
    VerifyReport rep;
    const JsonValue& files = def.json.at("implementation").at("files");
    for (const auto& kv : files.obj) {
        const std::string path = repoRoot + "/" + kv.first;
        std::string digest;
        if (!sha256FileHexLab(path, digest)) {
            rep.problems.push_back("baseline implementation file missing: " + kv.first);
            continue;
        }
        if (digest != kv.second.asString()) {
            rep.problems.push_back("baseline implementation file CHANGED vs pinned "
                                   "identity: " + kv.first
                                   + " (expected " + kv.second.asString().substr(0, 12)
                                   + "…, found " + digest.substr(0, 12) + "…)");
        }
    }
    // Shared decoder and provenance sources are part of the reconstruction
    // behavior, even though they live outside src/anvil/. Compare them to
    // the pinned git commit; changes invalidate the baseline label.
    const std::filesystem::path gitDir =
        std::filesystem::path(repoRoot) / ".git";
    if (std::filesystem::exists(gitDir)) {
        const std::string deps =
            "src/media/Demuxer.cpp src/media/Demuxer.hpp "
            "src/media/VideoDecoder.cpp src/media/VideoDecoder.hpp "
            "src/media/TimestampResolve.hpp src/util/Log.cpp src/util/Log.hpp "
            "cmake/AnvilProvenance.cmake";
        const std::string cmd = "git -C \\"" + repoRoot + "\\" diff --quiet "
            + def.pinnedCommit + " -- " + deps + " 2>/dev/null";
        if (std::system(cmd.c_str()) != 0)
            rep.problems.push_back("shared decoder/log/build-provenance dependency "
                "differs from the pinned baseline commit");
    }
    rep.ok = rep.problems.empty();
    if (rep.ok)
        rep.notes.push_back("worktree implementation and shared dependencies match the pinned baseline identity");
    return rep;
}

VerifyReport verifyPinnedCommit(const BaselineDef& def, const std::string& repoRoot) {
    VerifyReport rep;
    const std::string cmd = "git -C \"" + repoRoot + "\" cat-file -e "
        + def.pinnedCommit + "^{commit} 2>/dev/null";
    const int rc = std::system(cmd.c_str());
    if (rc != 0)
        rep.problems.push_back("pinned commit " + def.pinnedCommit
                               + " not present in this repository");
    rep.ok = rep.problems.empty();
    if (rep.ok)
        rep.notes.push_back("pinned commit present; baseline reproducible by checkout");
    return rep;
}

VerifyReport verifyRunIsBaseline(const BaselineDef& def, const JsonValue& manifest,
                                 const std::string& repoRoot) {
    VerifyReport rep = verifyBaselineTree(def, repoRoot);
    const JsonValue& cfg = manifest.at("config");
    const std::string runHash = sha256StringHex(canonicalConfigString(cfg));
    if (runHash != def.configHash) {
        rep.ok = false;
        rep.problems.push_back("run configuration hash " + runHash.substr(0, 12)
                               + "… does not equal the baseline config hash "
                               + def.configHash.substr(0, 12) + "…");
    }
    const std::string runSha = manifest.at("provenance").at("git_sha").asString();
    if (!isCommitSha(runSha)) {
        rep.ok = false;
        rep.problems.push_back("run manifest lacks a valid 40-hex git provenance commit");
    } else {
        // A commit-shaped string is not credible provenance on its own.
        // Require the object to exist and its reconstruction source to be
        // identical to the pinned version. A fabricated or stale SHA fails.
        const std::string cmd = "git -C \\"" + repoRoot + "\\" cat-file -e "
            + runSha + "^{commit} 2>/dev/null";
        if (std::system(cmd.c_str()) != 0) {
            rep.ok = false;
            rep.problems.push_back("run claims an unknown git commit: " + runSha);
        } else if (runSha != def.pinnedCommit) {
            const std::string src =
                "src/anvil tools/anvil/main.cpp src/media/Demuxer.cpp "
                "src/media/Demuxer.hpp src/media/VideoDecoder.cpp "
                "src/media/VideoDecoder.hpp src/media/TimestampResolve.hpp "
                "src/util/Log.cpp src/util/Log.hpp cmake/AnvilProvenance.cmake";
            const std::string compare = "git -C \\"" + repoRoot + "\\" diff --quiet "
                + def.pinnedCommit + " " + runSha + " -- " + src + " 2>/dev/null";
            if (std::system(compare.c_str()) != 0) {
                rep.ok = false;
                rep.problems.push_back("run commit has changed reconstruction sources "
                    "versus the canonical baseline");
            } else {
                rep.notes.push_back("different run commit verified to contain "
                    "the same ANVIL reconstruction and decoder sources");
            }
        }
    }
    return rep;
}

} // namespace anvil_lab
