# ANVIL Spring Training — research policy

**Status:** Proposed research policy for maintainer review; **not** an implemented feature, approved candidate roster, or launched quality campaign.
**Established:** 2026-10-09
**Scope:** Temporal Forge / ANVIL successor research; the closed FSR-centered research era remains closed.
**Document class:** **Research Policy — Implementation Independent**

> These documents establish research intent, terminology, decision rules, and evidence expectations. They **do not** prescribe software architecture, API contracts, manifest schemas, user-interface component structure, file formats, build targets, or implementation mechanisms. Once the separate Home Field Exhibition / Visual Review Lab implementation has been reviewed, its observed behavior must be documented accurately. Any conflict with the agreed research policy requires explicit adjudication; do **not** rewrite either the policy or the observations merely to make them appear consistent.

## Why baseball?

Spring Training is a shared mental model, not decoration. ANVIL is the **home field**: it hosts reproducible, controlled experiments. Prospective methods are invited to **Open Tryouts**, challenged in **Spring Training**, and evaluated for what they contribute both individually and as teammates. Selection builds a provisional **roster**; focused optimization comes afterward. A promising rejected candidate can enter the **minor league** for a documented future **callback**.

The analogy is particularly useful because different positions matter. A correspondence method, confidence estimator, fusion algorithm, and spatial upscaler should not all be judged as though they independently produce the same output. A less-visible supporting role must have a fair way to prove its contribution.

## Document map

| Document | Purpose |
|---|---|
| [CHARTER.md](CHARTER.md) | Research goals, scientific principles, role-based fairness, governance |
| [OPEN_TRYOUTS.md](OPEN_TRYOUTS.md) | Candidate invitations, screening, comparisons, holdout safeguards |
| [ROSTER_POLICY.md](ROSTER_POLICY.md) | Immutable ANVIL baseline, six statuses, promotions/cuts, recalls |
| [SCOUTING_REPORT_STANDARD.md](SCOUTING_REPORT_STANDARD.md) | Evidence and human-review standard; claim limits |
| [ROADMAP.md](ROADMAP.md) | Home Field Exhibition through roster formation and optimization |
| [IMPLEMENTATION_RECONCILIATION.md](IMPLEMENTATION_RECONCILIATION.md) | How to connect policy to verified implementation **after** ZCode's work |

## Operating boundaries

1. **The original ANVIL baseline does not move.** It is the frozen original reconstruction implementation/configuration; it is **not** the raw decoded-frame control or the evolving current main branch. Its reproducible technical identity is to be established by the separate implementation work, not invented in these documents.
2. **Every experimental label must describe the actual pipeline.** The baseline prefix identifies lineage; detailed descriptions must show additions, replacements, disabled stages, temporal reconstruction, and any true spatial upscaling.
3. **The roster evolves; evidence does not.** Current dispositions can change, but past experiments and decisions remain available.
4. **A candidate earns a fair opportunity in its role.** Equal opportunity is not the same as identical tests for fundamentally different algorithms.
5. **No one wins by default.** FSR 4.1, a neural model, a classical estimator, and an internal ANVIL stage all require technically valid tests and evidence. A technical-integration blocker is not a measured image-quality defeat.
6. **No single score crowns the team.** Spatial fidelity, temporal integrity, computational cost, and compatibility are distinct considerations.
7. **Unsuccessful research remains useful.** Preserve negative findings, ambiguous outcomes, invalidated experiments, and conditional wins.
8. **No automatic promotions or merges.** Evidence supports a disposition; the authorized maintainer makes or delegates the decision explicitly.

## Authority and noninterference

This research policy supplements, but **does not amend**, the repository's existing [agent instructions](../../AGENTS.md), [current successor state](../current/STATE.md), [ANVIL architecture](../current/ANVIL_SUCCESSOR_ARCHITECTURE.md), or [Build-Ready contract](../../zcode_packs/temporal_forge_anvil_zcode_pack_2026-10-05/BUILD_READY_CONTRACT.md).

For research-claim discipline, reuse the definitions and safeguards of the [existing evaluation standard](../closure/EVALUATION_STANDARD.md), while recognizing that document's formal authority over the **closed FSR-era** campaign. The new successor campaign's policy must not silently promote an archived FSR plan into active authority.

**This branch intentionally adds only these standalone policy files.** It does not edit `AGENTS.md`, the root/docs index, existing contracts, application code, tests, or the historical review harness. Cross-links into the main documentation navigation and executable details are deferred to coordinated post-implementation reconciliation.

## Publication state

These documents are proposed for independent review in an unmerged docs-only PR. Their appearance on a feature branch is not authorization to launch Open Tryouts, declare candidate winners, merge a roster, or reopen FSR research.
