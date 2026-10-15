# ANVIL PR #8 — reviewer handoffs

**Latest independent evaluation (2026-10-14):** [review.md — code head `18eb5e3`](2026-10-14_18eb5e3/review.md), [machine-readable repair handoff](2026-10-14_18eb5e3/repair-handoff.json), and [append-only completed-CI receipt](2026-10-14_18eb5e3/ci-follow-up.md).

**Current technical disposition:** **INCOMPLETE; no merge authorization.** Both `PR8-F0001` and `PR8-F0002` are `FIXED_UNVERIFIED`, each with unchanged DoD r1 AC-01/02/03. The exact reviewed code head `18eb5e3777497c77f2659a915bce6c8fcf61f6c0` passed hosted Python (144) and Release CTest (37 run/0 fail, 4 skip including both headless GPU tests, 4 disabled). The repairer's local AMD Qt/Vulkan component evidence was inspected but full native Main.qml/FSR neural error-path, authenticated paired capture and three clean exhaustive review passes remain unverified.

**Versioning caution:** This index and handoff files were committed *after* the independent review, so the live PR branch SHA now differs from the reviewed source SHA solely by these evidence/documentation commits. **Do not treat a later code change as reviewed** or attach a prior CI status to a different code tree. Compare the live PR head with the documented reviewed head and inspect any intervening commits before promotion.

## Frozen findings and GitHub record

- [PR8-F0001 original immutable DoD](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6157506067) · [latest finding review](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6181293524)
- [PR8-F0002 original immutable DoD](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6180883157) · [latest finding review](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6181293950)
- [Latest independent PR-level COMMENT review](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#pullrequestreview-5526342577) · [CI completion update](https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL/pull/8#issuecomment-6181330333)

## Earlier review and repair evidence

- [Earlier independent evaluation at `205fe7e`](2026-10-14_205fe7e/review.md) · [earlier repair handoff](2026-10-14_205fe7e/repair-handoff.json).
- [Repair-side GPU packet (parent `2ab4c738`)](2026-10-14_repair-2ab4c738/repair.md), with `native-off.log`, `native-optin.log`, raw/EASU PNGs, and source manifest. Its own status is PARTIAL/UNVERIFIED; it is not an independent evaluator PASS.

## Future review protocol

Maintain immutable snapshot directories by reviewed head with `review.md` and `repair-handoff.json`; keep complete canonical `review.json` when validated and available. Link new GitHub COMMENT reviews and append CI after-the-fact as a dated addendum, never silently change prior evidence. Update this stable LATEST index only after review publication and verify branch/SHA before writes.

Workflow: [review-handoff policy](../README.md). **No automatic merge; owner authorization is separate.**
