[PR-REPAIR][PR8-F0001][PATCHED_UNVERIFIED][DoD r1][RESULT patch:sha256:d4cf17b68db35ce0307644b826a09c06e432f2d0623a1ac8a5840749b728033c]

**Reviewer defect / source:** https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6157506067 — original body SHA-256 2192d67f77d70d757a9677c38680098164f3cbf4eb64e8738382153632e553c7. Current canonical full review.json unavailable; formal DoD continuity unverified.

**Changed behavior / repair:** Paired EASU raw/output publication, fail-closed raw cache/node selection, and decoded CPU-stage reset independent of neural history. Real Qt/Vulkan regression exercises both orientations, actual distinct raw/spatial rendered pixels, unavailable publication and recovery.

**Definition of Done — original published text, unchanged:**

- [ ] **AC-01:** When opt-in ANVIL processing is active, each native A/B comparison label must identify the actual pixel-generation stage; the label SOURCE is valid only if the pane genuinely displays original, unmodified decoded pixels. **Falsifier:** an ANVIL-modified `rawPresentationImage` is displayed under a bare SOURCE label. **Proof:** trace image-producing/upload/rendering paths and independently inspect native UI during opt-in playback of an actually reconstructed frame.
- [ ] **AC-02:** Native settings and status text must accurately state when the causal ANVIL live stage is available/active, while distinguishing the incomplete full ANVIL roster. **Falsifier:** UI claims no ANVIL reconstruction is connected while `ANVIL_LIVE_PRE_FSR=1` has engaged a real stage. **Proof:** inspect user-visible wording and exercise both default-off and opt-in playback states.
- [ ] **AC-03:** A regression must ensure that the native compare controls cannot silently conflate the original source and post-ANVIL control when the opt-in stage is enabled. **Falsifier:** tests cover only the separate browser reviewer, not the native comparison and status state. **Proof:** execute a focused native QML/UI integration or equivalent behavioral test with distinct unprocessed and reconstructed pixels.

**Local proof status:** Complete original ACs remain unverified; GPU EASU subset demonstrated, full neural failure/manual acceptance outstanding.

**Checks and regressions:** Release build; CTest 37 executed/zero failures/one skipped/four disabled; Python 144 tests plus 39 subtests; both actual AMD Qt/Vulkan pixel tests passed. Evidence at docs/review-handoffs/pr-8/2026-10-14_repair-2ab4c738/. Exact repair-head hosted CI not available for an uncommitted local patch.

**Contract stability / scope:** No frozen AC text changed or evaluator status authored. STRUCTURED SOURCE MISSING. RISK-001 proposes evaluator triage of CPU/GPU reset coupling; no new reviewer ID or criterion fabricated.

**Remaining uncertainty / external gates:** Current source/handoff reconciliation; full native/neural/manual proof; authentic FSR captures/browser verification; complete original PR scope; three full clean passes; exact-result hosted CI.

**Review request:** Independent re-review on the exact published repair head; no approval or merge requested. Draft only.
