# AnvilProvenance.cmake — build-time Git provenance capture for ANVIL.
#
# Run in script mode (-P) with:
#   cmake -DOUT_FILE=<path-to-AnvilBuildProvenance.cpp> \
#         -DSOURCE_DIR=<repository or worktree root> \
#         -P cmake/AnvilProvenance.cmake
#
# Captures the current Git source identity of SOURCE_DIR and writes a C++
# translation unit defining (STRONG, namespace anvil::buildprov):
#   gitSha       — "git rev-parse HEAD", 40-hex validated, "" when unavailable
#   gitDirty     — "true"/"false" from "git status --porcelain", "" when
#                  Git itself failed
#   gitDirtyHash — SHA-256 (lowercase hex) over a content-complete dirty-tree
#                  fingerprint: porcelain status; a full-index --binary diff
#                  for tracked changes; ordered untracked path names + Git
#                  blob hashes; and ignored src/anvil inputs + blob hashes.
#                  "" when the tree is clean or the repository is unavailable.
#                  Any dirty-state hashing failure is fatal: a build must not
#                  publish an incomplete source identity.
#   origin       — always "build_generated"
#
# Determinism: the file contains no timestamps or other volatile content, so
# an identical Git state produces byte-identical output. CRITICAL for build
# hygiene: the OUT_FILE is rewritten ONLY when its content would actually
# change. The anvil_provenance custom target re-runs this script on every
# build; without the content guard, Ninja would relink on every invocation.

cmake_minimum_required(VERSION 3.24)

if(NOT OUT_FILE OR NOT SOURCE_DIR)
    message(FATAL_ERROR
        "AnvilProvenance.cmake: both -DOUT_FILE= and -DSOURCE_DIR= are required")
endif()

set(ANVIL_PROV_SHA "")
set(ANVIL_PROV_DIRTY "")
set(ANVIL_PROV_DIRTY_HASH "")

execute_process(
    COMMAND git rev-parse HEAD
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE ANVIL_PROV_SHA_RC
    OUTPUT_VARIABLE ANVIL_PROV_SHA_OUT
    ERROR_QUIET)
# Strip the trailing newline for the SHA itself; the porcelain/diff outputs
# stay UNSTRIPPED because the dirty hash is defined over the raw bytes.
string(STRIP "${ANVIL_PROV_SHA_OUT}" ANVIL_PROV_SHA_STRIPPED)
string(LENGTH "${ANVIL_PROV_SHA_STRIPPED}" ANVIL_PROV_SHA_LEN)
if(ANVIL_PROV_SHA_RC EQUAL 0
   AND ANVIL_PROV_SHA_LEN EQUAL 40
   AND ANVIL_PROV_SHA_STRIPPED MATCHES "^[0-9A-Fa-f]+$")
    set(ANVIL_PROV_SHA "${ANVIL_PROV_SHA_STRIPPED}")
endif()

execute_process(
    COMMAND git status --porcelain=v1 --untracked-files=all
    WORKING_DIRECTORY "${SOURCE_DIR}"
    RESULT_VARIABLE ANVIL_PROV_STATUS_RC
    OUTPUT_VARIABLE ANVIL_PROV_STATUS_OUT
    ERROR_VARIABLE ANVIL_PROV_STATUS_ERR)
