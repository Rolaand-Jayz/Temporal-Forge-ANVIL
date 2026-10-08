// BuildProvenance.hpp — build-embedded Git source identity for ANVIL.
//
// Strong/weak scheme (review 4209766186):
//  - CMake builds compile the GENERATED translation unit
//    <build-dir>/anvil_provenance/AnvilBuildProvenance.cpp (written by
//    cmake/AnvilProvenance.cmake). It provides STRONG definitions captured
//    at BUILD time — the anvil_provenance custom target re-runs the capture
//    on every build and rewrites the file only when the Git state actually
//    changes, so a rebuilt binary always carries the identity of the tree it
//    was built from (never the identity at configure time).
//  - Builds that compile only src/anvil/*.cpp (the CI g++-direct path)
//    omit the generated unit. The WEAK definitions in
//    src/anvil/BuildProvenance.cpp default every value to "" so linking
//    still succeeds; detection then falls through to the compile-macro tier
//    (ANVIL_GIT_SHA / ANVIL_GIT_DIRTY, which that path defines fresh at
//    compile time; the dirty hash is honestly null on that path) or to
//    runtime Git.
//
// Provenance truthfulness contract: "" means "not captured", never an
// invented value. Callers (Manifest detection) must validate before use.
#pragma once

namespace anvil::buildprov {

// "git rev-parse HEAD" at build time; 40-hex, or "" when unavailable.
extern const char* gitSha;
// "true"/"false" from "git status --porcelain" at build time; "" when the
// repository state could not be captured.
extern const char* gitDirty;
// SHA-256 over the concatenation of the raw "git status --porcelain" output
// and the raw "git diff HEAD" output at build time (lowercase 64-hex) when
// the tree was dirty; "" when the tree was clean, not a repository, or the
// capture failed. Untracked files are covered by name/status only.
extern const char* gitDirtyHash;
// Identity marker of the capture mechanism itself: "build_generated".
extern const char* origin;

} // namespace anvil::buildprov
