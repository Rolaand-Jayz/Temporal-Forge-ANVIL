# Repository-based PR review handoffs

**Maintainer workflow direction (2026-10-14):** Store all future ANVIL PR evaluation/repair handoffs **in this repository** as well as publishing findings and a PR-level review on GitHub. Do not make chat-only downloadable archives the sole handoff.

## Repository layout

- `docs/review-handoffs/pr-<number>/LATEST.md`: stable entry point and the exact reviewed head, with a warning if the PR has since advanced.
- `docs/review-handoffs/pr-<number>/<YYYY-MM-DD>_<reviewed-head-short>/review.md`: immutable human-readable review, scoped DoD, proof/CI and unresolved gates.
- `docs/review-handoffs/pr-<number>/<YYYY-MM-DD>_<reviewed-head-short>/repair-handoff.json`: immutable machine-readable handoff using the current PR Evaluation ↔ Repair schema.

Commit these documentation-only artifacts **to the PR's working branch**, not directly to `main`, under the maintainer's authorization. Do not modify code, make unrelated edits, approve, resolve findings, or merge during handoff publication. Verify the target branch and publish URLs and GitHub comments. If a branch moved while publishing, do not overwrite it: refresh and reconcile first.

## Identity and continuity rules

1. A handoff is evidence tied to the **reviewed base/head SHAs**, not a timeless diagnosis. A documentation-only commit changes the branch SHA; clearly distinguish the reviewed code head, the handoff publication commit, and any subsequent repair head. Never silently carry forward PASS/green CI to an unseen head.
2. Preserve original PR-scoped DoD, findings and ACs, frozen revisions, checksum, prior comments and status transitions. A repair agent's `PATCHED_PROVISIONAL` is not an independent reviewer PASS.
3. Never overwrite earlier review snapshots. On re-review add a fresh dated/commit-scoped snapshot and update only the stable index. Keep GitHub finding comments and PR-level COMMENT reviews as the independently visible discussion record.
4. A machine handoff describes a **specific reviewer snapshot**. The full canonical `review.json` may remain in the evaluator's external archive if it cannot be committed; do **not** pretend the short handoff replaces the immutable full ledger. Link known reviewer history and source review comments.
5. Require independent exact-head testing and full mandatory discovery loops before a PASS/NEEDS REVISION verdict. No automatic merge; maintainer approval is separate.

**Current example:** [PR #8 handoff index](pr-8/LATEST.md).