if(ANVIL_PROV_STATUS_RC EQUAL 0)
    if(ANVIL_PROV_STATUS_OUT STREQUAL "")
        set(ANVIL_PROV_DIRTY "false")
    else()
        set(ANVIL_PROV_DIRTY "true")

        execute_process(
            COMMAND git diff --binary --full-index --no-ext-diff HEAD --
            WORKING_DIRECTORY "${SOURCE_DIR}"
            RESULT_VARIABLE ANVIL_PROV_DIFF_RC
            OUTPUT_VARIABLE ANVIL_PROV_DIFF_OUT
            ERROR_VARIABLE ANVIL_PROV_DIFF_ERR)
        if(NOT ANVIL_PROV_DIFF_RC EQUAL 0)
            message(FATAL_ERROR
                "ANVIL provenance: failed to capture tracked dirty content: "
                "${ANVIL_PROV_DIFF_ERR}")
        endif()

        execute_process(
            COMMAND git ls-files --others --exclude-standard
            WORKING_DIRECTORY "${SOURCE_DIR}"
            RESULT_VARIABLE ANVIL_PROV_UNTRACKED_LIST_RC
            OUTPUT_VARIABLE ANVIL_PROV_UNTRACKED_PATHS
            ERROR_VARIABLE ANVIL_PROV_UNTRACKED_LIST_ERR)
        if(NOT ANVIL_PROV_UNTRACKED_LIST_RC EQUAL 0)
            message(FATAL_ERROR
                "ANVIL provenance: failed to enumerate untracked files: "
                "${ANVIL_PROV_UNTRACKED_LIST_ERR}")
        endif()
        set(ANVIL_PROV_UNTRACKED_HASHES "")
        if(NOT ANVIL_PROV_UNTRACKED_PATHS STREQUAL "")
            execute_process(
                COMMAND git ls-files --others --exclude-standard
                COMMAND git hash-object --no-filters --stdin-paths
                WORKING_DIRECTORY "${SOURCE_DIR}"
                RESULTS_VARIABLE ANVIL_PROV_UNTRACKED_HASH_RCS
                OUTPUT_VARIABLE ANVIL_PROV_UNTRACKED_HASHES
                ERROR_VARIABLE ANVIL_PROV_UNTRACKED_HASH_ERR)
            foreach(ANVIL_PROV_RC IN LISTS ANVIL_PROV_UNTRACKED_HASH_RCS)
                if(NOT ANVIL_PROV_RC EQUAL 0)
                    message(FATAL_ERROR
                        "ANVIL provenance: failed to hash untracked content: "
                        "${ANVIL_PROV_UNTRACKED_HASH_ERR}")
                endif()
            endforeach()
        endif()

        # CONFIGURE_DEPENDS can compile ignored files under src/anvil if they
        # match the source glob, so include those build inputs as well.
        execute_process(
            COMMAND git ls-files --others --ignored --exclude-standard -- src/anvil
            WORKING_DIRECTORY "${SOURCE_DIR}"
            RESULT_VARIABLE ANVIL_PROV_IGNORED_LIST_RC
            OUTPUT_VARIABLE ANVIL_PROV_IGNORED_PATHS
            ERROR_VARIABLE ANVIL_PROV_IGNORED_LIST_ERR)
        if(NOT ANVIL_PROV_IGNORED_LIST_RC EQUAL 0)
            message(FATAL_ERROR
                "ANVIL provenance: failed to enumerate ignored ANVIL inputs: "
                "${ANVIL_PROV_IGNORED_LIST_ERR}")
        endif()
        set(ANVIL_PROV_IGNORED_HASHES "")
        if(NOT ANVIL_PROV_IGNORED_PATHS STREQUAL "")
            execute_process(
                COMMAND git ls-files --others --ignored --exclude-standard -- src/anvil
                COMMAND git hash-object --no-filters --stdin-paths
                WORKING_DIRECTORY "${SOURCE_DIR}"
                RESULTS_VARIABLE ANVIL_PROV_IGNORED_HASH_RCS
                OUTPUT_VARIABLE ANVIL_PROV_IGNORED_HASHES
                ERROR_VARIABLE ANVIL_PROV_IGNORED_HASH_ERR)
            foreach(ANVIL_PROV_RC IN LISTS ANVIL_PROV_IGNORED_HASH_RCS)
                if(NOT ANVIL_PROV_RC EQUAL 0)
                    message(FATAL_ERROR
                        "ANVIL provenance: failed to hash ignored ANVIL input: "
                        "${ANVIL_PROV_IGNORED_HASH_ERR}")
                endif()
            endforeach()
        endif()

        string(SHA256 ANVIL_PROV_DIRTY_HASH
            "status\n${ANVIL_PROV_STATUS_OUT}"
            "tracked-binary-diff\n${ANVIL_PROV_DIFF_OUT}"
            "untracked-paths\n${ANVIL_PROV_UNTRACKED_PATHS}"
            "untracked-blobs\n${ANVIL_PROV_UNTRACKED_HASHES}"
            "ignored-anvil-paths\n${ANVIL_PROV_IGNORED_PATHS}"
            "ignored-anvil-blobs\n${ANVIL_PROV_IGNORED_HASHES}")
    endif()
elseif(NOT ANVIL_PROV_SHA STREQUAL "")
    message(FATAL_ERROR
        "ANVIL provenance: Git HEAD was available but dirty-state capture failed: "
        "${ANVIL_PROV_STATUS_ERR}")
endif()

set(ANVIL_PROV_CONTENT "// AnvilBuildProvenance.cpp — GENERATED by \
cmake/AnvilProvenance.cmake. DO NOT EDIT, DO NOT COMMIT.
// Build-time Git source identity of the source tree this binary was built
// from (review 4209766186: captured per build, not per configure, so a
// rebuilt binary can never report a stale source identity).
// Definitions are STRONG; src/anvil/BuildProvenance.cpp carries the matching
// WEAK fallbacks so direct-compiler builds (CI g++ path) still link.
namespace anvil {
namespace buildprov {
const char* gitSha = \"${ANVIL_PROV_SHA}\";
const char* gitDirty = \"${ANVIL_PROV_DIRTY}\";
const char* gitDirtyHash = \"${ANVIL_PROV_DIRTY_HASH}\";
const char* origin = \"build_generated\";
} // namespace buildprov
} // namespace anvil
")

if(EXISTS "${OUT_FILE}")
    file(READ "${OUT_FILE}" ANVIL_PROV_EXISTING)
    if(ANVIL_PROV_EXISTING STREQUAL ANVIL_PROV_CONTENT)
        # Unchanged Git state: leave the file untouched so Ninja sees no
        # source change and nothing recompiles.
        return()
    endif()
endif()
file(WRITE "${OUT_FILE}" "${ANVIL_PROV_CONTENT}")
