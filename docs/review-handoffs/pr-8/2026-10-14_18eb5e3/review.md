# ANVIL PR #8 — independent evaluation, 2026-10-14

## Classification
**INCOMPLETE — NOT MERGE READY.** At code head `18eb5e3777497c77f2659a915bce6c8fcf61f6c0`, both earlier MAJOR findings have credible source-level repairs and recorded native AMD GPU behavioral evidence, but are **FIXED_UNVERIFIED**, not `fixed_verified`. An exact-head CI job was in progress and full required three clean discovery loops, native Main.qml and neural/FSR failure modes, authenticated pair/browser review, full policy and diff coverage were not independently certified. This is not GitHub approval or merge authorization.

## PR / exact head
- PR: https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8
- Base: `f2f8b9929c1cc3845dab90bbe80d889b44c99390`
- **Reviewed code head**: `18eb5e3777497c77f2659a915bce6c8fcf61f6c0`
- Branch: `feature/anvil-unified`, **open/unmerged** during review.
- This review handoff will be committed later on the branch; that documentation commit must **not** be confused with this reviewed code head, nor may old CI be attributed to a changed head.

## Scope and coverage
Unified branch consolidates the prior #3 (research policies), #5 (Qt player UI), #6 (live ANVIL placement), and #7 (authenticated paired review). GitHub metadata at reviewed head: **69 changed files, 5,420 additions and 370 deletions**, comprising code, documentation, repair logs/tests, and four PNG capture examples. Compared `1882627c3a7e0fe9a8ce3782210db135084afef9` → `18eb5e3777497c77f2659a915bce6c8fcf61f6c0`: six commits including reviewer-handoff documentation and the new native EASU/Qt repair.

Focused inspected surfaces: `PlaybackEngine.cpp/.hpp` decoded/published stage, neural/native/EASU frame selectors, `LivePublicationTracker`, unchanged video image provider, `VideoSurfaceItem.cpp` fail-closed raw-only and QSG scene-graph cache, `tests/native_compare_gpu_tests.cpp`, `tests/CMakeLists.txt`, `test_player_ui_contract.py`, repair-side `native-off.log`, `native-optin.log`, native CTest logs, Qt PNG raw/EASU screenshots, previous comments/DoD, active repo review policy and exact-head CI.

All 13 universal review dimensions considered: requirements, diff inventory, API callers, nominal runtime, errors/recovery, persistence, concurrency/lifecycle, security, compatibility, tests/regression, CI, docs/assets, and prior findings. Triggered risk surfaces: GPU texture lifetime, asynchronous display identity, fake/pass-through comparison risk, source-vs-processed labels, release headless GPU test skips, capture HMAC and provenance, spring-training policy reconciliation, binary evidence authenticity. **Full semantic 69-file/all-hunk audit was not completed; not reported as checked.**

Mandatory failure scenario families: happy path partially checked through code and repair logs; invalid/boundary and downstream native Qt behavior partially checked; failure injection and interrupted async Vulkan render unverified; security denied HMAC negative contracts not rerun independently; old state/migration largely N/A (no persisted database/schema migration), but previous FSR settings/UI interoperability remains unverified.

## Findings
### PR8-F0001 — MAJOR, FIXED_UNVERIFIED
[Original DoD r1](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6157506067) · [exact-head finding update](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6181293524)

Previous source-level decoder-before-GPU-publish ordering flaw is corrected in inspected code by per-slot `LivePublicationTracker` with commit-on-success; EASU also publishes stage with its image. Native opt-in `ObservedSurface` on Radeon GPU reports reconstructed status and distinct source/EASU output. Remaining AC-01/02/03 are **unverified as full criteria**, because source-level fixes and component-level GPU tests do not independently exercise full QML comparison labels, Settings/HUD at 760/1280, delayed/reordered neural FSR, failed GPU scaler/upload/dispatch and transitions between reconstructed/bypassed frames. **Do not modify frozen DoD r1 or its SHA-256** `279c8849c4feb544c62ff70d8f3d9912f85807209938c7cdf28fe37ff7187d3c`.

### PR8-F0002 — MAJOR, FIXED_UNVERIFIED
[Original DoD r1](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6180883157) · [exact-head finding update](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6181293950)

