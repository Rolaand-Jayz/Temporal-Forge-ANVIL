# ANVIL BUILD_READY_FOR_RESEARCH Qualification Record

**Date:** 2026-10-07
**Branch:** `successor/anvil-build-ready`
**Implementation evidence head before documentation refresh:** `0e03af613bbaa904d8497857989ab1c88b9381ad`
**Contract evaluated:** `zcode_packs/temporal_forge_anvil_zcode_pack_2026-10-05/BUILD_READY_CONTRACT.md`
**Evidence vocabulary:** per `docs/closure/EVALUATION_STANDARD.md`

Every mandatory contract item is recorded below with concrete implementation
evidence. The current repair head is a **candidate** for
`BUILD_READY_FOR_RESEARCH = TRUE`; the independent PR evaluator re-review is
still pending and remains authoritative for closing the open review threads.
This record does not launch the later adversarial campaign.

## Mandatory architecture conditions

| Item | Verdict | Evidence |
|---|---|---|
| Successor path active and isolated from FSR | PASS | `src/anvil/` namespace `anvil`; `anvil_lib`/`anvil_runner` targets exclude FSR code by CMake source filter (`CMakeLists.txt`, "ANVIL successor isolation"); no FSR header included from `src/anvil/`; FSR-era player untouched (baseline tests unchanged). PROJECT DECISION recorded in the run ledger. |
| Runs without proprietary FSR assets | PASS | `anvil_lib` links only FFmpeg/Threads; `anvil_runner` runs end-to-end in clean clone with no FSR assets present (`tests/anvil_runner_tests.cpp`, CI clean-clone verification 2026-10-06). |
| FSR optional, not a core dependency | PASS | Successor target graph contains no FSR sources; FSR-era player builds independently in the same tree (CTest 26/26). |
| Past/future temporal window configurable | PASS | `--past/--future`; `WindowConfig::windowFor` (`src/anvil/Core.cpp`); `tests/anvil_core_tests.cpp` window tests; `test_single_frame_control_and_window_selection`. |
| Single-frame control exists | PASS | `--past 0 --future 0` produces zero neighbor samples (manifest `frames[].total_samples == 0`, python test). |
| Stages independently selectable/bypassable | PASS | `--refinement none|local`, `--confidence unit|estimate|oracle`, `--no-accumulate`, `--no-color-convert`, correspondence/visibility/geometry selection; unsupported geometry estimation is rejected rather than exposed as a false arm. Stage timing/dumps cover refinement and confidence. |
| Missing side info explicit, never fabricated | PASS | `SideInfoState` (available/unsupported/ambiguous/estimator_only) recorded per frame in manifest; `side_info_unsupported` events; `test_oracle_injection_replaces_estimates`, codec probe notes. |
| Backend-neutral confidence/visibility | PASS | Per-pixel `ConfidenceField` C(x)∈[0,1] supports unit, deterministic photometric estimate, and per-neighbor oracle modes; `Visibility` remains a separate invalid/valid/unknown signal. `accumulate()` consumes confidence as the sample weight and excludes every visibility state except `Valid`; `test_confidence_estimate_oracle_and_consumption` and strict visibility-oracle regressions cover the separation. |
| Sample geometry known/estimated/unknown, no fake jitter | PASS | `SampleGeometryState` preserves Known/Estimated/Unknown representation. The current runner exposes `unknown|oracle`; `--geometry estimate` is deliberately rejected until a real estimator exists, preventing a false experimental arm. Oracle relative phase is operationally applied to proven flow and changes reconstruction (`test_geometry_oracle_changes_sampling_and_is_provenanced`). |
| Temporal reconstruction outputs via non-FSR path | PASS | Deterministic confidence-weighted aligned average (`src/anvil/Reconstruct.cpp`) consumes flow, visibility and confidence, preserves single-frame control, and supports exact target/reference ablation with `--exclude-neighbor TARGET:REFERENCE`; exercised by C++ and Python runner tests. |

## Mandatory testability conditions

