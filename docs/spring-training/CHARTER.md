# Spring Training Charter

**Status:** Proposed — Implementation-independent research policy
**Applies to:** ANVIL successor candidate discovery, comparison, selection, and subsequent optimization

## 1. Mission

Find which algorithms and combinations of algorithms genuinely improve source-supported video reconstruction while respecting fidelity, temporal stability, runtime cost, portability, and provenance. Build the architecture around **measured contributions**, not allegiance to early assumptions, vendor reputation, visual sharpness alone, or framework fashion.

ANVIL is the **home field**—a research instrument rather than a guaranteed winning reconstruction algorithm. The first **Home Field Exhibition** measures ANVIL itself; only after that evidence is qualified do outside candidates enter Open Tryouts.

## 2. Research questions, not predetermined answers

The program should be able to challenge at least three rival hypotheses:

- **Sample-driven:** complementary information already present in multiple low-resolution frames can be recovered by accurate registration, sampling geometry, and fusion.
- **Learned reconstruction:** compact learned priors and feature propagation are necessary for useful quality gains beyond classical reconstruction.
- **Hybrid:** classical, codec-derived, or learned correspondence coupled with selective fusion and/or learned refinement provides a better quality/cost balance.

No hypothesis has protected status. Evidence may support different approaches under different scenes, resolutions, codecs, or runtime constraints.

## 3. Roles and fair opportunity

Test candidates **by role**, then test their **contribution to a team**:

- **Correspondence / motion:** where observations align, with error, coverage, and cost evidence.
- **Visibility / occlusion:** which samples should be admitted or rejected.
- **Confidence / reliability:** whether weighting protects quality and avoids contamination.
- **Sample geometry / phase:** whether genuine subpixel sampling differences are estimated and useful.
- **Temporal fusion / memory:** whether multiple observations add information without blur, ghosting, or flicker.
- **Spatial reconstruction / finishing:** how output is delivered at a chosen scale, kept distinct from temporal information recovery.
- **Complete alternative pipelines:** whether independently reconstructed video competes on an honestly matched task.
- **Scheduling / acceleration:** runtime viability and integration cost; high quality does not erase unacceptable cost.

A candidate may be valuable only when paired with another. A component that doesn't directly output higher-resolution images must not be eliminated solely for not doing a different position's job.

**Fair opportunity** means a valid test, appropriate controls, a disclosed and reasonably comparable optimization allowance, and a success criterion relevant to the candidate's role. It does **not** mean identical algorithms, identical metric interpretation, or endless attempts to rescue one favored candidate.

## 4. Stages

1. **Home Field Exhibition:** establish how the frozen ANVIL baseline performs against a decoded-frame control on real and/or clearly marked synthetic material. This stage is already assigned to a separate ZCode implementation effort; its completion is **not claimed here**.
2. **Open Tryouts:** run low-cost, technically qualified screens; discover strengths, blockers, and weaknesses rather than declare champions.
3. **Spring Training:** repeat promising methods across longer, diverse, held-out sequences; test interactions and reproducibility.
4. **Roster Selection:** compose a provisional architecture from complementary demonstrated contributors and explicit tradeoffs.
5. **Training Camp / Optimization:** only now focus on combined-runtime optimization, scheduling, substitutions, and component-specific tuning. The informal name **footsketball** may be used for discussions of coordinated runtime optimization, not as a technical implementation term.
6. **Minor-league callbacks:** reopen bounded questions when new evidence, tooling, compatible inputs, or faster implementations change a previously identified blocker.

Each transition requires recorded reasons, not just a label change.

## 5. Success is multidimensional

Evaluate separately, when applicable:

- **Spatial fidelity:** appropriate reference metrics; edge/texture retention; avoidance of invented detail.
- **Temporal integrity:** ghosting, flicker, shimmer, disocclusions, motion boundaries, scene cuts, and stability over consecutive frames.
- **Contribution:** controlled ablation, interaction with other players, and demonstrated role-specific benefit.
- **Cost:** latency, throughput, memory, resource demand, preparation cost, and eventual real-time viability.
- **Validity:** compatible inputs, correctly executed stage, matched output geometry/color, reproducible provenance, and legitimate controls.
- **Practicality:** license, redistribution constraints, Linux/AMD feasibility, portability, and maintenance effort.

A sharper image is not sufficient proof of recovered source detail. Smoother playback can reflect blur. Strong PSNR on one frame can coexist with temporal failure. A fast method can be useful despite not winning every fidelity metric.

## 6. Integrity and governance

- Label every claim as a measured fact, observation, inference, hypothesis, unresolved question, or project decision.
- Record competitors and controls before interpreting results whenever practical.
- Retain complete negative and conflicting slices; avoid cherry-picking.
- Keep oracle inputs strictly separate from deployable inputs. Oracles measure headroom; they cannot claim deployable performance.
- Separate single-frame, native-resolution temporal reconstruction, spatial delivery upscaling, and end-to-end video super-resolution.
- Separate **technical nonqualification** (missing valid inputs, compatibility or rights blockers) from **measured underperformance**.
- Do not grant FSR 4.1 preferential integration, but also do not call invalid FSR input an empirical quality failure.
- The authorized maintainer controls roster dispositions, architecture promotions, and merges; the evaluator provides adversarial evidence, not automatic approval.

## 7. Boundaries

This charter specifies **what and why**, not **how**. It creates no requirement for an unverified API, exact configuration schema, UI framework, CI job, database, or computational backend. Execution details will be reconciled with verified successor code after the Home Field Exhibition implementation is reviewed.

See [OPEN_TRYOUTS.md](OPEN_TRYOUTS.md), [ROSTER_POLICY.md](ROSTER_POLICY.md), [SCOUTING_REPORT_STANDARD.md](SCOUTING_REPORT_STANDARD.md), and [IMPLEMENTATION_RECONCILIATION.md](IMPLEMENTATION_RECONCILIATION.md).
