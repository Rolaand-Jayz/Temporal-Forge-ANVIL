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
  Reconstruct.hpp/.cpp      deterministic block-SAD estimator (always labeled
                            image_estimate) + confidence-weighted aligned
                            average accumulator
  Runner.hpp/.cpp           pipeline integration: decode -> window ->
                            correspondence -> visibility -> geometry ->
                            color -> accumulate -> output
  (reused, Qt-free): ../media/Demuxer, ../media/VideoDecoder, ../util/Log
```

Isolation: `temporal_forge_lib` (FSR-era player) excludes `src/anvil/` by
CMake filter; `anvil_lib` contains no Qt, Vulkan, or FSR code.

## Pipeline stages (each: selectable, bypassable, timed, dumpable)

decode → window_select → correspondence → visibility → sample_geometry →
color_convert → accumulate → output

## Run manifest

`manifest.json` (deterministic serialization; stage timings are wall-clock
and excluded from replay equality):

- `config` — every runner parameter, including `deterministic_no_random_components: true`
- `ground_truth[]` — HR ground-truth attachments: frame association, SHA-256,
  size, width/height, maxval, format, usage note (reference evidence only)
- `output_files[]` — final artifact inventory (paths relative to the output dir)
- `provenance` — git SHA/dirty (null when unavailable), FFmpeg version,
  build type, compiler, input SHA-256 + size, decode mode
- `codec_capabilities` — measured H.264/HEVC/AV1 matrix
- `stage_timings[]`, `events[]` (scene_cut, window_reset, fallback,
  side_info_unsupported, side_info_ambiguous, color_unknown_metadata,
  oracle_used), `dump_files[]`, `frames[]` (per-frame side-info state,
  correspondence source, geometry state, color conversion, sample counts)

## CLI

```
anvil_runner --input FILE --output-dir DIR
    [--start-frame N] [--frame-count N] [--past N] [--future N]
    [--correspondence codec|estimate|oracle|none]
    [--visibility valid|oracle] [--geometry unknown|oracle]
    [--no-accumulate] [--no-color-convert]
    [--cut-frames 3,17] [--auto-scene-cut] [--auto-cut-threshold X]
    [--oracle-dir DIR] [--dump-dir DIR] [--dump-stages all|stage,list]
    [--ground-truth F=PATH ...] [--seed N]
```

`--ground-truth` attaches HR reference truth to a target frame (repeatable).
It is validated (PNM header, dimensions vs the decoded target) and recorded
with provenance in the manifest, but its pixels are never read into
reconstruction — attaching it cannot change output frames (proven by test).

With `--dump-stages all`, the captured artifact set per target frame is:
`decode_f<N>_y.pgm` (source planes), `decode_f<N>_mvs.txt` (raw codec MV
entries as delivered, or explicit `state=none`), `window_select_f<N>_window.txt`
(selection + exclusions), `correspondence_f<N>_correspondence.txt` (actual
block/vector values with provenance), `visibility_f<N>_i<I>.pgm` (actual
masks; `visibility_f<N>_none.txt` for the single-frame control),
`sample_geometry_f<N>_geometry.txt`, `color_convert_f<N>_color.txt` (metadata
+ working-space decision), `accumulate_f<N>_y.pgm` (reconstructed data).
Per-pixel confidence capture is N/A (a fixed scalar exists; no per-pixel
estimator is implemented); there is no distinct backend-input stage. All
inventory paths are recorded relative to their base directory for
deterministic replay.

Example (deterministic offline run):

```
anvil_runner --input clip.mp4 --output-dir out --start-frame 5 \
  --frame-count 4 --past 2 --future 2 --correspondence estimate
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
- `tests/test_anvil_contract.py` — 15 python contract tests over the CLI