| Item | Verdict | Evidence |
|---|---|---|
| Headless/offline deterministic runner | PASS | `anvil_runner` CLI; no window/GPU/network; all runner tests run headless in CI containers. |
| Exact input frame indices/timestamps selectable | PASS | `--start-frame/--frame-count` plus exact `--start-pts-us` selection (no nearest fallback; no-match is an error; mutually exclusive with explicit start-frame); VFR regression `test_exact_pts_selection_on_vfr`; manifest records frame indices and `pts_us`. |
| Exact configuration serialized in run manifest | PASS | `manifest.json` `config` section (all CLI parameters); `test_manifest_schema_and_provenance`. |
| Git/build/input provenance captured | PASS | `provenance.git_sha/git_dirty/ffmpeg_version/build_type/compiler`; input SHA-256 verified equal to independent `hashlib` digest (`test_manifest_schema_and_provenance`); FIPS 180-4 known-answer tests guard the hash implementation. |
| Per-stage timing hooks | PASS | `stage_timings[]` covers decode, window_select, correspondence, correspondence_refinement, visibility, confidence, sample_geometry, color_convert, accumulate and output; `test_stage_timing_attribution` asserts exercised stages have meaningful timings. |
| Intermediate numeric/image dumps per consequential stage | PASS | `--dump-dir/--dump-stages all`; `test_comprehensive_stage_capture_inventory` asserts decode source/MV evidence, window selection + exclusions, coarse correspondence, refined correspondence, per-neighbor visibility masks, per-neighbor confidence fields, sample geometry, color decision, accumulated reconstruction and final outputs. `dump_files[]` is normalized to dump-dir-relative paths and asserted equal to disk contents; replay is byte-identical. A distinct backend-input stage is still not present; the accumulate artifact is the output writer's input. |
| Oracle correspondence replaces estimates | PASS | `--correspondence oracle`; manifest records `oracle_used` events and `correspondence_source == "oracle"`. |
| Oracle visibility replaces estimates | PASS | `--visibility oracle` uses strict per-target/per-reference PGM masks (exact 0/128/255 vocabulary), then gates them by proven correspondence coverage so visibility cannot manufacture motion evidence. |
| Oracle confidence replaces estimates | PASS | `--confidence oracle` loads strict per-target/per-reference confidence PGM fields; consumed artifacts are SHA-256-provenanced and `accumulate()` uses the values as weights. |
| Oracle sample geometry/phase replaces estimates | PASS | `--geometry oracle` uses strict finite three-field records, hashes every consumed target/reference geometry artifact, applies relative phase to proven flow, and records `geometry_state == "known"`. |
| HR ground truth attachable to synthetic fixtures | PASS | Repeatable `--ground-truth FRAME=PATH` accepts same-resolution reference truth or genuinely higher-resolution truth. References smaller than the LR observation in either axis are rejected. Manifest `ground_truth[]` records frame association, SHA-256, size, truth dimensions/maxval/bytes-per-sample, observation dimensions, scale_x/scale_y, resolution relation, format, and the fixed reference-only usage note; `frames[].has_ground_truth` marks association. Evidence: `test_ground_truth_attachment_and_provenance` (SHA-256 verified against an independent `hashlib` digest), `anvil_runner_tests` scenario 7d. Contamination guard: pixel data is never loaded into the pipeline; `test_ground_truth_never_contaminates_reconstruction` and runner-test scenario 7d prove byte-identical reconstruction with and without GT attached. Clean rejection (exit 1, named reason): missing, malformed, dimension mismatch, out-of-range frame, duplicate mapping — all tested. **Corrected 2026-10-06:** the earlier PASS row cited only synthetic-pattern knowledge and hypothetical later-campaign use, which is not attachment; that evidence was rejected and replaced by the mechanism above. |
| Stochastic components seeded or disabled | PASS | N/A-strength PASS: no stochastic component exists (`config.deterministic_no_random_components: true` asserted in tests); `--seed` recorded for future use. |
| Scene cut/reset observable and controllable | PASS | `--cut-frames` (forced) + `--auto-scene-cut` (deterministic luma SAD threshold). Repair 63580a7a: detected boundaries are canonicalized in a pre-pass BEFORE windows are built and share one exclusion rule with forced cuts, in both temporal directions — reconstruction cannot sample across a detected cut. `test_auto_scene_cut_blocks_cross_cut_accumulation` asserts the detected cut frame, excluded neighbor IDs per side, window contents, and no cross-scene neighbors; `test_scene_cut_observable_and_controllable` covers forced cuts. |

## Mandatory codec conditions (H.264 / HEVC / AV1)

Runtime probe (`src/anvil/CodecProbe.cpp`) measures each codec on the host
FFmpeg build: in-process synthetic encode → software decode with
`AV_CODEC_FLAG2_EXPORT_MVS` → count frames carrying
`AV_FRAME_DATA_MOTION_VECTORS`.

