# ANVIL Successor Architecture (build-ready)

**Status:** CURRENT — successor integration branch `successor/anvil-build-ready`
**Contract:** `zcode_packs/temporal_forge_anvil_zcode_pack_2026-10-05/BUILD_READY_CONTRACT.md`
**Qualification:** [`ANVIL_BUILD_READY_QUALIFICATION_20261005.md`](ANVIL_BUILD_READY_QUALIFICATION_20261005.md)  
**Implementation/evidence head:** `5f10b26f84ba89293e1b38622521acde37ee0732`  
**Exact-head CI:** Actions run `37855358002` — SUCCESS  
**Gate state:** candidate/pending independent evaluator; not self-approved

## What ANVIL is at this gate

ANVIL is the non-FSR successor of Temporal Forge: a deterministic, offline,
CPU-only temporal video-reconstruction research rig. It exists so the later
adversarial campaign can prove or falsify claims about codec motion priors,
visibility/confidence modeling, sampling-phase truth, and temporal
accumulation — with exact controls, oracles, provenance, and replay. It is
explicitly NOT a quality claim, NOT a performance claim, and NOT an FSR
replacement claim.

## Module map

```
tools/anvil/main.cpp        anvil_runner CLI (arg parsing, exit codes)
src/anvil/
  Core.hpp/.cpp             Observation (incl. native timestamp identity),
                            BlockMotion, SideInfoState, Confidence,
                            Visibility, SampleGeometry, WindowConfig
  ColorMeta.hpp/.cpp        color metadata mirror of AVFrame + explicit
                            working-space conversion gate + HDR guard
  JsonWriter.hpp/.cpp       deterministic JSON serialization
  Manifest.hpp/.cpp         run manifest: config, provenance, codec matrix,
                            stage timings, event log, dump inventory
  BuildProvenance.hpp/.cpp  weak-default source-identity variables; the
                            build regenerates strong definitions from Git
                            state on EVERY build (cmake/AnvilProvenance.cmake)
  Sha256.hpp/.cpp           SHA-256 (FIPS 180-4) for input provenance
  Pnm.hpp/.cpp              deterministic PGM/PPM captures
  Oracle.hpp/.cpp           oracle loading (correspondence txt, visibility
                            PGM, geometry txt) with strict validation
  GroundTruth.hpp/.cpp      HR ground-truth attachment: per-frame mapping,
                            PNM validation, SHA-256 provenance; reference
                            evidence only (never fed into reconstruction)
  CodecProbe.hpp/.cpp       per-codec MEASURED capability probe
                            (H.264 / HEVC / AV1 MV-export reality)
  SideInfoNormalize.hpp/.cpp  explicit side-info normalization stage: the
                            raw codec side information -> normalized prior
                            boundary, with a truthful bypass control
  Reconstruct.hpp/.cpp      deterministic block-SAD coarse estimator
                            (even-lattice prior), local residual
                            correspondence refinement (odd-lattice residual
                            precision), photometric confidence estimator,
                            sample-geometry flow adjustment +
                            confidence-weighted accumulator
  SampleGeometryEstimate.hpp/.cpp  deterministic image-evidence-only
                            sample-geometry estimator (block subpixel
                            registration, parabolic fit, median aggregate)
  OutputBackend.hpp/.cpp    replaceable output-backend boundary: "pnm"
                            (the former inline runner serialization,
                            byte-identical) and "null" (zero-artifact bypass)
  TimestampSelect.hpp       exact native-tick selection semantics
                            (match / no-match / ambiguity; never nearest)
  Runner.hpp/.cpp           pipeline integration: decode -> window ->
                            side-info normalization -> correspondence ->
                            refinement -> visibility -> sample geometry ->
                            confidence -> color -> accumulate -> output
                            backend; exact per-neighbor ablation
  (reused, Qt-free): ../media/Demuxer, ../media/VideoDecoder, ../util/Log
```

Isolation: `temporal_forge_lib` (FSR-era player) excludes `src/anvil/` by
CMake filter; `anvil_lib` contains no Qt, Vulkan, or FSR code.

## Pipeline stages (each: selectable, bypassable, timed, dumpable)

decode → window_select → side_info_normalization → correspondence →
correspondence_refinement → visibility → sample_geometry → confidence →
color_convert → accumulate → output (serialized stage name; the stage
is the output-backend boundary — see the stage semantics note below)

Execution order note (deliberate, and load-bearing): sample geometry is
applied to proven flow BEFORE confidence is estimated, so estimated
confidence measures the geometry-adjusted warp. The StageId enum encodes
this order (`SampleGeometryStage` precedes `Confidence`).

Stage semantics:

- `side_info_normalization` — raw codec side information crosses an explicit
  raw→normalized boundary before any correspondence algorithm runs
  (`--side-info normalize|bypass`; bypass refuses to interpret raw data
  while leaving the correspondence arm unchanged).
