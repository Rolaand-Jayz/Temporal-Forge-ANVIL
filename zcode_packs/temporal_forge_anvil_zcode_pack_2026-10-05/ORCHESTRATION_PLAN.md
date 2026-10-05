# Heavily Orchestrated Build Plan

## Orchestration policy

Use **every available GLM 5.3 Flash worker slot** whenever there is independent ready work.

All workers and subworkers must be **GLM 5.3 Flash**. The parent ZCode session is also expected to run GLM 5.3 Flash for this goal.

The orchestrator owns:
- dependency graph;
- architecture decisions;
- task boundaries;
- merge/integration sequencing;
- conflict resolution;
- evidence adjudication;
- final Build-Ready-for-Research gate.

Workers own:
- repository inspection;
- scoped design proposals;
- implementation;
- tests;
- debugging;
- documentation;
- instrumentation;
- benchmarking infrastructure;
- static analysis;
- visual inspection when required.

Do not give multiple workers overlapping write ownership unless they are explicitly acting as independent reviewers. Prefer disjoint directories/files.

Do not delegate and idle. While workers run, the orchestrator works the current critical path. When a worker finishes, immediately consume its output and launch the next dependency-ready task into the freed slot.

Do not merge a worker's claims merely because it reports success. Inspect its diff, tests, assumptions, and integration effects.

## Phase 0 — Bootstrap and establish truth

Run `BOOTSTRAP.sh`.

Primary working tree:
`/mnt/workdrive/Temporal-Forge-ANVIL`

Evidence-only tree:
`/mnt/workdrive/RE-of-FSR-4.1.0-Upscaling`

Record:
- HEAD SHA;
- branch;
- dirty state;
- compiler/toolchain;
- Vulkan device/driver;
- FFmpeg version/config;
- available codecs;
- existing test status.

Create a dedicated successor branch. Do not rewrite or force-push historical `main`.

## Phase 1 — Parallel repository and architecture audit

Fill all worker slots with independent audits. Suggested streams:

1. **Repository topology / reuse audit**
   - map modules, ownership, threading, dependencies;
   - identify reusable shell infrastructure vs FSR-shaped coupling.

2. **Closure/evidence audit**
   - extract negative evidence and historical failure modes that successor design must not repeat.

3. **Media/decode audit**
   - FFmpeg demux/decode path;
   - frame ownership;
   - timestamps;
   - color metadata;
   - software vs hardware decode;
   - existing motion-vector handling.

4. **Codec side-information audit**
   - current ability to obtain encoded MVs/reference data for H.264, HEVC, AV1;
   - prove actual APIs/code paths rather than assume them.

5. **Vulkan/runtime audit**
   - queues, resource lifetime, staging/import, synchronization, format support, compute pipeline infrastructure.

6. **Existing benchmark/test harness audit**
   - what can be reused;
   - what is too FSR-specific;
   - provenance and artifact format.

7. **Color pipeline audit**
   - decode formats, range, primaries, transfer, chroma siting, HDR handling, linearization points.

8. **Temporal window/scheduling audit**
   - how to support past + future frames without damaging playback/audio semantics;
   - offline versus real-time execution boundaries.

9. **FSR isolation audit**
   - identify exactly what must remain historical;
   - define optional adapter boundary without infecting the core architecture.

10. **Testing/observability architecture**
    - replay manifests, stage dumps, trace schema, deterministic mode, oracle injection, per-stage timing.

11. **Build/CI portability audit**
    - clean build assumptions, optional dependencies, fixture behavior.

12. **Successor architecture proposal**
    - independent proposal constrained by the reconstruction problem, not old FSR shape.

If more worker slots exist, split the above further rather than leaving capacity idle.

### Phase 1 exit
Orchestrator produces one reconciled successor engineering decision document. Conflicts remain explicit. No large implementation begins until file/module ownership and core interfaces are decided.

## Phase 2 — Build testability and successor skeleton first

Before implementing aggressive reconstruction algorithms, build the substrate that will make them falsifiable.

Required infrastructure:

### A. Successor execution path
Create an isolated active successor path that can run without FSR/private assets.

It may reuse:
- demux/decode;
- audio/presentation shell;
- Vulkan context;
- general utilities;
- benchmark infrastructure;

only where independently justified.

### B. Observation model
Represent each observation with enough provenance for reconstruction:
- frame/timestamp/index;
- decoded image planes/working image;
- color metadata;
- codec/frame type where known;
- codec side information with exact reference identity where known;
- past/future relationship;
- validity flags;
- source hash/fixture identity.

### C. Temporal window
Support configurable past/future frames and a center target frame.

The later research campaign must be able to compare:
- causal-only;
- symmetric buffered;
- varying support lengths.

Do not hard-code the ~1 s idea into architecture.

### D. Stage interfaces
The exact naming is flexible, but the architecture must make these independently replaceable/bypassable:
- side-information normalization;
- coarse correspondence prior;
- correspondence refinement;
- visibility/confidence;
- sample-geometry/phase estimation;
- temporal fusion/reconstruction;
- spatial/output backend;
- presentation.