| Item | Verdict | Evidence (this host, ffmpeg n9.0.2, MEASURED) |
|---|---|---|
| Capability detected explicitly | PASS | Per-codec matrix in every manifest: encoder name, encoder/decoder availability, MV-export proven flag, probe frame counts, note. |
| Encoded-MV availability proven or marked unsupported | PASS | **H.264: MV export PROVEN (7/8 probe frames carry MV side data).** **HEVC: decodes fine, 0/8 frames export MVs → recorded unsupported with truthful note.** **AV1: decodes fine, 0/8 frames → recorded unsupported with truthful note.** Repair 63580a7a: per-frame manifest state is derived from the ACTUAL input codec (no cross-codec leakage): H.264 I-frames → `estimator_only`; H.264 P-frames with exported-but-unproven references → `ambiguous` with `codec_mv_usable_count=0` (matching the reconstruction behavior that rejects them); HEVC/AV1 inputs → `unsupported` even when the host also proves H.264. Regressions: `test_codec_side_info_state_is_per_codec_and_truthful` (H.264 I/P + HEVC). |
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
| SDR/HDR do not silently share wrong transfer | PASS | PQ/HLG inputs receive no implicit transfer conversion or tone mapping. Planar 4:2:0 temporal reconstruction is component-depth aware for 8/10/12/16-bit samples; PGM output records the actual numerical maxval and emits all Y/U/V planes. `test_hdr_10bit_temporal_reconstruction_is_sample_depth_safe` independently decodes yuv420p10le source frames and checks the temporal Y/U/V result sample-for-sample. |
| Metrics/captures know their space | PASS | Every dump/output is accompanied by manifest frame records carrying the working-space decision; PGM/PPM outputs only produced for a stated conversion state. |

## Mandatory engineering verification

| Item | Verdict | Evidence |
|---|---|---|
| Clean configure/build on intended environment | PASS | Fresh configure+build on host (gcc 16.2.1, cmake 4.4.4, ninja, FFmpeg n9.0.2) 2026-10-05; clean-clone from GitHub URL 2026-10-06 (see below). |
| Existing applicable tests pass | PASS | CTest 26/26 (up from baseline 23; the 3 new ANVIL targets included); the 4 previously disabled FSR-era GPU tests remain disabled by design. |
| New unit/integration tests pass | PASS | `anvil_core_tests`, `anvil_codec_tests`, `anvil_runner_tests` (C++), plus `tests/test_anvil_contract.py` (**46 tests** at implementation evidence head `0e03af61`). |
| Vulkan validation on exercised successor paths | N/A | **Justification:** the build-ready successor path exercises no Vulkan code — `anvil_lib`/`anvil_runner` do not link or call Vulkan, and no existing Vulkan path was modified (diff scope: `src/anvil/`, `tools/anvil/`, tests, CMake, CI). The historical player's Vulkan paths are unchanged and its tests remain green. Any future Vulkan successor backend re-opens this item with validation-layer evidence. |
| No known resource lifetime/synchronization defect | PASS | CPU-only pipeline; ownership reviewed: AVPacket freed in probe (`CodecProbe.cpp`), decoder flush on EOF (delayed-frame drain), decoder/demuxer RAII via reused classes, no cross-thread sharing. |
| Graceful fallback/error tested | PASS | Missing input → exit 1 with message; missing oracle → explicit error; absent encoders → capability notes; unknown color metadata → recorded unknown state. |
| Deterministic replay reproduces artifacts | PASS | Byte-equal frame outputs across identical runs (`test_deterministic_replay_byte_equality` + C++ replay checks); manifests equal excluding wall-clock timings (documented exclusion). |
| Clean-clone instructions sufficient | PASS | Instructions in `docs/current/ANVIL_SUCCESSOR_ARCHITECTURE.md`; verified by cloning `successor/anvil-build-ready` from GitHub and building/testing fresh (CTEST green, runner tests pass), 2026-10-06. |

## Repair addendum — independent review of PR #1 (2026-10-06)

All seven review findings (4202350954/957/960/964/968/973/977) were repaired
in commit `63580a7a` with the regression tests named above and in the PR
thread replies; CI run `37566726205` (Arch build + CTest, Python contract
suite) is green on that head. Additional repair-phase evidence:

