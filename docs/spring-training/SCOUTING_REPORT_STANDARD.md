# ANVIL scouting report — evidence and adjudication standard

**Status:** Proposed research-policy standard; no results in this document are measured campaign findings.
**Use:** Individual methods, complete pipelines, integration pairs, unsuccessful attempts, and minor-league callbacks.

This standard borrows the disciplined claim taxonomy of [Temporal Forge's evaluation standard](../closure/EVALUATION_STANDARD.md) without reopening the closed FSR-centered campaign. Its purpose is to make tryout decisions falsifiable and easy to review.

## 1. Report objective

Every report should answer these questions in language a researcher and a non-specialist can both follow:

1. **Who's trying out?** Exact candidate and **position**.
2. **What's the hypothesis?** Which failure or bottleneck could the candidate fix?
3. **What actually ran?** Reproducible implementation/configuration identity, not a marketing method name.
4. **Against whom?** Fair controls and role-specific baseline.
5. **What changed?** Isolated variable(s), documented confounders, and valid execution confirmation.
6. **What happened?** Quality metrics, temporal observations, performance, and negative cases.
7. **How strong is the evidence?** Sample limitations, uncertainty, conflicting evidence, holdout status.
8. **Would it help the roster?** Marginal/combined contribution, not only standalone strength.
9. **What's the recommendation?** Proposed status with rationale; final disposition only under authority.
10. **What would change our mind?** Callback or additional test criterion.

## 2. Evidence classes

Distinguish explicitly:

- **Measured fact:** a reproducible quantity or traced behavior with identified inputs, tooling, and outputs.
- **Observation:** human review with scene/frame/region and conditions.
- **Inference:** interpretation supported by findings but not directly measured.
- **Hypothesis:** falsifiable proposition to test.
- **Unresolved:** insufficient, conflicting, invalid, or blocked evidence.
- **Project decision:** authorized candidate disposition or architectural commitment, not itself a scientific fact.

Where multiple classes are present, label each claim rather than blurring them together.

## 3. Minimum experiment evidence

A report should allow a qualified reviewer to trace the chain from candidate to observation:

| Evidence category | Expectations |
|---|---|
| Implementation | Source revision, model/weight identity if applicable, relevant binaries/dependencies, changed/disabled stages |
| Inputs | Scene/source identity, license/provenance limitations, compression/degradation, codec, time window, frame/timestamp, hashes when available |
| Controls | Native decoded-frame, frozen ANVIL baseline, appropriate role-control, correct spatial delivery/reference arms as applicable |
| Configuration | Active parameters and alternatives, oracle use, fallback states, tuning budget, input/output dimensions and color semantics |
| Execution proof | Stage/dispatch trace sufficient to distinguish actual candidate execution from passthrough or fallback; valid sample participation |
| Outputs | Retained images/sequences, paired frame identity, hashes and derivative lineage, errors and exclusions |
| Metrics | Correctly aligned PSNR/SSIM or other fidelity metrics when real references exist, edge/detail measure, temporal defect measures as meaningful |
| Human inspection | Image pair, frame, crop/region, viewer settings if relevant, what was visible, and whether finding was blind or informed |
| Cost | Per-stage and whole-run time, memory/hardware context, initialization and preprocessing overhead when relevant |
| Interpretation | What improves, regresses, or remains inconclusive; plausible confounders; bounded claims |
| Disposition | Proposed and then authorized state, evidence link, and next-test/callback criteria |

This is a **conceptual checklist**, **not** a mandated JSON layout or directory structure.

## 4. Validity gates

Before interpreting a score:

- Confirm both arms refer to the same intended source scene and frame/time.
- Confirm scale ratio, output geometry, color interpretation, crop, and reference-space assumptions.
- Confirm required temporal neighbors exist and any scene-cut exclusions are truthful.
- Confirm the candidate executed with valid data rather than taking a fallback route.
- For mixed pipelines, separate native temporal reconstruction from subsequent spatial scaling and sharpening.
- Confirm that ground truth did not leak into reconstruction. Oracle/estimated trials must be clearly separated.
- Confirm no mismatched screenshots, lossy previews, or alignment edits were used as though they were native scientific pixels.

If a gate fails, record **invalid/not qualified** instead of assigning a winning/losing quality score.

## 5. Scoreboard dimensions

Keep at least these dimensions distinct:

- **Spatial:** reference-relative fidelity and structure, ringing, hallucinated detail, texture loss.
- **Temporal:** flicker, ghosting, shimmer, motion boundaries, disocclusion, stability throughout sequences.
- **Contribution:** stage ablation, synergy/conflicts with teammates, location-dependent strengths.
- **Cost:** CPU/GPU time, inference latency, memory pressure, bandwidth, throughput, portability.
- **Robustness:** codec/resolution/scene dependence, color/HDR assumptions, failure handling.
- **Evidence confidence:** how many independent sequences, how many frames, matched baselines, and whether evaluation was held out.

A single aggregate number may help organize a table but must **not** replace the multidimensional record.

## 6. Human review standard

The review interface is intended to make the exact A/B identity obvious:

- Display actual source/control/candidate labels; never "latest", "better", or generic unnamed output.
- Identify whether the image was **temporally reconstructed at native resolution**, **spatially upscaled**, or reconstructed with a learned super-resolution method.
- Show what stages changed and what remained unchanged.
- Offer frame-by-frame inspection and registered comparison tools; do not mistake a display transform for a scientific metric.
- Record reviewer-selected crops and observations against specific artifact and configuration identities.
- Ensure a concrete visual defect can reopen an apparently positive automated assessment.

A human preference is an observation; it cannot override failed provenance or establish factual recovery of details unsupported by the source.

## 7. Evidence confidence and sampling

Two scenes are appropriate for **low-cost scouting** and hypothesis refinement only. They are not enough to claim generality.

Avoid tuning to the same scenes used for final selection. Protect unseen scenes, longer sequences, different motion/occlusion patterns, and appropriate alternate codecs/resolutions for the later Spring Training validation.

Document missing coverage. Report per-scene and per-condition results so an average cannot hide a severe defect.

A candidate that wins only under restricted conditions may still have value, but its claimed scope stays restricted.

## 8. Decision template (human readable, not a file schema)

**Player / Position:**

**Roster status when tested:**

**Question / hypothesis:**

**Eligible task / valid input contract:**

**Candidate versus controls:**

**What changed / what stayed fixed:**

**Execution proof / artifact provenance:**

**Measured spatial findings:**

**Measured temporal findings:**

**Cost and operating conditions:**

**Human review observations:**

**Regressions and contradictory evidence:**

**Validity gaps / alternative explanations:**

**Interaction with prospective teammates:**

**Proposed disposition and supporting evidence:**

**Authorized disposition (if decided):**

**Next test or callback condition:**

## 9. No automatic decision

The report provides the evidence for a human-authorized roster decision, as governed by [ROSTER_POLICY.md](ROSTER_POLICY.md). Preserve unsuccessful reports. Never convert evaluator comments, metric thresholds, or PR merge mechanics into silent promotions.