EASU-only raw accessor now accepts a successfully published spatial pair (no neural harness dependency). Production `VideoSurfaceItem` now drops stale textures and fails closed on missing raw image instead of showing processed output. EASU upload withdraws publication until dispatch succeeds. Repair-supplied *real AMD* tests use decoder + Vulkan native scene graph, distinct raw/EASU pixels, both A/B swap orientations, missing-input/cache invalidation, mode switching, close and recovery. Images visually reviewed: raw and EASU are perceptibly different samples of the same testbars fixture. Remaining AC-01/02/03 are **unverified as full criteria** because this is not full `Main.qml` UI nor injected GPU/neural failure testing; CTest on headless CI may skip via return 77. No new independent finding ID was justified from the targeted evidence.

## Definition of Done
Original PR SCOPE-01 causal live stage: **UNVERIFIED** full GPU/neural integration.
SCOPE-02 authenticated same-frame causal/live + FSR4 capture: **UNVERIFIED**.
SCOPE-03 truthful accessible UI and native comparison: **UNVERIFIED** complete full-player/GPU failure paths.
SCOPE-04 spring-training research governance/frozen baseline: **UNVERIFIED** full cross-policy contradiction check.
SCOPE-05 both Python suites, Release negative control, combined build: **UNVERIFIED** final exact-head Release job still pending at reviewer check; source wiring inspected.
SCOPE-06 independently authentic AMD GPU/Qt/browser evidence: **UNVERIFIED** (repair-supplied EASU GPU proof is useful but narrower).
PR8-F0001 original AC-01, AC-02, AC-03: **UNVERIFIED**, see original URL.
PR8-F0002 original AC-01, AC-02, AC-03: **UNVERIFIED**, see original URL.
No waiver, amendment, scope change, or merge permission.

## Tests and CI
[Exact-head CI run 38658016460](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/actions/runs/38658016460): **Python contract suite SUCCESS, 144 passed**, Arch Release build+CTest **IN PROGRESS** at review. This status is timestamped, not future-proof; refresh it for final merge decision. `tests/native_compare_gpu_tests.cpp` returns **77** if no X11/Wayland display or Vulkan device, so a headless successful CI run does NOT prove native GPU execution.

Actual committed repair evidence under [2026-10-14 repair packet](../2026-10-14_repair-2ab4c738/repair.md):
- `native-off.log` / `native-optin.log`: AMD Radeon RX 7900 GRE (RADV NAVI31), real Vulkan/QSG image selection, output 64x48→128x96, zero reported failures. Opt-in reports `ANVIL causal live stage: frames=1` and subsequent reconstructed frames.
- `native-ctest.log`: both target GPU cases passed locally; `pixels-off/native-raw.png` vs `pixels-off/native-easu.png` differ.
- `negative-renderer.log`: restoring old renderer caused core dump during switching; this is earlier negative-control evidence, **not** a claim of a current build crash.
- Vulkan Khronos validation layer and final full native player interaction proof not shown.

## Three-loop receipt
**0/3 full clean discovery passes certified** at this exact head. Targeted lenses: frozen findings/code reconciliation, real native GPU/QSG artifact inspection, exact-head CI/remaining gap review. These are **not** exhaustive full-coverage clean loops.

## Posted PR comments
- [PR-level COMMENT review](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#pullrequestreview-5526342577)
- [F0001 updated re-review](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6181293524)
- [F0002 updated re-review](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6181293950)

## Repair handoff / next step
Stable machine-readable handoff: [repair-handoff.json](repair-handoff.json). **NOT A COMPLETE CANONICAL REVIEW LEDGER:** The prior repository packet lacks a complete `review.json`, so this concise handoff explicitly declares snapshot continuity unresolved instead of manufacturing a valid canonical v1.3 fingerprint.

Keep F0001/F0002 original ACs unchanged. Independently verify actual **full** Qt `Main.qml` + Settings + HUD and true GPU frame identity during neural FSR delayed/failed publication, EASU↔neural rapid switch, texture/screenshot continuity, both A/B orientations, 760/1280 layouts, and real authenticated same-frame FSR4 pair in browser. Recheck CI completion and all 69 paths, then complete three distinct clean discovery passes. Never merge without maintainer's explicit authorization.
