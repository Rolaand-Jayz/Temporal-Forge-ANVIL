# PR #8 repair — pre-publication evidence snapshot

## Source contract

**PARTIAL / UNVERIFIED — STRUCTURED SOURCE MISSING.** Parent: `2ab4c73847a64fd72e93f3ab1ac4ae3006097636`.
Snapshot result: `patch:sha256:d4cf17b68db35ce0307644b826a09c06e432f2d0623a1ac8a5840749b728033c` (uncommitted). [Patch](repair.patch) and [source identity](source-manifest.json).
The remote handoff at `../2026-10-14_205fe7e/` is historical, contains only PR8-F0001 and no complete review.json. Current published evaluator comments add PR8-F0002 at 1882627c. Neither the historical archive nor this repair manufactures a new evaluator ledger.

`comment-triage.json` is the schema-validated raw-comment fallback. `repair.json` is the same fallback document, **not a formal v1.3 repair ledger**. `evaluator-handoff.json` explicitly states the missing source and requests reconciliation. Review snapshot hashes/finding fingerprints are not invented. No approval, merge or reviewer status transition claimed.

## Original PR DoD

SCOPE-01 through SCOPE-06 remain original reviewer obligations. The repair exercises real EASU integration, preserves default-off, demonstrates reachable opt-in reconstruction, and tests native raw/output ownership and recovery. It does not certify authenticated FSR paired captures, full manual player/browser acceptance, the complete policy audit or all neural failure paths. SCOPE-05 local build/test wiring remains present; required hosted checks must run on a resulting pushed head. No scope or frozen criterion is amended.

## Findings/AC matrix

PR8-F0001 remains reviewer-owned FIXED_UNVERIFIED in the latest comment. This patch removes GPU-only reset coupling from the CPU stage and supplies real EASU native stage/texture behavior evidence. Full AC-01/02/03 require broader native/neural/manual proof and remain **unverified as complete criteria**.

PR8-F0002 is implemented locally: raw/control and enhanced spatial images use the same successfully published uploader; raw access no longer requires neural harness readiness; beginning upload withdraws the pair and publication requires successful allocation, conversion, dispatch and submission. Raw-only/split rendering clears missing cached textures and returns no video rather than selecting enhanced output. Scene nodes retire before their texture wrappers, including single/split mode transitions. Full AC-01/02/03 remain **unverified as complete criteria** pending canonical reconciliation and manual/neural/failure proof.

All original comment AC text (expected behavior, falsifier and proof) is preserved verbatim below and in comment-triage.json; local evidence does not overwrite reviewer acceptance.

### PR8-F0001 — original published criteria

Source: https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6157506067

- [ ] **AC-01:** When opt-in ANVIL processing is active, each native A/B comparison label must identify the actual pixel-generation stage; the label SOURCE is valid only if the pane genuinely displays original, unmodified decoded pixels. **Falsifier:** an ANVIL-modified `rawPresentationImage` is displayed under a bare SOURCE label. **Proof:** trace image-producing/upload/rendering paths and independently inspect native UI during opt-in playback of an actually reconstructed frame.
- [ ] **AC-02:** Native settings and status text must accurately state when the causal ANVIL live stage is available/active, while distinguishing the incomplete full ANVIL roster. **Falsifier:** UI claims no ANVIL reconstruction is connected while `ANVIL_LIVE_PRE_FSR=1` has engaged a real stage. **Proof:** inspect user-visible wording and exercise both default-off and opt-in playback states.
- [ ] **AC-03:** A regression must ensure that the native compare controls cannot silently conflate the original source and post-ANVIL control when the opt-in stage is enabled. **Falsifier:** tests cover only the separate browser reviewer, not the native comparison and status state. **Proof:** execute a focused native QML/UI integration or equivalent behavioral test with distinct unprocessed and reconstructed pixels.

### PR8-F0002 — original published criteria

Source: https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6180883157

- [ ] **AC-01:** When compare is available in EASU-only mode, the pane labeled SOURCE displays decoder-owned pixels of the corresponding published frame, or a truthful post-ANVIL control if reconstruction modified those pixels. **Falsifier:** an EASU-upscaled output or a previous frame appears under SOURCE. **Proof:** trace EASU image and raw-source publication/accessor/Qt consumer; run a native EASU-only A/B session with intentionally distinct source/output frames.
- [ ] **AC-02:** Raw-only rendering must not silently substitute processed output or stale cached input whenever a valid paired raw texture is unavailable. **Falsifier:** `rawOnly: true` renders native output or an unrelated prior raw frame after raw accessor failure. **Proof:** exercise missing/failed raw image, switching FSR4/EASU, and cache invalidation/recovery.
- [ ] **AC-03:** Native automated/behavioral regression covers EASU-only comparison authenticity, both swap orientations, and default-off plus opt-in stage labeling without replacing runtime proof with QML string assertions. **Falsifier:** tests pass while both compare panes use processed EASU output. **Proof:** targeted source-to-present behavior verification using distinct raw/EASU pixels plus manual Qt/Vulkan inspection.

