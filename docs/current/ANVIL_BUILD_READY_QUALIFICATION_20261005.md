# ANVIL BUILD_READY_FOR_RESEARCH Qualification Record

**Date:** 2026-10-06
**Branch:** `successor/anvil-build-ready` (see final SHA in the goal handoff)
**Contract evaluated:** `zcode_packs/temporal_forge_anvil_zcode_pack_2026-10-05/BUILD_READY_CONTRACT.md`
**Evidence vocabulary:** per `docs/closure/EVALUATION_STANDARD.md`

Every mandatory contract item is recorded below as PASS or N/A with concrete
evidence. No item is BLOCKED. This record is the audit trail for the
`BUILD_READY_FOR_RESEARCH = TRUE` claim; it does not launch the later
adversarial campaign.

## Mandatory architecture conditions

| Item | Verdict | Evidence |
|---|---|---|
| Successor path active and isolated from FSR | PASS | `src/anvil/` namespace `anvil`; `anvil_lib`/`anvil_runner` targets exclude FSR code by CMake source filter (`CMakeLists.txt`, "ANVIL successor isolation"); no FSR header included from `src/anvil/`; FSR-era player untouched (baseline tests unchanged). PROJECT DECISION recorded in the run ledger. |
| Runs without proprietary FSR assets | PASS | `anvil_lib` links only FFmpeg/Threads; `anvil_runner` runs end-to-end in clean clone with no FSR assets present (`tests/anvil_runner_tests.cpp`, CI clean-clone verification 2026-10-06). |
| FSR optional, not a core dependency | PASS | Successor target graph contains no FSR sources; FSR-era player builds independently in the same tree (CTest 26/26). |
| Past/future temporal window configurable | PASS | `--past/--future`; `WindowConfig::windowFor` (`src/anvil/Core.cpp`); `tests/anvil_core_tests.cpp` window tests; `test_single_frame_control_and_window_selection`. |
| Single-frame control exists | PASS | `--past 0 --future 0` produces zero neighbor samples (manifest `frames[].total_samples == 0`, python test). |
| Stages independently selectable/bypassable | PASS | `--no-accumulate`, `--no-color-convert`, correspondence/visibility/geometry mode selection incl. `none`; `test_stage_bypass`. |
| Missing side info explicit, never fabricated | PASS | `SideInfoState` (available/unsupported/ambiguous/estimator_only) recorded per frame in manifest; `side_info_unsupported` events; `test_oracle_injection_replaces_estimates`, codec probe notes. |
| Backend-neutral confidence/visibility | PASS | `Confidence` C(x)∈[0,1] with known/unknown state; `Visibility` separate enum (invalid/valid/unknown) — never conflated (`src/anvil/Core.hpp`); visibility oracle injection tested. |
| Sample geometry known/estimated/unknown, no fake jitter | PASS | `SampleGeometryState`; no phase synthesis anywhere in `src/anvil/` (code-searched); oracle geometry injection tested (`geometry_state == "known"`). |
| Temporal reconstruction outputs via non-FSR path | PASS | Deterministic confidence-weighted aligned average (`src/anvil/Reconstruct.cpp`) writing PPM/PGM through `Pnm` writer; exercised by runner tests. |

## Mandatory testability conditions