No stage may hide irrecoverable state that prevents controlled ablation.

### E. Deterministic offline runner
Add a headless/offline entry point capable of:
- selecting a fixture/clip;
- selecting exact frame window;
- choosing stages/arms;
- running without UI;
- emitting deterministic artifacts when the selected algorithms are deterministic.

This becomes the primary research harness later.

### F. Artifact/provenance system
Every experimental run must be able to emit a manifest containing:
- git SHA;
- dirty-tree hash/status;
- build type;
- executable hash if practical;
- configuration;
- input identity/hash;
- exact frame indices/timestamps;
- codec/decode path;
- GPU/driver;
- stage selections;
- relevant seeds;
- per-stage timings;
- produced artifact paths.

### G. Intermediate dumps
Make it possible to capture, where applicable:
- decoded source;
- normalized working image;
- raw codec vectors/blocks;
- dense prior;
- refined correspondence;
- forward/back correspondence;
- confidence/visibility;
- phase/sample geometry;
- accumulated/reconstructed target;
- backend input/output;
- final output.

Prefer machine-readable numeric artifacts plus viewable images where useful.

### H. Oracle injection
The harness must be able to replace estimated quantities with known/oracle data for synthetic tests:
- exact flow/correspondence;
- exact camera transform;
- exact sample positions/phases;
- exact visibility/occlusion;
- HR target.

Without oracle injection, later failures cannot be localized.

## Phase 3 — Implement the minimal complete successor candidate

Build the smallest architecture that can exercise the new hypothesis end-to-end without requiring the later research campaign to redesign it.

### 3.1 Codec side-information path
Implement truthful extraction/normalization for supported codecs.

Requirements:
- codec-specific adapters;
- exact units/scale;
- block geometry;
- direction/reference identity where accessible;
- unsupported/ambiguous states represented explicitly;
- clean fallback when data is unavailable.

Do not use `mestimate` and label it encoded MV extraction.

### 3.2 Coarse correspondence representation
Convert available priors into a backend-neutral representation without erasing discontinuities.

Keep the raw side-information artifact alongside any densified form.

### 3.3 Initial residual refinement
Implement at least one **simple, deterministic, testable** refinement path suitable for bringing a coarse prior closer to image evidence.

Do not commit the architecture to a heavyweight neural refiner before the later campaign demonstrates the need.

The interface must allow later addition/comparison of:
- local/hierarchical search;
- DIS-like refinement;
- feature/frequency methods;
- tiny learned residual flow.

### 3.4 Confidence / visibility
Produce general confidence/visibility data using evidence such as:
- warped inverse consistency;
- photometric/structural residual;
- motion discontinuity;
- codec metadata when trustworthy.

Keep it backend-neutral.

### 3.5 Sample geometry / phase
Implement the **representation and estimation hooks**, not unsupported magic.

Requirements:
- stabilized reconstruction coordinate system;
- distinguish camera/global transform from local object motion;
- represent known/oracle sample positions;
- support "unknown" rather than invent a phase;
- no `fract(mean MV)` shortcut.

### 3.6 Temporal reconstruction
Implement a deterministic baseline accumulator/reconstructor that consumes independently informative aligned observations and confidence.

It must:
- preserve a single-frame control;
- permit exact ablation of each neighbor;
- avoid treating artificial rephasing as new information;
- produce a reconstructable target independent of FSR.

The goal here is a clean scientific baseline, not maximum quality.

### 3.7 Backend abstraction
The successor must run with a non-FSR backend.

Optional comparison adapters may include:
- spatial reconstruction/upscaling;
- retained historical FSR path;
- future lightweight learned backend.

FSR remains an adapter/control, never a dependency of the core temporal representation.

## Phase 4 — Integration hardening

Parallel workers attack:
- lifetime/synchronization;
- frame-window scheduling;
- seek/reset/cut behavior;
- hardware/software decode transitions;
- missing codec metadata;
- resolution/format changes;
- VFR;
- color/HDR metadata;
- resource pressure;
- deterministic replay;
- error/fallback paths;
- clean-clone build;
- old player regressions caused by shared infrastructure.

Fix integration defects before broad quality experimentation.

## Phase 5 — Build-Ready-for-Research qualification

Use `BUILD_READY_CONTRACT.md`.

This phase is NOT the final ANVIL quality campaign.

It verifies that:
- the system is complete enough to test;
- every major claim can be isolated;
- artifacts are trustworthy;
- controls/oracles exist;
- later experiments will not be confounded by missing observability.

Only after every mandatory condition passes may the orchestrator declare:
`BUILD_READY_FOR_RESEARCH = TRUE`

## Phase 6 — Stop

Do not automatically launch the full reconstruction-quality campaign.

Produce:
- exact final SHA;
- build/test result;
- architecture map;
- known limitations;
- unsupported codec paths;
- instrumentation inventory;
- example offline-run command;
- list of hypotheses ready for the later campaign;
- recommended first experiment ordering.

The maintainer will start the rigorous research campaign separately.