- Structured per-frame color metadata (`color_source`/`color_output` with
  range, primaries, transfer, matrix, chroma_location, pixel_format,
  bit_depth, HDR side-data presence; explicit `unspecified` preserved) is
  now in every manifest without debug dumps —
  `test_per_frame_structured_color_metadata` (SDR, PQ, missing-tag, genuine
  HDR10 side data).
- HDR side-data presence flows AVFrame → `DecodedVideoFrame` → `Observation`
  → manifest; verified with a genuine x265 HDR10 SEI fixture. On this FFmpeg
  build the h264 decoder does not export mastering-display frame side data,
  so such inputs truthfully report absent — recorded, not fabricated.
- Stage timings: `visibility` is recorded independently; the actual
  `sws_scale` YUV→RGB transform is attributed to `color_convert`; output
  covers serialization only; stage names serialize as strings (a
  `const char*`→`bool` JsonWriter overload corruption is fixed) —
  `test_stage_timing_attribution`.
- Partial-edge-tile estimation with a coverage mask (66x65 and 1920x1080
  regressions; measured zero motion is valid, uncovered pixels are
  Visibility::Invalid); correspondence-mode `none` truthfully contributes
  nothing — the 10-bit depth-safety test was corrected to this
  contract-correct passthrough semantics and now also proves estimate-mode
  full coverage.
- Git provenance accepts any 40-hex SHA and equals `git rev-parse HEAD`
  (`test_git_sha_matches_rev_parse`, `testGitShaAcceptsAnyHex`).

Test counts at `63580a7a`: CTest 26/26; ANVIL python suite 22 passed;
historical python suite 68 passed, 39 subtests.

## Repair addendum — round 2 review (2026-10-07)

Six further P1 findings (4202852907/4085/4270/4436/4571/5070) were repaired
in commits `098b5225` (repairs + regressions), `5615f68e`/`fb9a7bf8` (VFR
test portability). CI runs `37572890396` (first push; one test portability
failure found and fixed), `37573479829`, `37573917283`, and `37574437634`
(final head `fb9a7bf8`: Arch build + CTest green, Python contract suite
green).

- Exact timestamp selection: `--start-pts-us` (exact match, no nearest
  fallback, hard error on no-match, ambiguity guard for duplicate PTS,
  mutually exclusive with `--start-frame`); manifest records
  `config.start_pts_us`. VFR regression selects from actual container PTS
  values on an uneven 10+25 fps stream (`test_exact_pts_selection_on_vfr`).
  Duplicate decoded PTS is not representable in a valid mp4/h264 stream
  (the muxer monotonizes); the ambiguity guard remains defensive and is
  documented as such.
- Oracle correspondence is validated before any narrowing: wide integer
  parsing, finite + float-representable motion, positive extents,
  representable coordinates, direction/distance sign consistency,
  ref >= -1, and runner-side frame-bounds checks; malformed fixtures fail
  with file+line reasons (`test_malformed_oracle_rejection_matrix`,
  C++ `testOracleCorrespondenceRejection`).
- Mandatory evidence writes propagate failure (output dir, manifest.json,
  every requested dump, output frames) with operation+path diagnostics;
  regressions block each artifact class deterministically
  (`test_mandatory_write_failure_propagation`).
- Strict CLI configuration parsing (from_chars, whole-token, no
  exceptions; validated mode vocabularies; strict `--cut-frames`;
  16-case malformed matrix asserting exit 2 and no experiment output —
  `test_strict_cli_configuration_matrix`).
- Visibility oracles are keyed per target AND reference
  (`visibility_<t>_ref<r>.pgm`); masks are never reused across neighbors;
  two-neighbor independence regression shows accumulation counts respond
  only to the changed neighbor's mask
  (`test_two_neighbor_visibility_oracle_independence`).
- Codec precision derives from the transported `AVMotionVector::motion_scale`
  (1=integer, 2=half, 4=quarter, 8/16/32=sub-quarter, else Unknown); decode
  dumps record the raw scale; h264 (scale 4) and mpeg2 (scale 2) fixture
  regressions plus a unit matrix
  (`test_codec_mv_precision_reflects_motion_scale`,
  `testMotionPrecisionFromScale`).

Test counts at `fb9a7bf8`: CTest 26/26; ANVIL python suite 28 passed;
historical python suite 68 passed, 39 subtests.

## Repair addendum — round 3 evaluator findings (implementation-side evidence, 2026-10-07)

