# ANVIL roster policy — identity, status, decisions, and callbacks

**Status:** Proposed — Implementation-independent research policy
**Important:** This defines naming and decision meaning, **not** executable schema, API, database, or UI internals.

## 1. Three separate identities

**Do not conflate these concepts:**

1. **ANVIL baseline:** a **fixed technical reference** representing the original ANVIL reconstruction implementation and its exact canonical configuration. Its source, parameters, and environment prerequisites must ultimately be reproducible from immutable evidence, independent of the evolving `main` branch. This is **not** the decoded-frame control. The exact reproducible definition belongs to the separate Home Field Exhibition implementation and verification effort. This policy does **not** prematurely declare a SHA or manifest structure.
2. **Candidate / configuration identity:** the exact method being tested, including additions, replacements, disabling, model versions, parameters, input/output conventions, and baseline lineage. A different scientifically meaningful configuration is a different technical candidate or version; do not hide it behind the same name.
3. **Roster disposition:** a **changeable** human research decision about the candidate's current standing. It must not change technical identity or rewrite the outcome of past experiments.

A method can be accepted into `main` without changing the original baseline. `main` reflects today's active lineup; **ANVIL baseline** always reflects the frozen reference.

## 2. Required human-facing naming language

- Fixed original: **`ANVIL baseline`** (no suffix).
- Derived candidate: **`ANVIL baseline + <specific modification> + <specific modification> — <status>`**.
- Non-ANVIL external control: **`Spatial control — Lanczos 4×`**, **`Decoded-frame control`**, **`Ground truth`**, etc.; never add the ANVIL prefix if its pipeline did not use ANVIL lineage.

The modifier chain is an intelligible **summary**, not a replacement for the complete technical record. For a component replacement, an expanded description must explicitly say **replaces**; do not imply both predecessor and replacement executed. Include disabled stages and changed parameters when material.

Illustrative names **only**, not claims about existing candidates:

- `ANVIL baseline + DIS optical flow — tryout`
- `ANVIL baseline + estimated geometry — bench`
- `ANVIL baseline + robust fusion — rookie`
- `ANVIL baseline + robust fusion + Lanczos 4× — starter`
- `ANVIL baseline + NanoVSR 644K — minor`
- `ANVIL baseline + FSR 4.1 refinement — cut`

Names like "Current Forge", "Enhanced", "Latest", "New baseline", and "Test 3" are insufficient because they conceal what executed. Labels must reflect **observed configuration**, not intent.

Use **temporal reconstruction** for same-resolution temporal processing, **spatial upscaling** only when actual output dimensions increase, and distinguish a finishing scaler from recovered temporal information.

## 3. Six dispositions

| Status | Qualification meaning | What does **not** qualify |
|---|---|---|
| `tryout` | Invited and undergoing the **first** technically qualified evaluation | A name on a wishlist with no agreed evaluation |
| `bench` | Experimental, still under development, unproven, or inconclusive | An automatic declaration of failed image quality |
| `minor` | Not selected for the active lineup, but with specific plausible value and a **documented callback condition** | A vague archive with no reason to revisit |
| `rookie` | Repeatable, appropriately controlled evidence of value, **not yet integrated into `main`** | One favorable still or unverified claim |
| `starter` | Accepted into the active architecture **and** actually merged into `main`, with an explicit authorized decision | Merge alone, CI success alone, or unreviewed experiment |
| `cut` | Evidence-backed decision that tested contribution is insufficient for selection **within documented conditions** | Invalid comparison, wrong inputs, missing weights, unsupported execution, or unspecified general failure |

Each candidate has one current disposition when the roster tracks it, but historical experiments retain their recorded contemporaneous state. The system may separately display **current** and **at experiment time** status.

**Disposition is not a universal truth claim.** `cut` does not mean a method can never work. `rookie` does not mean universal superiority. `starter` does not mean the architecture is optimal.

## 4. Disposition decisions

A defensible change records, conceptually (not a prescribed schema):

- Candidate's immutable configuration identity, and the compared baseline/control.
- Prior and new disposition.
- Decision maker and authorization.
- When the decision was made.
- Evidence and result limitations.
- Competing explanations and regression slices.
- Next evidence gate, if applicable.
- Integration evidence for `starter`.
- Callback hypothesis and trigger for `minor`.

No model, script, viewer, metric summary, CI job, or agent may unilaterally turn evidence into an authorized roster promotion. The maintainer has decision authority unless explicitly delegated.

## 5. Example movement through the system

`tryout` → `bench` (promising but unresolved) → `rookie` (replicated benefit; integration pending) → `starter` (accepted and merged) is a **possible**, not mandatory, path.

Other valid decisions include:

- `tryout` → `minor` when an early, bounded mismatch reveals a credible later application.
- `bench` → `cut` when fair, valid testing demonstrates inadequate incremental value in defined conditions.
- `minor` → `tryout` or `bench` for a newly justified evaluation after a callback.
- `starter` → `bench`, `minor`, or `cut` if evidence and architectural decisions warrant removal or deprecation, with the historical merge and disposition retained.

No transition rewrites earlier observations or changes the baseline snapshot.

## 6. The callback principle

Minor-league admission requires a **falsifiable reason** for future reconsideration, for example:

- Missing codec motion vectors become legitimately available.
- A compatible Linux/AMD implementation arrives.
- A new phase estimator removes a measured bias.
- Acceleration reduces cost below a relevant constraint.
- A second algorithm makes a formerly weak component effective in combination.
- A new class of scenes demonstrates a previously unseen narrow strength.

A callback starts a **new** evaluation with a new evidence record. It cannot retroactively change old results.

## 7. Lineup versus individual talent

A strong solo PSNR score does not guarantee a place on the team. A low-profile visibility or confidence method can earn a place by improving another player's effectiveness, suppressing artifacts, or reducing cost. Evaluate candidate contribution with appropriate ablations and interaction tests.

Position-specific fairness does not imply unconditional tenure: after a role is established, it must still demonstrate that it adds enough value to the actual team.

## 8. Future implementation

The Home Field Exhibition / Visual Review Lab agent should implement the agreed policy with an appropriate technical mechanism. This document intentionally does not choose its keys, schema version, storage engine, status APIs, import/export format, or UI layout.

After that work is independently reviewed, map this policy to **verified** implementation behavior in [IMPLEMENTATION_RECONCILIATION.md](IMPLEMENTATION_RECONCILIATION.md). Do not change policy to conceal noncompliance.
