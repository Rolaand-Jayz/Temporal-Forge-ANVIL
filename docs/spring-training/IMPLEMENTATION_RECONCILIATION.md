# Implementation reconciliation — research policy versus verified system behavior

**Status:** Proposed process, **not** completed reconciliation.
**Purpose:** Integrate these policy documents safely with the independent ZCode Home Field Exhibition / Visual Review Lab work **after** implementation and evaluation.

## 1. Two stages, two kinds of truth

### Stage A — Now: implementation-independent research policy

The policy files in this directory say **what the project intends**, **why it matters**, and **what evidence/authority is needed**.

They deliberately do **not** define:

- Candidate JSON schema or storage keys.
- Database, endpoints, import/export, or manifest syntax.
- Exact class/module names, file paths, UI component structure, or frontend framework.
- Command-line switches or benchmark orchestration commands beyond existing documented ANVIL controls.
- Baseline technical SHA/configuration chosen without inspecting and freezing the real implementation.
- New executable contracts or changes to existing Build-Ready requirements.
- Claims that Home Field Exhibition runs or Spring Training have already occurred.

The concurrent ZCode assignment owns its implementation choices within the maintainer-approved handoff and repository contracts. These documents do not authorize a second agent to rework those choices mid-flight.

### Stage B — After independent implementation review: reconcile and make operational documentation explicit

Once ZCode delivers the Home Field Exhibition branch/PR, and independent evaluation establishes what actually works, write or update **separate implementation documentation** grounded in code, tests, and captures.

Map each policy item to verified actual behavior, or identify a gap for repair/adjudication. The research requirement is not automatically wrong merely because code omitted it; code documentation must not pretend incomplete implementation satisfies policy.

## 2. Reconciliation matrix

The entries below are **questions to verify**, not predictions of exact technical mechanisms.

| Policy commitment | Post-implementation proof/documentation |
|---|---|
| Original ANVIL baseline is immutable | Frozen source/configuration/parameter identity; clean reproduction instructions; checks against moving baseline references |
| Baseline ≠ decoded-frame control | Verified user-facing labels and distinct capture semantics |
| Candidate identity ≠ roster disposition | Technical configuration identity and independently changeable disposition; historical state preserved |
| Six status meanings | Verified accepted vocabulary, transitions, authority checks, change-history behavior |
| Names expose real pipeline changes | Exact stage inventory from validated run, additions versus replacements, bypasses, and scaling method |
| Native reconstruction ≠ upscaling | Grounded input/output dimension reporting and scaling factor semantics |
| Matched scientific comparisons | Validity checks for scene/frame/time, reference, dimensions, color, sample participation, and hashes |
| Human A/B selection is intuitive | Demonstrated search/filter/preview/label behavior at relevant desktop resolution |
| Review modes are functional | Real interaction tests for split, wipe, fade, flicker, subtraction, highlight, region focus, pan/zoom |
| Video, not only stills | Tested frame navigation and matched-sequence inspection |
| Human evidence is retained | Repeatable finding export/import with source and experiment identity |
| Evidence drives decisions; does not make them | Proof against silent promotion/merging, records of explicit maintainer authorization |
| Historical FSR era stays closed | Audit that historical docs and evidence have not been repurposed as successor qualification |

## 3. Safe synchronization protocol

1. Keep this policy PR **separate** and unmerged while the Home Field Exhibition implementation proceeds.
2. Avoid simultaneous changes to `AGENTS.md`, current state/architecture files, existing contracts, experiment source, the original review harness, and ZCode-owned feature files.
3. After ZCode presents its PR, inspect its **exact head SHA**, changed files, actual tests, and captures. Do not infer features from a design document.
4. Compare the proposed policy to the implementation and its authorized brief.
5. Classify each mismatch:
   - **Documentation gap:** feature works but its exact technical behavior is undocumented.
   - **Implementation gap:** policy/approved acceptance criteria require functionality that is missing or incorrect.
   - **Policy ambiguity:** terms or decision rights need explicit clarification.
   - **Genuine design tradeoff:** independently reviewed implementation suggests an alternative meeting the same intent.
6. For documentation gaps, describe tested behavior accurately.
7. For implementation gaps, request a specific repair through the relevant coding branch/PR; do not redefine success to cover the omission.
8. For policy ambiguity or material tradeoffs, record a maintainer decision before altering authoritative intent.
9. Once interfaces and evidence are stable, integrate discoverability links in `docs/README.md` and optionally root `README.md` through a coordinated, reviewed change.
10. Keep baseline snapshots, technical candidate identities, and historic scouting findings reproducible as both documentation and code evolve.

## 4. Authority and approval limits

The authoritative successor Build-Ready requirements remain in the [existing execution-pack contract](../../zcode_packs/temporal_forge_anvil_zcode_pack_2026-10-05/BUILD_READY_CONTRACT.md); the current executable implementation and test evidence establish what has been built, while explicit maintainer direction governs authorization and changes.

This policy is a separate layer defining research selection intent. It is **not** a replacement for the existing Build-Ready contract or a new unreviewed implementation contract.

An evaluator may find incompatibility and request changes, but neither evaluator nor agent may silently merge or promote a candidate without delegated authority.

## 5. Noninterference audit for this PR

Before presenting the standalone policy PR, verify:

- It adds **only** files inside `docs/spring-training/`.
- It does **not** edit root or docs navigation, current implementation documentation, governing contracts, code, tests, or historical evidence.
- It does not claim quality results or roster decisions that have not occurred.
- It does not prescribe data schema/API/UI framework choices to ZCode.
- All links within the new policy directory resolve relative to their destination.
- The PR is left unmerged for independent review.

## 6. Exit criteria for the later reconciliation

Reconciliation is complete only after:

- An independently reviewed implementation satisfies, or openly and explicitly resolves, each applicable accepted policy requirement.
- Technical documentation cites verified features, exact reproducible commands, constraints, and tests.
- Unimplemented requirements are labeled open, not buried in polished prose.
- Repo navigation exposes the policy and technical implementation documents without turning the FSR archives into active plans.
- The original ANVIL baseline remains identifiable independently of the current lineup.

**The rule:** Requirements govern intent; verified code governs descriptions of reality. Disagreements require evidence and a decision, not an edit that erases the disagreement.
