# ANVIL PR #8 — Re-review (2026-10-14)

## Classification
**INCOMPLETE**. Existing **major** finding **PR8-F0001 remains OPEN** at the reviewed head: new stage tracking fixes static texts, but stage status is published before GPU image success, producing possible stale SOURCE/POST-ANVIL identification. Full mandatory three clean passes and AMD/Qt checks not performed.

## PR / exact head
https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8  
Base: `f2f8b9929c1cc3845dab90bbe80d889b44c99390`  
**Reviewed head:** `205fe7e077da4332a175b77a07219df4cc4c3986`

**Historical snapshot:** A later repair commit `1882627c3a7e0fe9a8ce3782210db135084afef9` was already on PR #8 when this handoff was committed to the repository. That repair has **not** been independently re-reviewed here. Do not treat this handoff as a judgment of the later head; re-review its exact SHA.

## Scope and coverage
41 changed paths and 41 non-null patches inventoried (3,320 additions, 337 deletions) at the reviewed head. Focused QML/C++/CI/tests and native VideoSurface consumers inspected, not exhaustive 41-path semantic proof. Thirteen mandated dimensions represented in reviewer ledger.

## Findings
**PR8-F0001 (major, OPEN at reviewed head, original DoD r1 unchanged):** `PlaybackEngine.cpp:2949-2950` publishes the latest *processed* frame status, while `fsr4RawOutput` supplies the last *successfully published* GPU uploader. Between these boundaries (or after a failed next-frame dispatch), native comparison can label the previous post-ANVIL image SOURCE. This was a source-level falsifier, not an independently observed native screen recording. [Finding re-review](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6171500175) · [Original frozen DoD](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6157506067).

## Definition of Done (frozen r1)
- **AC-01 FAILED:** Native A/B label must track actual displayed pixel-generation stage, not only the latest processed frame; label SOURCE only when actually displayed pixels are original.
- **AC-02 UNVERIFIED:** Settings and HUD text corrected in code, but displayed-frame alignment and real default/opt-in sessions require verification.
- **AC-03 FAILED/UNVERIFIED:** Tests asserted tracker + pixel differences and QML strings, not the delayed/failed Vulkan publication boundary. Add integration regression falsifier.
- **SCOPE-01, 02, 04, 06 UNVERIFIED; SCOPE-03 FAILED; SCOPE-05 SATISFIED** for tested CI gate.
- Original DoD r1 SHA-256: `279c8849c4feb544c62ff70d8f3d9912f85807209938c7cdf28fe37ff7187d3c`. Never edit the frozen ACs without authorized, versioned amendment.

## Tests and CI
[Exact-head run 38584217028](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/actions/runs/38584217028): Release build + CTest 35 run, 0 failed, 2 skipped and 4 disabled. Python contracts 143 passed. No independent proof of native Qt GPU interaction or authenticated real-image capture.

## Three-loop receipt
**0/3 certified.** Focused review found a retained publication-order mismatch; exhaustive discovery across all paths/scenarios not certified.

## Posted PR comments
[PR-level COMMENT review](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#pullrequestreview-5520118585) · [Finding update](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6171500175) · [Original finding](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6157506067).

## Repair handoff / next step
Frozen original DoD r1 remains authoritative. Repair frame-to-*published-image* association; test reconstructed→bypassed, pending/failed GPU publication, and both source-to-reconstructed and reverse transitions. Verify labels on the exact published frame, then request independent re-review of the later head. Do not merge without explicit owner authorization.

Machine handoff: [repair-handoff.json](repair-handoff.json). This snapshot records *the reviewer state at 205fe7e*, not automatic current status.
