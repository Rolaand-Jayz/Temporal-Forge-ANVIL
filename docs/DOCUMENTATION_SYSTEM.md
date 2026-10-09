# Temporal Forge Documentation System

**Status:** CURRENT  
**As of:** 2026-10-08  
**Purpose:** Define authority, historical integrity, and evidence navigation for a repository that now contains both the closed FSR-era record and the active Temporal Forge / ANVIL successor program.

## 1. Governing principle

The repository must make it easy to distinguish:

```text
WHAT IS TRUE NOW
        ↓
WHAT GOVERNS THE ACTIVE ANVIL SUCCESSOR
        ↓
WHY THE FSR ERA ENDED
        ↓
HOW THE HISTORICAL PLAYER WORKS
        ↓
WHAT THE HISTORICAL EXPERIMENTS FOUND
        ↓
WHAT WAS BELIEVED OR PLANNED AT EACH TIME
        ↓
WHERE PRIMARY EVIDENCE LIVES
```

Historical truth must be preserved without allowing historical instructions to become current authority. Successor work must remain separately identifiable from the closed FSR campaign even though both now live in the same repository.

## 2. Current authority

### Successor program

Authority for the ANVIL successor line (merged into `main` by PR #1, `36952dce`, 2026-10-09) is:

1. Explicit maintainer direction — authorization, protected actions, and merge authority.
2. The governing ANVIL execution-pack contracts under [`../zcode_packs/temporal_forge_anvil_zcode_pack_2026-10-05/`](../zcode_packs/temporal_forge_anvil_zcode_pack_2026-10-05/).
3. Current executable source and tests — implementation truth.
4. [`current/ANVIL_BUILD_READY_QUALIFICATION_20261005.md`](current/ANVIL_BUILD_READY_QUALIFICATION_20261005.md) — Build-Ready gate/evidence record; the gate was closed `TRUE` on 2026-10-09 by independent evaluator adjudication (CLEAN verdict at `fe7dac22`) plus maintainer merge authorization — never a self-approval.
5. [`current/ANVIL_SUCCESSOR_ARCHITECTURE.md`](current/ANVIL_SUCCESSOR_ARCHITECTURE.md) — current successor architecture/reference.
6. [`current/STATE.md`](current/STATE.md) — concise repository/program status summary.
7. The PR #1 evaluator adjudication record (final CLEAN verdict at `fe7dac22`; merge `36952dce`).

When these conflict, governing contracts and validated current implementation/evidence outrank stale narrative summaries.

### Historical FSR record

Authority for interpreting the closed FSR era remains:

1. [`closure/`](closure/) — final adjudication, evaluation standard, claim dispositions, limitations, and successor boundary.
2. [`reference/`](reference/) — how the preserved historical implementation works.
3. [`decisions/TECHNICAL_HISTORY.md`](decisions/TECHNICAL_HISTORY.md) — causal direction changes.
4. Primary benchmark manifests/artifacts and dated reports — experiment evidence.
5. [`archive/`](archive/) and former `active/` records — historical context only.
6. Exploratory research, hypotheses, prompts, and unverified narrative.

Closure documents remain authoritative for whether the FSR-centered campaign is active: **it is not**.

## 3. Documentation classes

### Current state

Answers: **What is true now?**

[`current/STATE.md`](current/STATE.md) is the concise cross-program state summary. It identifies the FSR closure boundary, the merged successor line on `main`, the current implementation/evidence head, the adjudicated gate state, and current authority.

### Current successor architecture

Answers: **How does the ANVIL successor work?**

[`current/ANVIL_SUCCESSOR_ARCHITECTURE.md`](current/ANVIL_SUCCESSOR_ARCHITECTURE.md) must match current successor code or explicitly state divergence. It is not a rewrite of the historical FSR player architecture.

### Current successor qualification

Answers: **What evidence supports the Build-Ready gate and how was it adjudicated?**

[`current/ANVIL_BUILD_READY_QUALIFICATION_20261005.md`](current/ANVIL_BUILD_READY_QUALIFICATION_20261005.md) records contract-by-contract evidence, evaluator repair history, literal-head CI evidence, and non-blocking risks. The gate closed `TRUE` on 2026-10-09 through the independent evaluator's CLEAN verdict and the maintainer's merge of PR #1 — the record never self-approved it. Historical addenda inside the record describe their own dated heads and remain evidence.

### Closure

Answers: **Why did the FSR research era end, what survived evaluation, and what may be carried forward?**

Closure documents are not rewritten to make the successor look inevitable. They synthesize the historical record while keeping measured facts, inference, unresolved questions, and project decisions distinct.

### Historical reference / architecture

Answers: **How does the preserved FSR-era implementation work?**

Reference documents describe the historical player. They do not imply that its architecture governs ANVIL.

### Decisions / causal history

Answers: **Why did the project change direction?**

Preserve material chains such as:

```text
problem → hypothesis → experiment → evidence → conclusion → decision
```

### Reports / experimental results

Answers: **What happened in a dated experiment or campaign?**

Reports are evidence under a specific project state. They do not become current architecture merely because they were once authoritative.

### Research

Answers: **What external or exploratory information informed the work?**

Research does not silently become verified project behavior.

### Archive

Contains completed, superseded, abandoned, or historical plans, prompts, gates, and progress records. Imperative wording inside archived material is historical text, not execution authority.

### Former `active/` paths

At FSR closure, several widely linked files still lived under `docs/active/`. Their paths are retained as **tombstones only** for link stability. Nothing under `docs/active/` is an active FSR plan after 2026-09-15.

## 4. Evidence and interpretation

Where materially important, distinguish:

- measured fact;
- direct observation;
- inference;
- hypothesis;
- unresolved question;
- project decision;
- actual implementation state;
- intended behavior;
- invalidated evidence.

Use [`closure/EVALUATION_STANDARD.md`](closure/EVALUATION_STANDARD.md) as the repository evidence vocabulary unless a stronger task-specific contract applies.

Conditional evidence stays conditional. Negative results stay visible. Invalid evidence may remain historically important but cannot support a stronger claim after invalidation.

For ANVIL, a green test or CI run proves only the behavior exercised by that evidence. It does not automatically prove an architectural contract, quality claim, or Build-Ready adjudication.

## 5. Historical integrity

Never rewrite old plans, experiment reports, or closure artifacts to manufacture hindsight.

Correct pattern:

```text
At the time: X was believed because of evidence A.
Experiment: B tested X.
Result: B contradicted or qualified X.
Decision: the project changed direction.
```

When a historical document contains stale authority language, prefer one of:

- leave it in an unmistakable archive context;
- add an archive index/tombstone;
- link to an immutable commit containing its exact former contents.

Do not silently edit a historical experiment so it appears to have predicted its later outcome.

## 6. Contradiction resolution

For active ANVIL implementation questions, start from:

```text
governing successor contract + explicit maintainer decisions
        ↓
validated current implementation behavior
        ↓
current source/tests and revision-specific evidence
        ↓
current successor architecture / qualification / state docs
        ↓
worker claims / PR prose / older summaries
```

For historical FSR interpretation, start from:

```text
validated historical runtime behavior
        ↓
historical code
        ↓
validated experiment provenance
        ↓
dated primary evidence
        ↓
closure / historical reference docs
        ↓
historical plans
        ↓
research hypotheses / prompts / unverified narrative
```

These are reasoning hierarchies, not permission to erase conflicting evidence.

## 7. Benchmark evidence

Raw benchmark/evidence systems remain authoritative for detailed measurements. Narrative documents should provide the conclusion, critical supporting numbers, scope/qualification, and a path to primary evidence rather than duplicating giant result tables.

The broad ANVIL adversarial reconstruction-quality campaign remains separate from Build-Ready engineering qualification. Build-Ready evidence must not be presented as image-quality proof.

## 8. Git history

Git history is evidence, not the sole user interface for understanding the project.

The FSR closure uses immutable commit links for exact pre-closure records. The ANVIL branch preserves repair/evaluator history rather than rewriting it. Documentation corrections should use ordinary forward commits.

## 9. Agent/contributor rule

A new agent or contributor working on this repository must read:

1. [`../AGENTS.md`](../AGENTS.md);
2. [`README.md`](README.md);
3. [`current/STATE.md`](current/STATE.md);
4. for ANVIL work, [`current/ANVIL_SUCCESSOR_ARCHITECTURE.md`](current/ANVIL_SUCCESSOR_ARCHITECTURE.md) and the governing execution pack;
5. for FSR-era interpretation, [`closure/README.md`](closure/README.md).

No historical plan may be resumed merely because it contains instructions or unfinished gates. Behavioral code changes require explicit maintainer authorization. PR #1 was merged on 2026-10-09 only after that authorization was given (evaluator CLEAN verdict at `fe7dac22`, maintainer-approved merge `36952dce`).

## 10. Successor boundary

The successor now lives on `main` in this repository after development on `successor/anvil-build-ready` and the maintainer-authorized merge of PR #1, rather than in a separate successor repository.

That topology change does **not** collapse the evidence boundary:

- FSR-era closure remains historical authority for the closed campaign.
- ANVIL contracts, implementation, tests, and current successor documents govern the active successor.
- FSR-specific assumptions are not inherited unless independently justified.
- Pre-successor `main` history is preserved; do not rewrite history to make the successor appear to have always existed there.

## 11. Maintenance

Current documentation maintenance may include:

- reconciling current ANVIL status, architecture, qualification, and PR evidence with the exact repository/revision state;
- correcting broken links or demonstrably false current-status claims;
- clarifying false or ambiguous historical claims without rewriting historical outcomes;
- improving reproducibility without changing established evidence;
- security or licensing/provenance corrections;
- evidence indexing;
- explicitly requested restoration or historical investigation.

Do not create a new standing FSR quality plan by editing current docs. The ANVIL Build-Ready gate was adjudicated `TRUE` on 2026-10-09; any future gate or status promotion must likewise come from evidence plus the required adjudication/maintainer authority, never from documentation edits alone.