## Changed files/impact

- `src/core/PlaybackEngine.cpp`: EASU published owner, readiness checks, upload/submit failure gate, and CPU decoded reset independent of GPU history.
- `src/ui/VideoSurfaceItem.cpp`: fail-closed comparison, cache invalidation, safe node/wrapper replacement across mode transitions.
- `tests/native_compare_gpu_tests.cpp`: actual decoder/uploader/Qt render-thread image selection and rendered pixel readback; swap, invalidation, close and recovery.
- `tests/CMakeLists.txt`: small generated H.264 fixture and two native GPU CTests; honest return-77 skip without a display/device.
- `tests/test_player_ui_contract.py`: existing publication-location guard follows the decodedReset call.
- `docs/current/PLAYER_UI.md`: implemented EASU comparison semantics and test/manual boundary.

RISK-001: EASU did not run neural dispatch, so FSR continuity and restored GPU reset requests repeatedly primed the CPU ANVIL window. Separating decoded reset fixes this in the real opt-in test. Evaluator triage is still required; no new evaluator finding ID was assigned.

## Tests/CI and exact target

- Release build succeeded.
- Broad CTest: 41 registered, 37 executed, zero failures, one skip (tensor assets), four intentionally disabled diagnostics; provenance rebuild regression passed. See ctest.log. The subsequently strengthened screenshot assertions passed both fresh native CTests in native-ctest.log.
- Python CI-equivalent suite: 144 passed, 39 subtests passed (python.log). An initial stale build-py runner produced two provenance failures; using the freshly built CMake runner resolved them. No assertions were relaxed.
- Actual AMD Radeon RX 7900 GRE / RADV Qt Vulkan execution: default-off and opt-in pass; actual source/control pixels differ from spatial output for the frozen frame, both swap orientations choose corresponding VkImages, unavailable publication clears the node/cache, seek/retry recovers. See native-off.log/native-optin.log and pixels-off/pixels-on PNGs.
- Historical negative controls: restoring neural-only raw readiness produces one failed paired-source assertion (exit 1); restoring the original renderer produces SIGSEGV (exit -11) during mode transitions. Fixed code restored and rebuilt; these supporting negative runs precede the final screenshot assertion extension.
- No Khronos validation layer is installed. Do not claim validation-clean execution from an ordinary GPU test.
- Existing remote CI belongs to the parent/reviewer heads, **not** this local patch. No fresh repair-head hosted CI is claimed.

Qt documents that simple texture nodes do not own their textures by default; the implementation retires nodes before deleting the item-owned wrappers: https://doc.qt.io/qt-6/qsgsimpletexturenode.html .

## Three verification passes

Three targeted lenses are recorded in verification.json: dataflow/interface ownership; real GPU pixel authenticity and invalidation/recovery; Release/regression/provenance. **0/3 complete contractual clean passes certified.** These targeted checks cannot substitute for missing canonical source, complete original PR scope and native neural/manual evidence. Each complete AC is conservatively unverified; no READY_FOR_REVIEW claim.

## Remaining risks/blockers

- STRUCTURED SOURCE MISSING: remote archived handoff is historical at 205fe7e, includes only PR8-F0001, and no full review.json; latest PR8-F0002 exists in published comments.
- Full independent native Qt UI/manual acceptance and neural-mode pending/failed GPU publication tests remain unverified.
- Authenticated AMD same-frame capture/browser acceptance and complete original PR-scoped DoD remain unverified.
- Vulkan validation-layer proof unavailable on this host (no installed Khronos validation layer).
- Three complete clean contractual verification passes are not certified; targeted passes are not substituted.
- No resulting commit/push or exact-result hosted CI; repair is an uncommitted local patch.

## Evaluator handoff / actions and GitHub reply links

See evaluator-handoff.json, verification.json, comment-triage.json and the two reply drafts. Reconcile the current source snapshot, review each unchanged AC on the resulting published repair head, and decide reviewer statuses independently. Replies are drafts; no GitHub mutation, thread resolution, approval or merge occurred. Commit/push/reply remain separate authorization steps.

Publication note (2026-10-14): the maintainer subsequently authorized commit and push of this repair to PR #8. This immutable proof packet retains its pre-publication patch reference and limitations. It does not claim that posting repair replies was authorized.
