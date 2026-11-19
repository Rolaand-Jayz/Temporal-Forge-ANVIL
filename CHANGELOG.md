# Changelog

Notable changes to the project record. Dates are UTC. Detail lives in `docs/current/STATE.md`, `docs/decisions/TECHNICAL_HISTORY.md`, and the closure set under `docs/closure/`.

## 2026-11-19

- **Project hygiene:** added a code of conduct, issue and pull request templates, a changelog, and citation metadata.

## 2026-10-21

- **Storage:** pruned 1,740 unreferenced legacy LFS review images (PR #9). The 64 catalog-referenced images are kept.

## 2026-10-15

- **ANVIL:** merged PR #8. It adds opt-in causal live reconstruction before FSR playback, paired same-frame baseline and FSR 4.1 captures with an image-first reviewer, a player UI overhaul, and the Spring Training research policy documents. It consolidates the closed PRs #3, #5, #6, and #7.

## 2026-10-14

- **Review:** PR review and repair handoffs are now stored in the repository under `docs/review-handoffs/`.

## 2026-10-09

- **ANVIL:** merged PR #1 (successor build-ready implementation) into `main` at `36952dce`. The independent evaluator's final re-review at `fe7dac22` was clean, and `BUILD_READY_FOR_RESEARCH = TRUE` was adjudicated.

## 2026-10-05

- **ANVIL:** the maintainer authorized the Temporal Forge / ANVIL successor program. The execution pack was ingested under `zcode_packs/`.

## 2026-09-15

- **Closure:** closed the FSR-centered research line. Added the closure set: final adjudication, evaluation standard, claim/evidence ledger, and limitations and open questions.

## 2026-07-23 to 2026-09-12 (FSR era)

History before the closure is shared with Temporal-Forge-Player.

- 2026-09-12: merged the clean-clone portability remediation and the final Expected-Food checkpoint (`285a5788`).
- 2026-09-09: Apache-2.0 for original work, with third-party and reverse-engineering provenance recorded. Hosted CI added.
- 2026-09-06: Quality Lab review adjudication recorded and the quality-lab stack merged.
- 2026-09-03: lattice P0 reopened; fix for semantic lattice corruption at FP16 resolve committed.
- 2026-09-01: M6 recapture report.
- 2026-08-31: supersampling report.
- 2026-07-23: initial commit.