The independent evaluator's head `19ccb55a5da48e1e6a42f4099ea11847e684c61a`
left two partially repaired findings and nine new findings. The implementation
branch advanced **28 commits** from that reviewed head through
`0e03af613bbaa904d8497857989ab1c88b9381ad`. These are repair claims for the next evaluator pass, not a
self-approval; the corresponding GitHub threads intentionally remain open.

- Oracle correspondence now range-checks `temporalDistance` before narrowing
  and requires a known `refFrameIndex` to agree with target, direction and
  temporal distance. Malformed/self/contradictory oracle records fail closed.
- CLI destination narrowing and derived frame/window arithmetic are checked;
  oversized `past/future`, timestamps, seeds and overflowing window bounds
  return configuration error 2 with no experiment output.
- Oracle visibility is intersected with correspondence coverage, so a valid
  mask cannot resurrect placeholder zero-flow pixels.
- `--geometry estimate` is rejected because no estimator is implemented;
  the state model still preserves Estimated for future implementations.
- A distinct `correspondence_refinement` stage now exists with deterministic
  local residual search, mode control, timing, dumps and regression coverage.
- Confidence is now operational: unit, deterministic estimated and oracle
  per-pixel fields are supported, dumped/provenanced, and consumed as
  accumulation weights.
- Exact selected-neighbor ablation is exposed as repeatable
  `--exclude-neighbor TARGET:REFERENCE`, validated fail-closed, serialized in
  the manifest and visible in window dumps.
- Every consumed correspondence/visibility/confidence/sample-geometry oracle
  artifact is SHA-256/size/type/target/reference provenanced; provenance
  mutation and repeated-consumption roles are regression-tested.
- Known-but-unsupported color matrices fail the RGB conversion gate instead of
  falling through to `SWS_CS_DEFAULT`.
- Visibility oracle parsing accepts only 0/128/255 and `Unknown` is excluded
  from accumulation because only `Visibility::Valid` is consumable.
- Sample-geometry oracle parsing is strict: exactly three fields, finite phase,
  state bounds and phase range [0,1); trailing data is rejected.

Current clean GitHub Actions evidence on `0e03af613bbaa904d8497857989ab1c88b9381ad`: run
`37582895384` succeeded in both jobs. CTest reports **26/26 passed** and the
combined Python job reports **114 passed** = 68 historical + **46 ANVIL**
contract tests.

## Command evidence

Current implementation evidence head `0e03af613bbaa904d8497857989ab1c88b9381ad` (2026-10-07):

```
GitHub Actions run 37582895384 on PR #1:
  Arch build + CTest     SUCCESS
    - clean checkout
    - configure/build    SUCCESS
    - CTest              26/26 passed
  Python contract suite  SUCCESS
    - historical suite   68 passed
    - ANVIL suite        46 passed
    - combined total     114 passed

Source-level inventory at this head:
  - tests/test_anvil_contract.py: 46 test functions
  - deterministic refinement/confidence/ablation controls present
  - strict oracle parsing + SHA-256 provenance for consumed oracle inputs
  - exact PTS selection, scene-cut reset, color/HDR evidence and
    high-bit-depth temporal reconstruction regressions present
```

The earlier clean-clone verification remains historical evidence for the clone
and build instructions. GitHub Actions run `37582895384` supplies a fresh
clean-checkout build/test result for the implementation evidence head above.
The documentation refresh commit that follows this evidence head changes only
the two current ANVIL documentation files and must itself remain green before
evaluator handoff.

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

1. Codec-MV reference-frame identity remains unprovable from exported FFmpeg
   side data alone; ambiguous codec vectors are preserved as evidence but do
   not enter reconstruction.
2. The coarse estimator and local residual refiner are intentionally simple
   integer-search baselines. They are scientific controls, not quality claims.
3. The deterministic confidence estimator is a simple warped photometric
   residual. Its usefulness versus oracle/unit confidence is a later research
   question, not an established quality result.
4. No sample-geometry estimator is implemented. `--geometry estimate` is
   therefore rejected rather than pretending to provide an experimental arm;
   unknown and oracle geometry remain explicit.
5. `swscale` chroma siting convention may differ from the stream's recorded
   `chroma_location`; the assumption is recorded per frame for audit.
6. Long-input memory: the runner retains the bounded decoded working window in
   RAM; a streaming implementation is deferred until long-clip research needs
   it.
