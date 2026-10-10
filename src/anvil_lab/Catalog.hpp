// Catalog.hpp — candidate identity, naming contract, roster vocabulary.
//
// Implements the Home Field Exhibition identity rules:
// - "ANVIL baseline" is the exact canonical name of the frozen original
//   pipeline and never carries a roster-status suffix.
// - ANVIL-derived candidates are named
//     "ANVIL baseline + <mod> [+ <mod>...] — <status>"
//   where each <mod> reflects a REAL configuration difference (added,
//   replaced, disabled, or parameter-changed stage) validated against the
//   recorded pipeline inventory — never guessed from filenames.
// - External methods that do not use ANVIL are external controls and are
//   never named as ANVIL-derived.
// - "upscaled" describes an output resolution increase; same-resolution
//   temporal reconstruction is called temporal reconstruction.
// - Roster disposition is mutable metadata with append-only decision
//   history; identity fields and historical results are immutable, and no
//   status is ever assigned automatically from metrics/CI/branch presence.
#pragma once
#include <set>
#include <string>
#include <vector>

#include "Json.hpp"

namespace anvil_lab {

extern const char* const kBaselineName; // "ANVIL baseline"

// Spring Training roster vocabulary (exact dispositions).
extern const std::set<std::string> kRosterStatuses;

// Build the candidate display name from the modification list and status.
// An empty modification list with a status is INVALID (that is either the
// canonical baseline — no suffix — or a mislabeled record).
std::string buildCandidateName(const std::vector<std::string>& mods,
                               const std::string& rosterStatus);

// Validate a catalog record (JsonValue object). Appends human-readable
// violations to `errors`; returns true when the record is consistent with
// the naming/identity contracts above.
bool validateCandidateRecord(const JsonValue& record,
                             std::vector<std::string>& errors);

// ---- Roster ---------------------------------------------------------------
// Expected roster.json shape:
// { "entries": { "<candidate_id>": {"status": "..."} },
//   "history": [ {"candidate_id","from","to","timestamp","reason",
//                 "evidence_ref","authority","integration_commit"?} ] }

// Applies a status transition with full validation: known statuses only,
// no self-transition noise, `starter` requires merge evidence
// (integration_commit + merged=true evidence ref), the transition is
// appended to history, and NO identity/experiment field is touched.
bool rosterSetStatus(const std::string& rosterPath, const JsonValue& transition,
                     std::string& err);

// Verifies that the mutable roster never drifted from the immutable
// experiment records: every rostered candidate id must exist in the
// catalog, and no history entry may alter recorded evidence.
bool rosterAudit(const JsonValue& roster, const JsonValue& catalog,
                 std::vector<std::string>& errors);

// ---- Configuration diff (comparison identity header data) ---------------
// Diffs two catalog records' pipeline stage inventories into human-readable
// changed[] / unchanged[] entries. Only stages whose mode/parameters differ
// appear in changed[]; the result is derived from validated configuration
// data, never from display names.
JsonValue pipelineDiff(const JsonValue& candidate, const JsonValue& baseline);

} // namespace anvil_lab
