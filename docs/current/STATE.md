# Temporal Forge current state

**Status:** CURRENT — **FSR ERA CLOSED · ANVIL BUILD-READY GATE ADJUDICATED TRUE · SUCCESSOR MERGED TO `main`**  
**As of:** 2026-10-09  
**Historical closure base:** `285a5788f89787bce0ca26f8e8e8ca312890723f` (pre-successor `main`)  
**Successor line:** PR #1 **merged** into `main` at `36952dcee574040decdfc96d8143c5bae6cd3895` (2026-10-09, maintainer-authorized); `successor/anvil-build-ready` retained as merged history  
**Independent evaluator verdict:** **CLEAN** at `fe7dac22` — "no unresolved evaluator findings remain … no remaining basis to withhold `BUILD_READY_FOR_RESEARCH = TRUE`" (2026-10-09)  
**Implementation/evidence head:** `b6fbc7b43b25399ce34bfdddfcbf1bf5dc5e335e` — literal branch-head Actions run `37875251102` SUCCESS  
**Merged-`main` head CI:** Actions run `37890647442` on `36952dce` — SUCCESS

## Current program state

Temporal Forge now has two deliberately separate states:

1. The **FSR-centered research era is closed**. Its evidence, negative results, provenance boundaries, historical player, and closure adjudication remain preserved and must not be reinterpreted as an active FSR campaign.
2. The **Temporal Forge / ANVIL successor program passed its Build-Ready gate**. The successor rig lives on `main` (PR #1 merged, `36952dce`); `BUILD_READY_FOR_RESEARCH = TRUE` was adjudicated on 2026-10-09 by the independent evaluator's CLEAN verdict plus the maintainer's merge authorization. The later adversarial research campaign has **not** been launched.

The successor does not inherit FSR-specific jitter, motion, history, scaling, composition, tensor, or backend assumptions merely because they existed in the historical player.

## Current ANVIL successor

The current successor is a deterministic, headless/offline, CPU ANVIL research rig isolated under `src/anvil/`. Its active pipeline is:

```text
decode
→ window_select
→ side_info_normalization
→ correspondence
→ correspondence_refinement
→ visibility
→ sample_geometry
→ confidence
→ color_convert
→ accumulate
→ output backend
```

The order of `sample_geometry` before `confidence` is intentional: estimated confidence measures the geometry-adjusted warp.

Current research controls include:

- configurable past/future temporal windows and exact per-neighbor ablation;
- exact frame, microsecond timestamp, and native-tick timestamp selection;
- explicit raw codec side-info normalization with a truthful bypass;
- coarse image correspondence, meaningful residual refinement, strict oracle correspondence, codec side-info mode, and no-correspondence control;
- visibility and confidence controls/oracles;
- sample geometry with reachable `unknown`, `estimated`, and `known/oracle` states;
- deterministic temporal reconstruction independent of FSR;
- replaceable `pnm` and `null` output backends;
- per-stage timings/dumps, run manifests, input/oracle hashes, exact build/Git provenance, and deterministic replay;
- HR reference attachment for controlled fixtures without reconstruction contamination.

See [`ANVIL_SUCCESSOR_ARCHITECTURE.md`](ANVIL_SUCCESSOR_ARCHITECTURE.md) for the detailed interface and evidence model.

## Qualification and evaluator state

The governing Build-Ready contract is:

[`../../zcode_packs/temporal_forge_anvil_zcode_pack_2026-10-05/BUILD_READY_CONTRACT.md`](../../zcode_packs/temporal_forge_anvil_zcode_pack_2026-10-05/BUILD_READY_CONTRACT.md)

The current evidence record is:

[`ANVIL_BUILD_READY_QUALIFICATION_20261005.md`](ANVIL_BUILD_READY_QUALIFICATION_20261005.md)

The independent evaluator pass on 2026-10-07 left seven findings open (meaningful residual refinement, executable estimated sample geometry, independent side-info normalization, replaceable output backend, non-stale build provenance, native timestamp identity, and correct sample-geometry/confidence documentation order); later evaluator rounds added and repaired further findings (ignored-input provenance coverage, literal-head CI validation, dirty-fingerprint completeness, missing-timestamp µs selection). Every finding was repaired with focused regressions, and the evaluator re-reviewed each repaired head. The final re-review at `fe7dac22` (2026-10-09) was **CLEAN** with no unresolved findings and "no remaining basis to withhold `BUILD_READY_FOR_RESEARCH = TRUE`". The maintainer then merged PR #1. Therefore:

```text
BUILD_READY_FOR_RESEARCH = TRUE   (adjudicated 2026-10-09: evaluator CLEAN + maintainer merge)
```

This value was never self-promoted: internal repair/review loops and green CI were recorded as evidence only, and the gate closed only on the independent evaluator's verdict and the maintainer's explicit merge authorization.

## Home Field Exhibition & Visual Review Lab — preliminary, unmerged

The Home Field Exhibition and Visual Review Lab are being developed in **unmerged PR #4** on
`feature/anvil-home-field-exhibition`. The canonical **ANVIL baseline** is
defined by the original pinned source/configuration identity at `f2f8b992`;
that *definition* is separate from whether any particular output run is
scientifically qualified. The merged Build-Ready gate above remains TRUE,
but it does not establish image-quality improvement.

The original four-scene experiment (two analytic scenes and two Big Buck Bunny
excerpts) reported native PSNR gains over decoded-frame controls, but the
tracked run manifests were produced **before executable-byte attestation**,
report `git_dirty: "true"`, and lack the now-required
`exhibition_attestation`. The Blender upstream master is now pinned by
digest in [`exhibitions/home_field_2026-10/SOURCES.md`](../../exhibitions/home_field_2026-10/SOURCES.md),
but no pinned-source, attested regeneration has been executed yet. These
are **historical preliminary results**, not
current-head reproduced or qualified scientific evidence. Refinement+confidence
was not the best arm by every metric or scene; there is no universal winner
claim. The earlier 29/29 Chromium and Firefox browser checks are also
**pre-repair** evidence, not current-head UX verification.

The current qualification requirements and explicit limitations are in
[the Home Field Exhibition report](../../exhibitions/home_field_2026-10/REPORT.md)
and [operator guide](../../exhibitions/home_field_2026-10/OPERATOR_GUIDE.md).
New experiments must be regenerated with validated input fingerprints,
attested runner bytes and matched PTS/image hashes; visual checks must be
repeated on the final repaired PR head. CI success alone does not fulfill
these scientific and human-review gates. PR #4 is not approved or merged;
Spring Training policy PR #3 remains separate and unmerged.

## Current validation snapshot

- Implementation/evidence head `b6fbc7b43b25399ce34bfdddfcbf1bf5dc5e335e`: literal branch-head Actions run `37875251102` — **SUCCESS** (both jobs verified this exact SHA)
- Final evaluator-reviewed branch head `fe7dac22` (documentation-only beyond `b6fbc7b4`): evaluator verdict **CLEAN**
- Merged `main` head `36952dce`: Actions run `37890647442` — **SUCCESS**
  - Arch build + CTest: **32 non-disabled tests, 0 failures** — 30 passed normally and 2 asset-dependent tests skipped; 4 additional historical GPU tests were disabled by design
  - Python CI contract job: **124 passed** (68 historical + 56 ANVIL contract tests)
- ANVIL test inventory includes dedicated C++ refinement, geometry-estimate, side-info, output-backend, timestamp, codec, core, runner, and provenance-rebuild targets.

Green CI is necessary evidence, not an independent gate decision; the gate decision is the 2026-10-09 adjudication recorded above.

## FSR-era state

There is **no active FSR quality campaign** and no standing plan to resume FSR-specific expected-input reconstruction, motion/jitter tuning, graph adaptation, or quality promotion.

The closure authority remains [`../closure/README.md`](../closure/README.md), especially:

- [`../closure/FSR41_FINAL_ADJUDICATION_20260915.md`](../closure/FSR41_FINAL_ADJUDICATION_20260915.md)
- [`../closure/CLAIM_EVIDENCE_LEDGER.md`](../closure/CLAIM_EVIDENCE_LEDGER.md)
- [`../closure/LIMITATIONS_AND_OPEN_QUESTIONS.md`](../closure/LIMITATIONS_AND_OPEN_QUESTIONS.md)
- [`../closure/EVALUATION_STANDARD.md`](../closure/EVALUATION_STANDARD.md)

The preserved historical player remains an operational GPU-native Linux/Vulkan application with its FSR 4.1 RE experimental path, spatial fallback, tests, benchmark tooling, evidence, and provenance record. Those are historical implementation facts, not an ANVIL architectural mandate.

## Repository authority

- Explicit maintainer direction controls authorization, including merge authority.
- The ANVIL execution pack and Build-Ready contract govern successor requirements.
- Executable source/tests establish what the successor actually does.
- The qualification record summarizes evidence but does not outrank the governing contract or independent evaluation.
- Closure documents govern interpretation of the FSR era.
- Archived plans/progress logs remain historical evidence and cannot reactivate a closed campaign by themselves.

The FSR-era record is evidence, not a solution template. The active successor program must remain evidence-driven and vendor-agnostic.
