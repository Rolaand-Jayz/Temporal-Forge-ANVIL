# Temporal Forge current state

**Status:** CURRENT — **FSR ERA CLOSED · ANVIL BUILD-READY CANDIDATE ACTIVE**  
**As of:** 2026-10-08  
**Historical closure base:** `main` @ `285a5788f89787bce0ca26f8e8e8ca312890723f`  
**Successor branch:** `successor/anvil-build-ready`  
**Draft review:** PR #1 — **DO NOT MERGE without explicit maintainer authorization**  
**Current implementation/evidence head before this documentation reconciliation:** `5f10b26f84ba89293e1b38622521acde37ee0732`  
**Exact-head CI:** Actions run `37855358002` — SUCCESS

## Current program state

Temporal Forge now has two deliberately separate states:

1. The **FSR-centered research era is closed**. Its evidence, negative results, provenance boundaries, historical player, and closure adjudication remain preserved and must not be reinterpreted as an active FSR campaign.
2. The **Temporal Forge / ANVIL successor program is active**. A minimum scientifically testable build-ready candidate exists on `successor/anvil-build-ready` and is being qualified under the authorized ANVIL execution pack.

The successor does not inherit FSR-specific jitter, motion, history, scaling, composition, tensor, or backend assumptions merely because they existed in the historical player.

## Current ANVIL candidate

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

The independent evaluator pass on 2026-10-07 left seven findings open: meaningful residual refinement, executable estimated sample geometry, independent side-info normalization, replaceable output backend, non-stale build provenance, native timestamp identity, and correct sample-geometry/confidence documentation order.

All seven now have implementation repairs and focused regressions on the successor branch. Their GitHub review threads remain intentionally unresolved until the independent evaluator verifies the repaired head. Therefore:

```text
BUILD_READY_FOR_RESEARCH = CANDIDATE / PENDING INDEPENDENT EVALUATOR
```

This repository must not self-promote that value to TRUE merely because internal repair/review loops or CI are green.

## Current validation snapshot

Exact implementation/evidence head `5f10b26f84ba89293e1b38622521acde37ee0732`:

- GitHub Actions run `37855358002`: **SUCCESS**
- Arch build + CTest: **32/32 executed tests passed**
  - 1 additional provenance regression skipped in this CI environment
  - 4 historical GPU tests remain disabled by design
- Python CI contract job: **124 passed**
  - 68 historical Python tests
  - 56 ANVIL contract tests
- Current ANVIL test inventory also includes dedicated C++ refinement, geometry-estimate, side-info, output-backend, timestamp, codec, core, and runner targets.

Green CI is necessary evidence, not an independent gate decision.

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
