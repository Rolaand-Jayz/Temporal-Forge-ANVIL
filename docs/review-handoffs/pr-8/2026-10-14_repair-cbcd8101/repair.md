# PR #8 repair — PR8-F0003 settings warning readability and false-viewport evidence

Repair-side **PATCHED_PROVISIONAL** handoff for the finding opened by the
re-review at `cbcd8101a4953421af512f87503dd176768795a9`
(https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6182430653).
Not an independent verdict; re-review and merge authorization remain with the
evaluator and owner.

## Source contract

- Parent / last independently reviewed code head: `cbcd8101a4953421af512f87503dd176768795a9` (remote confirmed before repair).
- Tested result: patch SHA-256 `ef6ab9a41eafd54a3dc98c3b12401303d01d9367bec9847492034592f40f9e14` on that head (`repair.patch`, `source-manifest.json`); the commit that adds this packet is the resulting published revision.
- Latest structured evaluator handoff: `../2026-10-14_18eb5e3/repair-handoff.json` (canonical ledger unavailable; comments are the frozen authority). F0003 was issued after that snapshot as a GitHub comment with its own frozen DoD r1; its text is copied verbatim below and is not reworded.
- F0001 DoD r1 fingerprint (unchanged, from evaluator): `279c8849c4feb544c62ff70d8f3d9912f85807209938c7cdf28fe37ff7187d3c`. F0002 fingerprint is not invented (evaluator null).

## Original PR DoD

Scope statements are unchanged from the evaluator's PR-level summary
(SCOPE-01…06). This repair touches only SCOPE-03 surfaces (Settings dialog
layout and the responsive-layout test evidence) and the SCOPE-05 test suite
contents. No scope amendment is requested.

## Findings/AC matrix

### PR8-F0003 (MODERATE, OPEN at source) — repaired locally, provisional

Original defect (verbatim from the finding): "Full-player evidence purporting
to exercise 760px and 1280px layouts does not demonstrate distinct window
sizes: `ui-presentation-complete/main-760-swapped.png` and
`main-1280-swapped.png` are **byte-identical** (Git blob SHA `18b7efbe…`).
The saved `settings-760.png` and `settings-1280.png` also visibly clip the
start of the orange warning text. … at least one user-visible
experimental-backend warning is unreadable, and the test cannot establish
responsive behavior for the two requested sizes."

Definition of Done — unchanged (r1), evidence per AC:

- **AC-01 — "Experimental backend warning title and explanation remain fully readable within the Settings content viewport at supported minimum and standard window sizes; Falsifier: initial characters or the explanatory lead-in are cut off as in saved screenshots; Proof: inspect corrected actual Qt window captures and bounds at 760px and 1280px."** — satisfied_locally (provisional). Root cause: the warning banner `Rectangle` was a `ColumnLayout` child sized by a plain `width: parent.width` binding, which the layout does not honor — the banner rendered effectively zero-width/misplaced and its centered text overflowed left across the sidebar divider, so each line's lead-in ("FSR 4.1 IN…", "RE-derived. Local valida…") was cut. Repair: `Layout.fillWidth` + adaptive `Layout.preferredHeight`, both text lines capped at the banner content width with word-wrap fallback and `objectName`s for bounds checking. Evidence: `native-off.log` OK lines "warning banner is laid out and visible", "warning text bounds stay inside the settings content viewport", "warning title and explanation fit untruncated on their lines at this width" at realized settings widths 760 and 1280; `capture-verification.json` pixel audit (warn pixels start at x=205, right of the 181px sidebar divider, in both captures); vision inspection of `settings-760-banner` reads both full lines with nothing cut off.
- **AC-02 — "Responsive-layout tests must assert actual realized player window/content dimensions before claiming the 760px and 1280px cases and inspect distinct layout states; Falsifier: identical captures can pass as two different widths without verifying `window->width()`/geometry; Proof: focused negative/control test or explicit geometry assert plus captured effective dimensions."** — satisfied_locally (provisional). Second root cause (why the prior captures were byte-identical): on this KDE Wayland session a client resize issued while the window is still inactive — exactly the state right after the modal Settings dialog hides — is **silently dropped by the platform**; the prior 150 ms event pump then snapshotted the stale 760×480 layout twice. `resize-rootcause-probe/` reproduces this in a two-window probe (resize-while-inactive stays 760×480; activate-then-resize delivers 1280×720). Repair: `resizeConfirmed()` activates the window, re-issues the request across transient compositor states, and the test now **fails loudly** ("compositor realizes the requested player window geometry") when the delivered geometry never matches, prints the realized logical size and DPR for each case, asserts the two player captures are distinct layouts, and the Settings window itself is exercised at 760 (its minimumWidth) and 1280 with confirmed geometry. Negative control: the intermediate candidate without the activation step failed exactly this new assertion (`FAIL: compositor realizes the requested player window geometry`, then a byte-duplicated capture check) — observed during development, not from the final binary; recorded here so the evaluator can see the assertion catches the original silent defect.
- **AC-03 — "The production Main.qml and Settings layout must pass a behavioral/visual acceptance run without horizontal clipping, hidden essential controls or unreadable labels at both target sizes; Falsifier: a warning/essential control is clipped despite the automated test passing; Proof: independent Qt/Vulkan run with visual inspection and accurate per-size screenshots."** — satisfied_locally (provisional). Real Qt/Vulkan runs on AMD Radeon RX 7900 GRE (RADV NAVI31) at realized 760×480 / 1280×720 (player) and 760×560 / 1280×680 (Settings): production `Main.qml` with real controllers; compare labels, HUD, Settings stage text, pane swapping, preset shortcuts all pass in off, opt-in (`ANVIL_LIVE_PRE_FSR=1`), budget (ctest), neural and presentation-rejection variants. Accurate per-size screenshots are in `ui-presentation-complete/` (760 and 1280 are now different pixel sizes and contents). Vision inspection of both player captures found both pane labels readable and the control shelf unclipped; of the settings banner found both warning lines fully readable. Full manual human acceptance (every control at both sizes) remains evaluator work.