| Item | Verdict | Evidence |
|---|---|---|
| Headless/offline deterministic runner | PASS | `anvil_runner` CLI; no window/GPU/network; all runner tests run headless in CI containers. |
| Exact input frame indices/timestamps selectable | PASS | `--start-frame/--frame-count`; manifest `frames[].frame_index` and `pts_us` recorded per frame. |
| Exact configuration serialized in run manifest | PASS | `manifest.json` `config` section (all CLI parameters); `test_manifest_schema_and_provenance`. |
| Git/build/input provenance captured | PASS | `provenance.git_sha/git_dirty/ffmpeg_version/build_type/compiler`; input SHA-256 verified equal to independent `hashlib` digest (`test_manifest_schema_and_provenance`); FIPS 180-4 known-answer tests guard the hash implementation. |
| Per-stage timing hooks | PASS | `stage_timings[]` per StageId in manifest. |
| Intermediate numeric/image dumps per consequential stage | PASS | `--dump-dir/--dump-stages all`; per-stage artifact set asserted by test (`test_comprehensive_stage_capture_inventory`, `anvil_runner_tests` scenario 7e): decode = source Y plane + raw codec MV entries as delivered (or explicit `state=none`); window_select = selection + exclusions with reasons; correspondence = actual per-neighbor block values (geometry, mv, ref, ambiguous flag, precision, provenance); visibility = actual per-neighbor masks (0/128/255) or explicit `no_neighbors` state; sample_geometry = actual state/phase; color_convert = full recorded metadata + working-space decision; accumulate = reconstructed Y plane; output = final artifacts inventoried in manifest `output_files[]`. `dump_files[]` is normalized to dump-dir-relative paths and asserted equal to disk contents; capture replay is byte-identical. **Recorded N/A sub-items, not fabricated:** per-pixel confidence (a fixed scalar with known state exists; no per-pixel confidence estimator is implemented in the minimal successor, so there are no per-pixel confidence values to capture) and a distinct backend input (there is no separate backend stage; the accumulate artifact is the output writer's input). |
| Oracle correspondence replaces estimates | PASS | `--correspondence oracle`; manifest records `oracle_used` events and `correspondence_source == "oracle"`. |
| Oracle visibility replaces estimates | PASS | `--visibility oracle` (PGM masks; 0/128/255 states). |
| Oracle sample geometry/phase replaces estimates | PASS | `--geometry oracle` (`geometry_1.txt`; `geometry_state == "known"`). |
| HR ground truth attachable to synthetic fixtures | PASS | Explicit mechanism: repeatable `--ground-truth FRAME=PATH` (`src/anvil/GroundTruth.hpp/.cpp`). Each attachment is validated against the decoded target frame (P5/P6 PNM header, dimension match) and recorded in manifest `ground_truth[]` with frame association, SHA-256, size, width/height, maxval, format, and the fixed usage note "reference evidence only; excluded from candidate reconstruction"; `frames[].has_ground_truth` marks the association. Evidence: `test_ground_truth_attachment_and_provenance` (SHA-256 verified against an independent `hashlib` digest), `anvil_runner_tests` scenario 7d. Contamination guard: pixel data is never loaded into the pipeline; `test_ground_truth_never_contaminates_reconstruction` and runner-test scenario 7d prove byte-identical reconstruction with and without GT attached. Clean rejection (exit 1, named reason): missing, malformed, dimension mismatch, out-of-range frame, duplicate mapping — all tested. **Corrected 2026-10-06:** the earlier PASS row cited only synthetic-pattern knowledge and hypothetical later-campaign use, which is not attachment; that evidence was rejected and replaced by the mechanism above. |
| Stochastic components seeded or disabled | PASS | N/A-strength PASS: no stochastic component exists (`config.deterministic_no_random_components: true` asserted in tests); `--seed` recorded for future use. |
| Scene cut/reset observable and controllable | PASS | `--cut-frames` (forced, `window_reset` events) + `--auto-scene-cut` (deterministic luma SAD threshold, `scene_cut` events); `test_scene_cut_observable_and_controllable`. |

## Mandatory codec conditions (H.264 / HEVC / AV1)

Runtime probe (`src/anvil/CodecProbe.cpp`) measures each codec on the host
FFmpeg build: in-process synthetic encode → software decode with
`AV_CODEC_FLAG2_EXPORT_MVS` → count frames carrying
`AV_FRAME_DATA_MOTION_VECTORS`.

| Item | Verdict | Evidence (this host, ffmpeg n9.0.2, MEASURED) |
|---|---|---|
| Capability detected explicitly | PASS | Per-codec matrix in every manifest: encoder name, encoder/decoder availability, MV-export proven flag, probe frame counts, note. |
| Encoded-MV availability proven or marked unsupported | PASS | **H.264: MV export PROVEN (7/8 probe frames carry MV side data).** **HEVC: decodes fine, 0/8 frames export MVs → recorded unsupported with truthful note.** **AV1: decodes fine, 0/8 frames → recorded unsupported with truthful note.** |
| Precision/scale/partition/reference semantics normalized where available | PASS | `BlockMotion` preserves direction, block geometry (x,y,w,h), MV in source-pixel units, precision, frame type, intra/skip; codec entries carry `refFrameIndex = -1` because the exported side data does not prove reference identity (documented in `normalizeCodecMv`). |
| Ambiguous reference data cannot silently enter reconstruction | PASS | `buildFlowField` rejects entries with `ambiguous` or unproven `refFrameIndex`; codec-mode visibility marks unproven coverage invalid; unit test `test_ambiguous_blocks_never_enter_flow`. |
| Hardware/software decode separately documented | PASS | Successor forces software decode when motion metadata is requested (`VideoDecoder::setMotionMetadataRequested`, decoded log line "hardware decode disabled…"); manifest `provenance.decode_mode == "software"`. Hardware decode MV export is NOT claimed (guardrail). |
| Fallback when side info unavailable | PASS | `--correspondence estimate` (deterministic block-SAD, always labeled `image_estimate`) or `none`; graceful `EstimatorOnly` state for I-frames; `test_missing_input_graceful_error` + codec-mode runner test. |

Codec support/limitation matrix (this host — re-probed per run):

| Codec | Decode | Encode | MV side data via software decode | Successor consequence |
|---|---|---|---|---|
| H.264 | yes | libx264 | **yes (proven)** | codec-MV correspondence usable where reference identity proven by oracle pairing |
| HEVC | yes | libx265 | no (0/8) | unsupported → estimator/oracle fallback |
| AV1 | yes | libsvtav1 | no (0/8) | unsupported → estimator/oracle fallback |

## Mandatory color conditions

| Item | Verdict | Evidence |
|---|---|---|
| Range/primaries/transfer/matrix/chroma siting tracked | PASS | `ColorMeta` mirrors AVFrame fields incl. UNSPECIFIED + HDR side-data presence; manifest per-frame color fields; fixtures with full/partial/absent metadata. |
| Working-space conversion explicit | PASS | `conversionFullySpecified()` gate; explicit `sws_setColorspaceDetails` with coefficient tables matched to recorded matrix/range; conversion description recorded per frame (`matrix=bt709 range=limited` asserted in python test). |
| SDR/HDR do not silently share wrong transfer | PASS | PQ/HLG inputs: "no transfer conversion performed; planes preserved" (asserted), PGM planes output only; no tone mapping claimed. |
| Metrics/captures know their space | PASS | Every dump/output is accompanied by manifest frame records carrying the working-space decision; PGM/PPM outputs only produced for a stated conversion state. |

## Mandatory engineering verification

| Item | Verdict | Evidence |
|---|---|---|
| Clean configure/build on intended environment | PASS | Fresh configure+build on host (gcc 16.2.1, cmake 4.4.4, ninja, FFmpeg n9.0.2) 2026-10-05; clean-clone from GitHub URL 2026-10-06 (see below). |
| Existing applicable tests pass | PASS | CTest 26/26 (up from baseline 23; the 3 new ANVIL targets included); the 4 previously disabled FSR-era GPU tests remain disabled by design. |
| New unit/integration tests pass | PASS | `anvil_core_tests`, `anvil_codec_tests`, `anvil_runner_tests` (C++), `tests/test_anvil_contract.py` (11 tests). |
| Vulkan validation on exercised successor paths | N/A | **Justification:** the build-ready successor path exercises no Vulkan code — `anvil_lib`/`anvil_runner` do not link or call Vulkan, and no existing Vulkan path was modified (diff scope: `src/anvil/`, `tools/anvil/`, tests, CMake, CI). The historical player's Vulkan paths are unchanged and its tests remain green. Any future Vulkan successor backend re-opens this item with validation-layer evidence. |
| No known resource lifetime/synchronization defect | PASS | CPU-only pipeline; ownership reviewed: AVPacket freed in probe (`CodecProbe.cpp`), decoder flush on EOF (delayed-frame drain), decoder/demuxer RAII via reused classes, no cross-thread sharing. |
| Graceful fallback/error tested | PASS | Missing input → exit 1 with message; missing oracle → explicit error; absent encoders → capability notes; unknown color metadata → recorded unknown state. |
| Deterministic replay reproduces artifacts | PASS | Byte-equal frame outputs across identical runs (`test_deterministic_replay_byte_equality` + C++ replay checks); manifests equal excluding wall-clock timings (documented exclusion). |
| Clean-clone instructions sufficient | PASS | Instructions in `docs/current/ANVIL_SUCCESSOR_ARCHITECTURE.md`; verified by cloning `successor/anvil-build-ready` from GitHub and building/testing fresh (CTEST green, runner tests pass), 2026-10-06. |

## Command evidence

Host run at the original qualification head (2026-10-06, pre-repair):

```
ctest --test-dir build                 # 100% tests passed, 26 tests
ANVIL_RUNNER=build/anvil_runner python3 -m pytest -q tests/test_anvil_contract.py
                                       # 11 passed (pre-repair suite)
python3 -m pytest -q <historical suite files>   # 68 passed, 39 subtests
```

Host run at the repaired head `7c755712` (2026-10-06, current evidence):

```
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release   # clean configure
cmake --build build --parallel
ctest --test-dir build                 # 100% tests passed, 26 tests
ANVIL_RUNNER=build/anvil_runner python3 -m pytest -q tests/test_anvil_contract.py
                                       # 15 passed (adds ground-truth and
                                       # comprehensive-capture coverage)
python3 -m pytest -q <historical suite files>   # 68 passed, 39 subtests
# Clean-clone from GitHub at 7c755712: build OK, 26/26 CTest, 15/15 ANVIL
# pytest, 68 historical pytest.
# GitHub CI run 37419954759 on PR #1 head 7c755712: both jobs green.
```

## Reused historical infrastructure and justification

- `src/media/Demuxer.cpp` / `src/media/VideoDecoder.cpp`: Qt-free FFmpeg
  decode shell with correct PTS handling, color-metadata propagation,
  b-frame flagging, and `AV_FRAME_DATA_MOTION_VECTORS` extraction. Reuse is
  justified because the successor independently needs exactly this decode
  correctness; no FSR semantics are attached to these classes.
- `src/util/Log.cpp`: minimal leveled logger (Qt-free).
- Test conventions: standalone CTest executables, ffmpeg-CLI fixture
  generation with `SKIP_RETURN_CODE 77` deterministic skip.

## FSR-era assumptions deliberately NOT inherited

- FSR jitter/sample-phase semantics (`fract(mean(mv))` is not implemented or
  believed); sample geometry has an explicit unknown state.
- Renderer-style depth inputs.
- Causal-only history: the successor window includes future frames by design.
- Fixed scale ratios / FSR4 specific input sizes.
- FSR postpass/composition and reactive/transparency mask semantics.
- Motion-texture synthesis and FSR-specific confidence mappings.
- FSR4 model assets, weights, tensor maps, and backend policy.

## Unresolved engineering risks (for the later campaign, not blocking)

1. Codec-MV reference-frame identity is unprovable from exported side data
   alone; codec MVs currently cannot enter reconstruction unless an oracle
   pairing proves the reference. HYPOTHESIS for the campaign: pairing probes
   or decoder-internal reference tracking could change this.
2. The deterministic block-SAD estimator is integer-precision, 16×16, small
   radius — a deliberately weak baseline so the campaign can measure prior
   value honestly.
3. Accumulation confidence is fixed at 1.0 for valid samples (visibility
   only); confidence estimation is deferred by design.
4. `swscale` chroma siting convention may differ from the stream's recorded
   chroma_location; the assumption is recorded per frame for audit.
5. Long-input memory: the runner holds the decode window in RAM; frame-exact
   streaming is deferred until the campaign needs long clips.
