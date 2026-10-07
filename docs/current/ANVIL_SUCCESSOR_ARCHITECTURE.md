# ANVIL Successor Architecture (build-ready)

**Status:** CURRENT — successor integration branch `successor/anvil-build-ready`
**Contract:** `zcode_packs/temporal_forge_anvil_zcode_pack_2026-10-05/BUILD_READY_CONTRACT.md`
**Qualification:** [`ANVIL_BUILD_READY_QUALIFICATION_20261005.md`](ANVIL_BUILD_READY_QUALIFICATION_20261005.md)

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
  Core.hpp/.cpp             Observation, BlockMotion, SideInfoState,
                            Confidence, Visibility, SampleGeometry,
                            WindowConfig (past/future, single-frame control)
  ColorMeta.hpp/.cpp        color metadata mirror of AVFrame + explicit
                            working-space conversion gate + HDR guard
  JsonWriter.hpp/.cpp       deterministic JSON serialization
  Manifest.hpp/.cpp         run manifest: config, provenance, codec matrix,
                            stage timings, event log, dump inventory
  Sha256.hpp/.cpp           SHA-256 (FIPS 180-4) for input provenance
  Pnm.hpp/.cpp              deterministic PGM/PPM captures
  Oracle.hpp/.cpp           oracle loading (correspondence txt, visibility
                            PGM, geometry txt) with strict validation
  GroundTruth.hpp/.cpp      HR ground-truth attachment: per-frame mapping,
                            PNM validation, SHA-256 provenance; reference
                            evidence only (never fed to reconstruction)
  CodecProbe.hpp/.cpp       per-codec MEASURED capability probe
                            (H.264 / HEVC / AV1 MV-export reality)
  Reconstruct.hpp/.cpp      deterministic block-SAD coarse estimator,
                            local residual correspondence refinement,
                            photometric confidence estimator, sample-geometry
                            flow adjustment + confidence-weighted accumulator
  Runner.hpp/.cpp           pipeline integration: decode -> window ->
                            correspondence -> refinement -> visibility ->
                            confidence -> geometry -> color -> accumulate ->
                            output; exact per-neighbor ablation
  (reused, Qt-free): ../media/Demuxer, ../media/VideoDecoder, ../util/Log
```

Isolation: `temporal_forge_lib` (FSR-era player) excludes `src/anvil/` by
CMake filter; `anvil_lib` contains no Qt, Vulkan, or FSR code.

## Pipeline stages (each: selectable, bypassable, timed, dumpable)

decode → window_select → correspondence → correspondence_refinement →
visibility → confidence → sample_geometry → color_convert → accumulate →
output

## Run manifest

`manifest.json` (deterministic serialization; stage timings are wall-clock
and excluded from replay equality):

- `config` — every runner parameter, including refinement/confidence modes,
  exact PTS selection, exact neighbor exclusions and
  `deterministic_no_random_components: true`
- `ground_truth[]` — HR ground-truth attachments: frame association, SHA-256,
  size, width/height, maxval, format, usage note (reference evidence only)
- `oracle_artifacts[]` — every consumed correspondence/visibility/confidence/
  sample-geometry oracle input with SHA-256, size, type, target and optional
  reference consumption role
- `output_files[]` — final artifact inventory (paths relative to the output dir)
- `provenance` — git SHA/dirty, FFmpeg version, build type, compiler,
  input SHA-256 + size, decode mode
- `codec_capabilities` — measured H.264/HEVC/AV1 matrix
- `stage_timings[]`, `events[]`, `dump_files[]`, `frames[]` — side-info
  state, correspondence source, geometry/color state and sample counts

## CLI

```
anvil_runner --input FILE --output-dir DIR
    [--start-frame N | --start-pts-us US] [--frame-count N]
    [--past N] [--future N]
    [--correspondence codec|estimate|oracle|none]
    [--refinement none|local]
    [--visibility valid|oracle]
    [--confidence unit|estimate|oracle]
    [--geometry unknown|oracle]
    [--exclude-neighbor TARGET:REFERENCE ...]
    [--no-accumulate] [--no-color-convert]
    [--cut-frames 3,17] [--auto-scene-cut] [--auto-cut-threshold X]
    [--oracle-dir DIR] [--dump-dir DIR] [--dump-stages all|stage,list]
    [--ground-truth F=PATH ...] [--seed N]
```

`--ground-truth` attaches HR reference truth to a target frame (repeatable).
It is validated as P5/P6 reference truth and recorded with provenance in the
manifest, but its pixels are never read into reconstruction. Ground truth may
match observation resolution or be genuinely higher resolution; observation
dimensions and scale_x/scale_y are recorded so controlled HR→LR fixtures are
not forced into a false same-resolution contract.

With `--dump-stages all`, the captured artifact set per target frame includes
decoded source/MV evidence, window selection and ablation exclusions, coarse
`correspondence`, `correspondence_refinement`, per-neighbor `visibility`
and `confidence` PGM fields, sample geometry, color metadata/decision,
accumulated reconstruction and final outputs. Single-frame/no-neighbor states
are explicit. All dump inventory paths are recorded relative to their base
directory for deterministic replay.

Oracle files are strict evidence inputs: correspondence records are
range/relationship validated; visibility accepts only 0/128/255; geometry
requires exactly three finite fields with phase in [0,1); every consumed
oracle artifact is hashed and recorded in `oracle_artifacts[]`. Oracle
visibility is still gated by proven correspondence coverage, so it cannot
manufacture usable motion.

`--geometry estimate` is intentionally unsupported at this gate and is
rejected as a configuration error. The representation retains the Estimated
state for a future genuine estimator.

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
(for the historical player targets), ffmpeg CLI + pytest for the full test
suites. The ANVIL targets themselves need only FFmpeg.

## Tests

- `tests/anvil_core_tests.cpp` — window/geometry/confidence/JSON/
  estimator-provenance/accumulate/ambiguous-MV-rejection/SHA-256 vectors
- `tests/anvil_codec_tests.cpp` — measured codec capability truthfulness
- `tests/anvil_runner_tests.cpp` — end-to-end CLI on generated fixtures
  (SDR full-metadata, PQ-HDR, unspecified-metadata)
- `tests/test_anvil_contract.py` — 46 python contract tests over the CLI
