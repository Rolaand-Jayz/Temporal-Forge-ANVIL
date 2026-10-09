# ANVIL research league — campaign roadmap

**Status:** Proposed plan for subsequent research; **not** a record that the campaign has started or passed.
**Method:** Gates are evidence decisions, not calendar dates or automatically completing checkboxes.

## At a glance

```text
Build-Ready gate (already adjudicated true; engineering qualification)
       |
       v
Home Field Exhibition   — measure frozen ANVIL reconstruction baseline
       |                  and build trusted human visual comparison
       v
Open Tryouts            — low-cost screening by algorithmic position
       |
       v
Spring Training         — held-out, temporal, end-to-end, synergy tests
       |
       v
Roster Selection        — select complementary contributors for a
       |                  provisional research architecture
       v
Training Camp           — optimize measured team bottlenecks and runtime
       |
       +----> Minor leagues / callbacks — new evidence may reopen a tryout
```

## Gate 0 — Engineering readiness

**Historical status:** ANVIL's successor Build-Ready gate was adjudicated `TRUE` on 2026-10-09. This validates research-rig engineering requirements, **not** image quality or production performance.

Evidence resides in [current successor status](../current/STATE.md) and the [Build-Ready qualification](../current/ANVIL_BUILD_READY_QUALIFICATION_20261005.md).

## Gate 1 — Home Field Exhibition

**Separate ZCode assignment, not completed by these documents.**

Deliver:

- Fixed, independently reproducible **ANVIL baseline** identity.
- Honest control separation: decoded frame, native ANVIL reconstruction, derived spatial delivery output, and HR ground truth where available.
- Small but valid two-scene quality assessment, including adverse temporal content.
- Proof that selected reconstruction stages execute and actually contribute samples.
- Valid spatial, temporal, and cost evidence with provenance and limits.
- **Human-facing Visual Review Lab** for matched A/B selection, exact method labeling, sequence inspection, and saved observations.
- Clear negative/inconclusive findings.

**Advance only when:** baseline is genuinely reproducible, pairings and metrics are valid, failures are visible, review tooling works, and an authorized reviewer accepts the evidence basis. An unfavorable ANVIL score is useful—lack of trustworthy evidence is the blocker.

## Gate 2 — Open Tryouts

**Objective:** Learn where to invest research effort, not establish a champion in one afternoon.

- Qualify each invite's actual input/implementation contract.
- Group by positions: correspondence, visibility, confidence, geometry, fusion, spatial scaling, learned reconstruction, complete alternatives.
- Start with two contrasting scenes and controlled scaling baselines as a small screening set.
- Use a diverse sample of configurations rather than exhaustively fitting two scenes.
- Measure individual position performance and potential team contribution.
- Record costs, failed qualifications, neutral outcomes, and causal limitations.

**Advance only when:** each surviving prospect has a scoped, defensible hypothesis and a scouting report that explains the next experiment. Methods can enter `bench`, `minor`, or `cut` where evidence warrants; invitation alone confers no roster spot.

## Gate 3 — Spring Training

**Objective:** Discover whether apparent talent holds up under realistic pressure.

- Longer sequences with multiple motion and scene transitions.
- Held-out scenes not used for tryout tuning.
- More than one resolution and codec/degradation condition where relevant.
- Explicit artifact scrutiny: ghosting, disocclusions, hallucinations, aliasing, periodic patterns, flicker.
- Controlled stage ablation, combined-method interactions, and runtime/profile replication.
- Separate oracle upper bounds from estimated, deployable methods.
- Revisit fair tuning budgets rather than accidentally favoring a candidate.

**Advance only when:** claimed benefit survives representative adverse tests or is explicitly qualified to a narrow use case. Confirm that a candidate doesn't merely disguise artifacts through blur or sharpening.

## Gate 4 — Roster Selection

**Objective:** Form the most coherent candidate architecture, not maximize the count of famous algorithms.

- Allocate positions to complementary contributors based on incremental value.
- Recognize hidden supporting roles; ask whether excluding one degrades the whole.
- Document tradeoffs between quality, stability, complexity, and performance.
- Keep baseline frozen, even if selected candidates later merge into `main`.
- Mark `rookie` only with demonstrated unmerged value; `starter` only with authorized accepted integration and actual merge.
- Preserve rejected/minor-league candidate evidence and callback triggers.

**Advance only when:** an explicit authorized architectural decision identifies the roster and cites comparative evidence. The first lineup remains revisable.

## Gate 5 — Training Camp / Optimization

**Objective:** Optimize the chosen team *as a system*—no premature architectural lock-in.

Research directions may include stage-level acceleration, conditional computation, dynamic substitutes, memory/scheduling and pipeline handoffs, cached priors, and native AMD/Linux portability. The informal term **footsketball** may be used to discuss cooperative runtime execution; it is not an engine or API specification.

Any speedup must preserve validated quality and temporal behavior in matched tests. Test substitutions and regressions explicitly. If an apparently weak player becomes useful due to a new partner or fast implementation, issue a scoped callback rather than inventing retrospective success.

## Minor-league loop

```text
candidate not selected
    |
    +--> minor (specific plausible value + measurable callback condition)
    |       |
    |       +--> new evidence / integration feasibility / new teammate
    |                  |
    |                  +--> fresh tryout and scouting report
    |
    +--> cut (bounded evidence-backed lack of value)
            |
            +--> preserve history; reassessment requires a new reason
```

## Milestones not yet authorized

This roadmap **does not** set an implementation schedule, launch a candidate experiment, declare an algorithm eligible by license, choose a deployment platform, select model weights, or alter ZCode's Home Field Exhibition deliverables.

Actual work proceeds only through explicit maintainer authorization and the normal independent review/merge process.

For gate evidence, see [SCOUTING_REPORT_STANDARD.md](SCOUTING_REPORT_STANDARD.md). For operational policy, see [CHARTER.md](CHARTER.md) and [ROSTER_POLICY.md](ROSTER_POLICY.md).
