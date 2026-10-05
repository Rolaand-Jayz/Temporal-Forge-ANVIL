# Evidence and Architecture Guardrails

These rules prevent the successor effort from turning uncertain research into implementation folklore.

## Authority order

1. Maintainer's current goal and explicit decisions.
2. New successor architecture decisions that survive review and are recorded during this effort.
3. `Temporal-Forge-Player/docs/closure/EVALUATION_STANDARD.md`.
4. Reproducible current repository behavior when code is intentionally reused.
5. Valid measurements/artifacts produced by the new effort.
6. `RE-of-FSR-4.1.0-Upscaling/VALIDATION_STATUS.md` and `CURRENT_STATUS.md`.
7. Other historical FSR-era material as evidence/context only.
8. External literature/specifications, with source and applicability stated.
9. Inference/hypothesis.

Archived imperative text is not active authority.

## FSR 4.1 claim ceiling

The RE repository establishes substantial **static** structure, but explicitly does not establish:
- runtime pass order;
- runtime descriptor bindings;
- runtime CBV values;
- runtime tensor-offset use;
- functional equivalence to AMD's DLL;
- complete private FSR semantics.

Do not promote `STATIC-INFERRED` to runtime fact.

In particular:
- a statically inferred `motion/reprojection` resource name does not prove the exact runtime representation or semantics;
- absence of a directly identified depth SRV does not by itself prove that depth is semantically unnecessary at the useful integration boundary;
- 27 host/model-loop passes are not automatically "27 neural layers";
- public FidelityFX Super Resolution timing numbers are not automatically performance measurements for this RE Vulkan path.

## Codec motion-vector guardrails

- Distinguish **encoded codec motion vectors** from running a new estimator such as FFmpeg `mestimate`.
- Verify H.264, HEVC, and AV1 behavior separately.
- Preserve reference-frame identity, direction, temporal distance, block/partition geometry, precision/scale, frame type, skip/intra status, compound/bi-pred semantics when accessible.
- Do not assume hardware decode exposes motion vectors merely because the codec contains them.
- Do not claim VA-API/VCN zero-copy MV access without proving the exact API/driver path on the target stack.
- Missing/ambiguous codec metadata must have an explicit fallback; never silently fabricate it.
- Do not default to Gaussian smoothing across motion boundaries. Any densification/refinement must preserve discontinuities or demonstrate why not.

## Sampling-phase guardrails

- `fract(mean(motion_vector))` is **not** a valid general jitter/sample-phase estimator.
- Motion, camera transform, sampling-grid phase, and object motion are distinct quantities.
- Artificially shifting/resampling the same LR frame does not create new source information.
- If an artificial phase changes a backend's output, classify it as a backend heuristic/perturbation unless independent observations supplied new samples.
- Genuine multi-frame detail recovery requires independently informative observations plus sufficiently accurate registration and visibility modeling.
- The system must support proving where recovered detail came from.

## Confidence / occlusion guardrails

Use a backend-neutral confidence representation, e.g. `C(x) in [0,1]`, plus visibility/validity metadata where useful.

Do not equate:
- occlusion mask;
- disocclusion mask;
- motion confidence;
- codec residual;
- FSR Reactive Mask;
- Transparency & Composition mask.

Backend-specific mappings are adapters and hypotheses until validated.

Forward/backward consistency must compare inverse correspondences at the warped position, not vectors sampled at unrelated coordinates.

## Architecture-neutrality

FSR is an optional comparison/backend path, not the center of the successor architecture.

Do not inherit by default:
- FSR jitter semantics;
- renderer-style depth;
- causal-only history;
- fixed temporal support;
- fixed scale ratios;
- old postpass/composition;
- optical flow;
- machine learning;
- one-frame-in/one-frame-out if an offline path legitimately benefits from future frames.

Reuse old code only when the successor problem independently justifies it.

## Evidence vocabulary

Consequential claims must be classed as:
- MEASURED FACT
- OBSERVATION
- INFERENCE
- HYPOTHESIS
- UNRESOLVED
- PROJECT DECISION

Negative results remain first-class evidence.