- `correspondence_refinement` — the coarse estimator searches an even
  integer lattice (step 2); the local refiner supplies the odd-lattice
  residual. Neither stage alone covers the full integer field: that split is
  what makes refinement genuinely informative rather than a re-check of
  already-searched candidates.
- `output_backend` — serialization belongs to the backend implementation
  (`--backend pnm|null`), not the runner; the null backend completes the
  pipeline and manifest with zero artifacts.

## Run manifest

`manifest.json` (deterministic serialization; stage timings are wall-clock
and excluded from replay equality):

- `config` — every runner selection (window, stage modes incl.
  `side_info_normalization_mode` and `output_backend`, exact selection via
  `start_frame` / `start_pts_us` / `start_pts_ticks`, ablations,
  `deterministic_no_random_components: true`, backend-reported
  `output_format`)
- `ground_truth[]` — HR ground-truth attachments: frame association, SHA-256,
  size, width/height, maxval, format, usage note (reference evidence only)
- `oracle_artifacts[]` — every consumed correspondence/visibility/confidence/
  sample-geometry oracle input with SHA-256, size, type, target and optional
  reference consumption role
- `output_files[]` — final artifact inventory (paths relative to the output
  dir; empty of frame artifacts under the null backend)
- `provenance` — git SHA/dirty plus `git_dirty_hash` (SHA-256 over the dirty
  state's porcelain status + tracked-content diff when dirty) and
  `provenance_source` (`build_generated` | `compile_macro` | `runtime_git` | `unknown`);
  identity is regenerated from Git state on every build, so an incremental
  rebuild after a commit or source mutation cannot present the prior clean
  revision; FFmpeg version, build type, compiler, input SHA-256 + size,
  decode mode
- `codec_capabilities` — measured H.264/HEVC/AV1 matrix
- `stage_timings[]`, `events[]`, `dump_files[]`, `frames[]` — side-info
  state + normalization state, correspondence source, geometry/color state
  and sample counts; per-frame native timestamp identity (`pts_ticks`,
  `timebase_num`/`timebase_den`, `timestamp_source` in
  {`pts`,`best_effort`,`none`} — `pts_us` remains as a convenience field
  and is lossy for timebases finer than 1/1,000,000). Aggregation note:
  a converted frame carries TWO `color_convert` timing records — the
  working-space decision (inside the per-frame color stage) and the actual
  `sws_scale` transform (attributed to `color_convert` from the output
  block) — so summing timings by stage name must expect the duplicate.

## CLI

```
anvil_runner --input FILE --output-dir DIR
    [--start-frame N | --start-pts-us US | --start-pts-ticks TICKS]
    [--frame-count N] [--past N] [--future N]
    [--side-info normalize|bypass]
    [--correspondence codec|estimate|oracle|none]
    [--refinement none|local]
    [--visibility valid|oracle]
    [--confidence unit|estimate|oracle]
    [--geometry unknown|estimate|oracle]
    [--backend pnm|null]
    [--exclude-neighbor TARGET:REFERENCE ...]
    [--no-accumulate] [--no-color-convert]
    [--cut-frames 3,17] [--auto-scene-cut] [--auto-cut-threshold X]
    [--oracle-dir DIR] [--dump-dir DIR] [--dump-stages all|stage,list]
    [--ground-truth F=PATH ...] [--seed N]
```

`--start-pts-ticks` is the lossless selection control: it addresses frames
by their native ticks in the input stream's own timebase, so distinct
fine-timebase ticks that collide after microsecond rescaling stay
individually addressable (exact match only; no nearest fallback; duplicate
ticks are ambiguous and refused). `--start-pts-us` is exact within the
microsecond domain.

`--ground-truth` attaches HR reference truth to a target frame (repeatable).
It is validated as P5/P6 reference truth and recorded with provenance in the
manifest, but its pixels are never read into reconstruction. Ground truth may
match observation resolution or be genuinely higher resolution; observation
dimensions and scale_x/scale_y are recorded so controlled HR→LR fixtures are
not forced into a false same-resolution contract.

With `--dump-stages all`, the captured artifact set per target frame includes
decoded source/MV evidence, window selection and ablation exclusions, the
side-info normalization boundary output (mode/state/counts/blocks), coarse
`correspondence`, `correspondence_refinement`, per-neighbor `visibility`
and `confidence` PGM fields, sample geometry, color metadata/decision,
accumulated reconstruction, plus final outputs in the output directory.
Single-frame/no-neighbor states are explicit. All dump inventory paths are
recorded relative to their base directory for deterministic replay.

Oracle files are strict evidence inputs: correspondence records are
range/relationship validated; visibility accepts only 0/128/255; geometry
requires exactly three finite fields with phase in [0,1); every consumed
oracle artifact is hashed and recorded in `oracle_artifacts[]`. Oracle
visibility is still gated by proven correspondence coverage, so it cannot
manufacture usable motion.

`--geometry estimate` runs the real deterministic estimator: per-block
integer SAD with parabolic subpixel fits over luma evidence only (never
codec MVs — `fract(mean MV)` is not implemented and not believed), median
aggregation with a dispersion gate, phases labeled `SampleGeometryState::
Estimated` and applied to proven flow; insufficient evidence degrades
truthfully to unknown with recorded reasons. Estimated phases are
target-relative (the target grid is the anchor). The estimator has a
documented parabolic contraction bias on smooth periodic textures (on the
order of 0.08–0.09 px at 0.25 px offsets across tested texture periods of
6–24 px; independently reproduced numerically during review); accuracy
bounds live in the estimator header and its tests. Application is coherent
by construction: the runner applies estimated phases through
applyEstimatedPhaseResidual, the round-consistent signed residual matching
the integer correspondence flow (proven for both fractional quadrants).

Planar 4:2:0 temporal reconstruction is sample-depth aware: 8-bit uses one
byte/sample while 10/12/16-bit little-endian inputs use complete two-byte
samples. Y/U/V are all reconstructed and emitted. High-bit-depth PGM captures
use the source numerical maxval (for example 1023 for 10-bit) and PNM's
required big-endian raster encoding.

Example (deterministic offline run):

```
anvil_runner --input clip.mp4 --output-dir out --start-frame 5 \
  --frame-count 4 --past 2 --future 2 --correspondence estimate \
  --refinement local --confidence estimate
# -> out/manifest.json, out/frame_5.ppm ... out/frame_8.ppm
```

## Clean-clone instructions

```
git clone https://github.com/Rolaand-Jayz/Temporal-Forge-ANVIL.git
cd Temporal-Forge-ANVIL
git checkout successor/anvil-build-ready
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel
ctest --test-dir build --output-on-failure          # C++ + runner suites
ANVIL_RUNNER=$PWD/build/anvil_runner \
  python3 -m pytest -q tests/test_anvil_contract.py  # python contract suite
```

Requirements: C++23 compiler, CMake ≥ 3.24, Ninja, FFmpeg ≥ 5.1 dev libs
(libavformat/libavcodec/libavutil/libswscale/libswresample), Qt6 + Vulkan
(for the historical player targets), ffmpeg CLI + ffprobe + pytest for the
full test suites. The ANVIL targets themselves need only FFmpeg.

## Current validation snapshot

At implementation/evidence head `5f10b26f84ba89293e1b38622521acde37ee0732`:

- Actions run `37855358002`: SUCCESS.
- CTest: **100% tests passed out of 32 non-disabled tests**. Of the 36
  registered tests, 29 completed normally, three were explicitly skipped
  (`fsr4_weight_tests`, `fsr4_tensormap_tests`,
  `anvil_provenance_rebuild_regression`), and four historical GPU tests
  remain disabled by design.
- Python CI contract job: 124 passed = 68 historical + 56 ANVIL tests.

This validates the exact implementation/evidence head. It does not replace
independent evaluator adjudication of the Build-Ready gate.

## Tests

- `tests/anvil_core_tests.cpp` — window/geometry/confidence/JSON/
  estimator-provenance/accumulate/ambiguous-MV-rejection/SHA-256 vectors
- `tests/anvil_refinement_tests.cpp` — coarse/refine resolution hierarchy:
  odd truths corrected, even/boundary optima unchanged, fail-closed inputs
- `tests/anvil_geometry_estimate_tests.cpp` — estimator accuracy against a
  known-truth subpixel fixture (truth never injected as oracle), sign proof
  via flow application, insufficient-evidence and codec-MV independence
- `tests/anvil_side_info_tests.cpp` — normalization/bypass/not-applicable
  semantics, purity, determinism, fail-closed control values
- `tests/anvil_output_backend_tests.cpp` — factory + polymorphic
  substitution, byte-identical PNM serialization, null bypass, fail-closed
- `tests/anvil_timestamp_tests.cpp` — exact tick-selection semantics +
  decode-level collision regression on a fine-timebase fixture
  (10 MHz track timescale, 0.5 µs-spaced ticks colliding after µs rescale)
- `tests/anvil_provenance_rebuild_regression.py` — configures a detached
  worktree at HEAD, mutates source/Git state, rebuilds WITHOUT reconfigure:
  the manifest must not claim the stale clean identity (dirty hash appears
  on dirty rebuilds; the SHA advances on commit)
- `tests/anvil_codec_tests.cpp` — measured codec capability truthfulness
- `tests/anvil_runner_tests.cpp` — end-to-end CLI on generated fixtures
  (SDR full-metadata, PQ-HDR, unspecified-metadata)
- `tests/test_anvil_contract.py` — 56 python contract tests over the CLI,
  including the runner-level seven-finding regressions (corrective
  refinement on an odd-motion fixture, normalization isolation, backend
  null/pnm identity, estimated-geometry non-no-op proof on a subpixel
  fixture, native tick collision selection matrix)
