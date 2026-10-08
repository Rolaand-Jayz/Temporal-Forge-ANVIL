// BuildProvenance.cpp — WEAK fallback definitions for build provenance.
//
// Every symbol defaults to "" ("not captured"). When the CMake build's
// generated AnvilBuildProvenance.cpp is linked alongside this file, the
// linker resolves the strong definitions and these weak ones are discarded.
// Direct-compiler builds that omit the generated unit (CI g++-direct path)
// keep these definitions, so Manifest detection treats provenance as
// uncaptured at this tier instead of failing to link or fabricating values.
#include "anvil/BuildProvenance.hpp"

namespace anvil::buildprov {

__attribute__((weak)) const char* gitSha = "";
__attribute__((weak)) const char* gitDirty = "";
__attribute__((weak)) const char* gitDirtyHash = "";
__attribute__((weak)) const char* origin = "";

} // namespace anvil::buildprov
