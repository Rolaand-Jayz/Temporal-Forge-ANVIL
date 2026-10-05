# Later Rigorous Research Campaign — Deferred

**Do not execute this campaign during the build goal.**

This file exists so the build exposes everything the later campaign will need.

The later campaign should follow the old Forge's strongest research discipline:
- preserved baselines and controls;
- exact provenance;
- causal ablation;
- negative-result preservation;
- automated metrics plus human visual review;
- independent/adversarial review;
- reopening false-green results when artifacts contradict metrics.

## Campaign layers

1. **Motion prior truth**
   - encoded codec prior vs oracle correspondence vs image-estimated correspondence;
   - H.264 / HEVC / AV1 separately;
   - raw prior, densification, residual refinement.

2. **Visibility/confidence truth**
   - oracle visibility vs estimated confidence;
   - boundary/disocclusion/transparency/blur cases.

3. **Sampling-information truth**
   - controlled HR→LR observations with exact sample positions;
   - uncompressed/lossless controls;
   - H.264/HEVC/AV1 compression;
   - prove new information recovery separately from sharpening/hallucination.

4. **Phase estimation**
   - oracle phase vs estimated phase;
   - global transform vs local motion;
   - realistic acquisition classes.

5. **Temporal reconstruction**
   - single-frame;
   - naive aligned average;
   - confidence-weighted deterministic reconstruction;
   - candidate learned/hybrid methods.

6. **Backend value**
   - deterministic output backend;
   - spatial SR;
   - optional FSR comparison;
   - lightweight learned backend if justified.

7. **Real video**
   - camera-native;
   - streamed/compressed;
   - animation;
   - game capture with/without temporal AA where obtainable;
   - difficult motion/occlusion/blur/noise.

8. **Performance**
   - only after quality/architecture claims survive;
   - per-stage GPU/CPU timings with contention provenance;
   - memory;
   - 30 FPS and 60 FPS feasibility.

## Key falsifiers

The architecture must be narrowed or abandoned if valid evidence shows that:
- codec priors do not reduce correspondence cost enough to matter;
- realistic observations contain insufficient additional spatial information;
- registration/visibility errors erase the theoretical gain;
- phase cannot be estimated robustly enough to help;
- the deterministic accumulator cannot beat strong controls even with oracle inputs;
- a conventional VSR architecture dominates the proposed decomposition;
- complexity/latency is disproportionate to retained source-faithful gain.

No attractive image can override a failed causal test.
