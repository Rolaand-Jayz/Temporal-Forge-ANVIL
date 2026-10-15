# Open Tryouts — candidate eligibility and screening rules

**Status:** Proposed research policy; invitations are not endorsements or qualification results.
**Execution:** Not yet authorized as a launched campaign.

## 1. Who can try out?

Candidates may be a single algorithm, a component-stage replacement, a standalone external pipeline, or a compatible combination with a testable hypothesis. Examples of **positions**, not approved integrations:

| Position | Illustrative invite types | Legitimate opening question |
|---|---|---|
| Spatial controls | Bicubic, Lanczos, other trustworthy scaling baselines | What quality can spatial-only processing deliver? |
| Correspondence | ANVIL estimates, codec side information, classical or learned optical flow | Can this method improve valid alignment at acceptable cost? |
| Reliability | Occlusion checks, confidence weighting, residual-guided selection | Does it prevent bad neighbors from contaminating output? |
| Geometry | Known, estimated, or oracle phase methods | Does real sampling displacement supply recoverable detail? |
| Fusion | Existing accumulator, robust estimators, adaptive neighbor selection | Does multi-frame fusion help beyond no temporal accumulation? |
| Learned reconstruction | Lightweight recurrent or other verified models | Does the model add useful quality and temporal stability at feasible cost? |
| Optional external reconstruction | FSR 4.1 and other implementation-dependent methods | Can a technically valid, licensed, reproducible comparison even be made? |

An invitation establishes a **hypothesis and test plan**. It does not commit ANVIL to an external dependency, model, license, platform, port, or architectural choice.

## 2. Qualification before a score

Verify that each candidate can legitimately attempt the assigned task:

- Input format, pixel/color space, dimensions, timestamps, temporal context, and motion/phase prerequisites are compatible.
- The actual named algorithm executes; log fallback/bypass and warn on passthrough.
- Output semantics are understood: native reconstruction, higher-resolution delivery, interpolation, restoration, or another task.
- Required model weights, binary assets, APIs, and legal/redistribution boundaries are available or explicitly limited.
- Reference data is independent of reconstruction inputs; ground truth cannot leak into an estimated arm.
- The same requested frame and comparable output conditions are available for the corresponding control.

A failed qualification is **NOT** evidence of inferior reconstruction quality. Record the exact blocker and a falsifiable next step, or decline the candidate without misrepresenting it.

## 3. First-round format

The opening screen is deliberately small: **two contrasting scenes** and **two controlled spatial scaling methods** are proposed as the initial budget, not as final proof of generalization.

A sensible initial contrast:

- **Fine detail / modest motion:** thin edges, textures, small movement, stable geometry.
- **Motion / occlusion:** object crossings, boundaries, disocclusion, shifting detail.

Bicubic and Lanczos are reasonable opening **spatial controls**. They are not substitutes for comparing native temporal outputs. The exact assets, resolution ratios, clip intervals, and sweep size must be determined from valid available evidence and approved experiment design.

Use a modest, diverse configuration sweep. Exhaustive combinatorial search on only two scenes encourages selection overfit and consumes budget without learning proportional value.

**No current numerical sweep count, scene filename, algorithm-version choice, or performance target is mandated by this policy.**

## 4. Fair-match rules

- Compare each method with an appropriate **role-specific baseline** and a meaningful **end-to-end contribution** test where possible.
- When claiming isolation, change only the stated factor(s). If several factors changed, name each one and limit causal claims.
- Use the same frame identity, source, temporal sampling, delivery dimensions, scale factor, and controlled color interpretation for each valid paired comparison.
- Separate processing-quality metrics from subjective preference and from runtime cost.
- Give participants a disclosed, reasonable, comparable tuning allowance. Do not train or optimize a favorite extensively while evaluating others only in default mode.
- Do not punish techniques with different purposes for failing another role's metric; judge output-specialists on a matched output task.
- Do not silently count a disabled stage, unsupported codec side information, empty valid-sample set, or fallback path as an algorithm's successful tryout.
- Preserve a no-accumulation/single-frame control and qualified high-resolution reference when available.
- Distinguish codec-side information and true optical-flow measurement from guessed motion priors.
- Avoid automatic scoring changes after seeing results unless revised criteria are labeled as exploratory.

## 5. Scouting and escalation

After the small opening screen, each candidate receives a **scouting report**, not an automatic promotion:

1. Evidence of the candidate doing the intended work.
2. Role-specific strengths and weaknesses.
3. Direct comparison to valid controls.
4. Measured or observed impact when combined with other stages.
5. Temporal failure modes and performance costs.
6. Source limitations, contradictory cases, and confidence in conclusions.
7. Suggested next disposition and the precise evidence needed to change it.

Only surviving or diagnostically promising candidates advance to longer sequences and unseen scenes. **Held-out scenes must remain unseen during first-round tuning**, and generalization is assessed before claiming broad superiority.

A method can be promising because it handles an edge case, reduces cost, enables another candidate, or provides a high-quality oracle upper bound—not just because it tops an aggregate visual metric.

## 6. Early-exit and minor-league policy

Stop spending early-screen resources when:

- Comparable evidence shows no incremental benefit **under the tested conditions**.
- The implementation is invalid, illegal to distribute in the proposed form, or cannot be fairly tested.
- The method's incremental value is dwarfed by cost relative to realistic alternatives.
- A fundamental limitation is found and confirmed.

Do **not** translate every early exit into `cut`. Candidates with unresolved integration issues, plausible synergy, narrower use cases, or a promising but unaffordable design may merit `bench` or `minor` with a written callback condition.

Record what changed if a callback is later justified: e.g., valid motion inputs, a new subpixel estimator, cheaper inference, better codec support, a reliable port, or new independent evidence.

## 7. FSR 4.1 treatment

FSR 4.1 is invited for a possible technically qualified comparison. It is **not** a default dependency, inherited core, guaranteed starter, or permission to reopen the historical FSR-centered campaign. Establish correct input semantics, reproducible execution, and rights/provenance constraints before measuring it. A failure to qualify its integration must not be relabeled as a measured quality defeat.

## 8. What the opening round does not prove

Two scenes can help direct research investment, **not** establish universal quality superiority, production readiness, 30/60 FPS, or generalization across codecs and scenes. Promotion claims are bounded by the evidence. See [SCOUTING_REPORT_STANDARD.md](SCOUTING_REPORT_STANDARD.md) and [ROADMAP.md](ROADMAP.md).