### PR8-F0001 / PR8-F0002 (MAJOR, FIXED_UNVERIFIED at source) — unchanged

No code path of either finding is touched by this repair (see
`source-manifest.json`: only `SettingsDialog.qml`, `native_player_ui_tests.cpp`,
`PLAYER_UI.md`). All four native variants and the full CTest suite — including
the live-frame negative control, provenance rebuild regression, and both
component GPU suites — pass at this repair state, so no regression is
introduced against their existing evidence. Their immutable DoD r1 ACs remain
unverified pending independent exact-head execution, exactly as the reviewer
recorded. No status change is claimed here.

## Changed files/impact

- `resources/qml/SettingsDialog.qml`: warning banner converted to layout-managed sizing (`Layout.fillWidth`, adaptive `Layout.preferredHeight` collapsing when hidden), texts width-capped with word-wrap fallback, `objectName`s added for bounds assertions. No other pane changed.
- `tests/native_player_ui_tests.cpp`: `resizeConfirmed()` helper (activate → re-issue → assert realized geometry, 5 s budget); realized-size logging; distinct-capture assertion for the two player widths; Settings dialog resized to its own supported 760/1280 with confirmed geometry; warning banner visibility/bounds/untruncated checks; retained preset-key focus stabilization (each Ctrl+1..5 waits for the real main window to be active — required now that the Settings dialog is shown/hidden inside both width iterations).
- `docs/current/PLAYER_UI.md`: verification paragraph updated to describe realized-geometry assertion and warning-bounds verification (no acceptance criteria weakened).

## Tests/CI and exact target

Executed on real hardware (AMD Radeon RX 7900 GRE, RADV NAVI31, KDE Wayland,
Release build): `native-off.log`, `native-optin.log`, `neural.log`,
`presentation.log` — **0 failures each** (neural runs provisioned with
`TFORGE_FSR4_RE_ROOT` for the quality initializer, as in prior evidence).
`ctest.log`: **100% of 42 passed** (1 asset-dependent skip, 4 disabled — same
disabled set as the prior packet). `python.log`: the nine CI-selected suites —
**144 passed, 39 subtests passed**, matching the hosted-CI baseline. Full
local pytest beyond the CI selection shows 3 failures in
`test_m6_quality_class_annotations` / `test_review_harness_contract` that are
**pre-existing at `cbcd8101`** (verified identical via a `git stash` control
run) and outside this repair's surfaces. Exact-head hosted CI must be
confirmed on the pushed commit; nothing here may be attributed to a different
tree.

## Three verification passes

`verification.json` records three consecutive clean passes on the tested
patch state with distinct lenses: (1) runtime geometry & visual acceptance,
(2) regression/contract suites, (3) source adversarial & root-cause
falsification. No new findings, regressions or blocking gaps surfaced. This
is a repair-scoped receipt; the PR-level three exhaustive clean discovery
passes remain the independent evaluator's gate (still 0/3 certified).

## Remaining risks/blockers

- Independent exact-head re-review of F0003, and of F0001/F0002 immutable
  ACs, is outstanding; nothing here is a reviewer PASS.
- PR-level exhaustive coverage (114 paths, 13 dimensions, spring-policy
  cross-check) and the evaluator's three clean discovery loops remain open.
- Wayland resize semantics are compositor-dependent; the test now fails
  loudly rather than passing silently when a compositor declines a geometry,
  and `resize-rootcause-probe/` documents the observed KDE behavior for
  reproduction.
- Full human accessibility/manual acceptance of every control at both sizes
  (beyond the automated and vision checks recorded here) is still evaluator
  work.

## Evaluator handoff / next step

Independent `pr-evaluation` re-review requested at the resulting pushed head
for PR8-F0003 (and unchanged F0001/F0002). No approval, thread resolution or
merge is requested or implied. Machine-readable: `repair.json`,
`evaluator-handoff.json`.
