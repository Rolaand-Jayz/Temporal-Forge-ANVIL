[PR-REPAIR][PR8-F0002][PATCHED_UNVERIFIED][DoD r1][RESULT patch:sha256:d4cf17b68db35ce0307644b826a09c06e432f2d0623a1ac8a5840749b728033c]

**Reviewer defect / source:** https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6180883157 — original body SHA-256 ffd437463579e50e39f773c7b73324ed7177f1511e7c855264b835d0428a9258. Current canonical full review.json unavailable; formal DoD continuity unverified.

**Changed behavior / repair:** Paired EASU raw/output publication, fail-closed raw cache/node selection, and decoded CPU-stage reset independent of neural history. Real Qt/Vulkan regression exercises both orientations, actual distinct raw/spatial rendered pixels, unavailable publication and recovery.

**Definition of Done — original published text, unchanged:**

- [ ] **AC-01:** When compare is available in EASU-only mode, the pane labeled SOURCE displays decoder-owned pixels of the corresponding published frame, or a truthful post-ANVIL control if reconstruction modified those pixels. **Falsifier:** an EASU-upscaled output or a previous frame appears under SOURCE. **Proof:** trace EASU image and raw-source publication/accessor/Qt consumer; run a native EASU-only A/B session with intentionally distinct source/output frames.
- [ ] **AC-02:** Raw-only rendering must not silently substitute processed output or stale cached input whenever a valid paired raw texture is unavailable. **Falsifier:** `rawOnly: true` renders native output or an unrelated prior raw frame after raw accessor failure. **Proof:** exercise missing/failed raw image, switching FSR4/EASU, and cache invalidation/recovery.
- [ ] **AC-03:** Native automated/behavioral regression covers EASU-only comparison authenticity, both swap orientations, and default-off plus opt-in stage labeling without replacing runtime proof with QML string assertions. **Falsifier:** tests pass while both compare panes use processed EASU output. **Proof:** targeted source-to-present behavior verification using distinct raw/EASU pixels plus manual Qt/Vulkan inspection.

**Local proof status:** Complete original ACs remain unverified; GPU EASU subset demonstrated, full neural failure/manual acceptance outstanding.

**Checks and regressions:** Release build; CTest 37 executed/zero failures/one skipped/four disabled; Python 144 tests plus 39 subtests; both actual AMD Qt/Vulkan pixel tests passed. Evidence at docs/review-handoffs/pr-8/2026-10-14_repair-2ab4c738/. Exact repair-head hosted CI not available for an uncommitted local patch.

**Contract stability / scope:** No frozen AC text changed or evaluator status authored. STRUCTURED SOURCE MISSING. RISK-001 proposes evaluator triage of CPU/GPU reset coupling; no new reviewer ID or criterion fabricated.

**Remaining uncertainty / external gates:** Current source/handoff reconciliation; full native/neural/manual proof; authentic FSR captures/browser verification; complete original PR scope; three full clean passes; exact-result hosted CI.

**Review request:** Independent re-review on the exact published repair head; no approval or merge requested. Draft only.
