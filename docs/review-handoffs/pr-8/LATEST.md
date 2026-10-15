# ANVIL PR #8 — reviewer handoffs

**Latest repair (2026-10-14, F0003):** [repair packet — repair head `9605cb9f`](2026-10-14_repair-cbcd8101/repair.md), produced against the re-review at code head `cbcd8101` that opened [PR8-F0003](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6182430653). Bounded patch: Settings warning banner layout repair, realized-geometry/distinct-capture/warning-bounds test assertions, realized-width evidence captures, and a Wayland dropped-resize root-cause probe. All native variants, full CTest and the CI-selected Python contracts pass on real AMD hardware at that exact tree; machine handoff: [repair.json](2026-10-14_repair-cbcd8101/repair.json) / [evaluator-handoff.json](2026-10-14_repair-cbcd8101/evaluator-handoff.json); reply draft: [reply-draft-PR8-F0003.md](2026-10-14_repair-cbcd8101/reply-draft-PR8-F0003.md); exact-head hosted CI receipt: [ci-follow-up.md](2026-10-14_repair-cbcd8101/ci-follow-up.md) (run 38680717192 SUCCESS, native GPU tests skipped headless). PATCHED_PROVISIONAL — independent re-review required; F0001/F0002 remain FIXED_UNVERIFIED with untouched code paths.

**Latest independent evaluation (2026-10-14):** [review.md — code head `18eb5e3`](2026-10-14_18eb5e3/review.md), [machine-readable repair handoff](2026-10-14_18eb5e3/repair-handoff.json), and [append-only completed-CI receipt](2026-10-14_18eb5e3/ci-follow-up.md). A later comments-based re-review at `cbcd8101` (findings [F0001](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6182429757) / [F0002](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6182430190) FIXED_UNVERIFIED, new [F0003](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6182430653) OPEN) is the frozen authority for the current repair cycle; no canonical `review.json` exists for it in-repository.

**Current technical disposition:** **INCOMPLETE; no merge authorization.** `PR8-F0001`/`PR8-F0002` FIXED_UNVERIFIED with immutable DoD r1; `PR8-F0003` repaired provisionally at `9605cb9f` pending independent verification. PR-level exhaustive coverage and three clean discovery passes remain 0/3.

**Versioning caution:** This index and handoff files are committed *after* the independent reviews, so the live PR branch SHA differs from any reviewed/repaired source SHA by evidence/documentation commits. **Do not treat a later code change as reviewed** or attach a prior CI status to a different code tree. The repair code commit itself (`9605cb9f`) exactly equals the tested patch SHA-256 `ef6ab9a4…` over `cbcd8101`. Compare the live PR head with the documented heads and inspect any intervening commits before promotion.

## Frozen findings and GitHub record

- [PR8-F0001 original immutable DoD](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6157506067) · [latest finding review](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6182429757)
- [PR8-F0002 original immutable DoD](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6180883157) · [latest finding review](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6182430190)
- [PR8-F0003 frozen DoD (opened at `cbcd8101`)](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6182430653) · [PR-level INCOMPLETE summary](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6182430653)
- [Earlier independent evaluation at `18eb5e3`](2026-10-14_18eb5e3/review.md) · [CI completion update](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6181330333)

## Earlier review and repair evidence

- [Earlier independent evaluation at `205fe7e`](2026-10-14_205fe7e/review.md) · [earlier repair handoff](2026-10-14_205fe7e/repair-handoff.json).
- [Repair-side GPU packet (parent `2ab4c738`)](2026-10-14_repair-2ab4c738/repair.md), with `native-off.log`, `native-optin.log`, raw/EASU PNGs, and source manifest. PARTIAL/UNVERIFIED; not an independent evaluator PASS.
- [Follow-up repair packet (parent `62d2706e`, published as `cbcd8101`)](2026-10-14_repair-62d2706e/repair.md). Its responsive-width captures are the F0003 defect evidence (both width cases byte-identical 950×600; fixed-size Settings dialog with clipped warning) and are superseded by the `cbcd8101`→`9605cb9f` repair packet above.

## Future review protocol

Maintain immutable snapshot directories by reviewed head with `review.md` and `repair-handoff.json`; keep complete canonical `review.json` when validated and available. Link new GitHub COMMENT reviews and append CI after-the-fact as a dated addendum, never silently change prior evidence. Update this stable LATEST index only after review publication and verify branch/SHA before writes.

Workflow: [review-handoff policy](../README.md). **No automatic merge; owner authorization is separate.**
