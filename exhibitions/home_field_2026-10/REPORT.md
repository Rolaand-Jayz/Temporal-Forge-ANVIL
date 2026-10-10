# ANVIL Home Field Exhibition — Technical Report

**Exhibition:** `home_field_2026-10` · 2026-10-09
**Branch:** `feature/anvil-home-field-exhibition` (base `origin-anvil/main` @ `f2f8b992`)
**Assignment:** ANVIL Home Field Exhibition & Visual Review Lab (2026-10-09)
**Status:** implemented and validated; PR prepared, **not merged**

---

## 1. Implementation

- **Tooling commit:** `a88b4b39` (initial lab layer) plus follow-up fixes on this branch; the PR head is the authoritative implementation SHA.
- **New code:** `src/anvil_lab/` (image model, PNM I/O, PNG derivatives, strict JSON, bicubic/Lanczos3 scaler, PSNR/SSIM/edge/temporal metrics, abs-diff/heatmap/region analysis, catalog + naming/roster contracts, fail-closed baseline verifier, deterministic scene synthesis, sandboxed HTTP core), `tools/anvil_lab/exhibit_main.cpp` (`anvil_exhibit`), `tools/anvil_lab/server_main.cpp` (`anvil_review_lab`), `tools/anvil_lab/web/` (Visual Review Lab UI).
- **No ANVIL reconstruction code was modified.** `src/anvil/` and `tools/anvil/` are byte-identical to the merged main implementation (verified by the baseline identity below). The lab layer is FFmpeg-free; it drives `anvil_runner` as a subprocess and processes its PNM artifacts.
- **Architecture:** exhibition definitions (`EXPERIMENTS.json`) → `anvil_exhibit run` (executes arms via the runner, verifies each baseline run fail-closed, applies delivery scaling) → `measure` (full-reference metrics against representation-matched references) → `catalog build` (validated candidate records + roster linkage). The Review Lab server (`anvil_review_lab`) serves the catalog, hash-linked 8-bit display derivatives, and computes all difference math server-side from the original PNM samples.
- **New dependency:** vendored `stb_image_write.h` v1.16 (public domain / MIT, pinned commit recorded in `external/stb/STB_IMAGE_WRITE_COMMIT.txt`); recorded in `THIRD_PARTY_LICENSES.md`. No other new dependencies.
- **Remaining limitations:** single-user loopback server; UI verified in Chromium and Firefox 157 (stable, headless real render) at 2560×1440 — WebKit untested; no automatic image alignment (manual display-only offsets are provided and disclosed); synthetic scenes are analytic renderings, not natural footage.

## 2. Baseline identity (permanent)

- `BASELINE.json` pins **ANVIL baseline** to commit `f2f8b9929c1cc3845dab90bbe80d889b44c99390` (post-merge main head) with per-file SHA-256 of all 32 implementation files (`src/anvil/*`, `tools/anvil/main.cpp`), the canonical configuration (`past=2, future=2`, all stage modes at their defaults), and its hash.
- `verify-baseline` fails closed on: implementation hash mismatch (a future merged candidate cannot silently redefine the baseline), missing pinned commit, or a run whose canonical configuration hash differs from the frozen one.
- Reproduction: `git checkout f2f8b992` + the build/run command recorded inside `BASELINE.json`.
- The baseline arm in every scene was verified run-by-run during the exhibition (`verifyRunIsBaseline`); run manifests record the tooling-branch SHA with an explicit note that acceptance rests on the byte-identical implementation tree.

## 3. Scenes

| Scene | Kind | Profile | Input → output |
|---|---|---|---|
| `archive_grid_drift` | synthetic, analytic | fine textures, subpixel drift (0.37, 0.11 px/frame) | 640×360 → 640×360 |
| `crossing_occluders` | synthetic, analytic | 3 crossing/occluding slabs over drifting background | 640×360 → 640×360 |
| `bbb_detail_motion` | real (BBB © Blender Foundation, CC BY 3.0, t+44 s center-crop) | flower close-ups, modest motion | 640×360 → 640×360 |
| `bbb_occlusion` | real (BBB, t+369 s) | butterfly moving across detailed foliage | 640×360 → 640×360 |

Common conditions: 46 input frames @ 24 fps, targets 2–43 (42 frames, full past-2/future-2 window everywhere), HR masters 1280×720, LR inputs produced by separable bicubic 0.5× + independent per-channel Gaussian noise σ=6/255, encoded libx264 CRF 20 yuv420p bt709 (identical for every arm). Scene identities, seeds, degradation models, and per-clip SHA-256 are recorded in each `artifacts/scenes/<id>/scene.json`.

**Scope deviation note:** the assignment's example range was 24–60 target frames; 42 was used. The real-material scenes use the project's sanctioned Blender open-movie source (`benchmarks/video_corpus/README.md` license and attribution apply).

## 4. Native-resolution results (vs `reference_clean`)

Full-reference PSNR/SSIM against the clean LR input passed through the identical decode/color/output path with accumulation disabled. Identical images would measure ∞/1.0000.

| Scene | Arm | PSNR (dB) | SSIM | edge Δ | temporal Δ |
|---|---|---:|---:|---:|---:|
| archive_grid_drift | control | 33.94 | 0.9760 | 15.87 | 6.46 |
| | **ANVIL baseline** | 34.67 | 0.9798 | 16.24 | 4.24 |
| | + refinement + confidence | **35.24** | **0.9820** | **14.55** | 4.30 |
| | + … + geometry | 34.94 | 0.9804 | 17.57 | **4.12** |
| crossing_occluders | control | 36.44 | 0.9648 | 12.16 | 5.06 |
| | **ANVIL baseline** | 36.67 | 0.9740 | 11.41 | 2.48 |
| | + refinement + confidence | 38.57 | 0.9803 | 9.58 | 3.03 |
| | + … + geometry | **38.64** | **0.9809** | **9.56** | 2.94 |
| bbb_detail_motion | control | 37.12 | 0.8938 | 10.70 | 3.98 |
| | **ANVIL baseline** | 39.08 | 0.9503 | 7.87 | 2.24 |
| | + refinement + confidence | **39.92** | 0.9491 | **7.45** | 2.54 |
| | + … + geometry | 39.92 | 0.9491 | 7.45 | 2.54 |
| bbb_occlusion | control | 36.35 | 0.9194 | 11.09 | 3.34 |
| | **ANVIL baseline** | 38.57 | 0.9558 | 8.74 | 1.24 |
| | + refinement + confidence | 38.82 | 0.9561 | 8.48 | 1.46 |
| | + … + geometry | **38.88** | **0.9573** | **8.50** | 1.39 |

Temporal participation is proven in every accumulation run's manifest (e.g. 38,707,200/38,707,200 valid temporal samples; the geometry arm truthfully excludes ~0.4% as insufficient-evidence).

## 5. Spatial delivery results (×2 vs HR master)

| Scene | Arm | Bicubic | Lanczos3 |
|---|---|---:|---:|
| archive_grid_drift | control (spatial) | 29.03 | 29.49 |
| | ANVIL baseline | 28.58 | 29.24 |
| | + refinement + confidence | 28.89 | **29.54** |
| crossing_occluders | control (spatial) | 33.91 | 34.05 |
| | ANVIL baseline | 32.97 | 33.34 |
| | + refinement + confidence | **34.35** | **34.72** |
| bbb_detail_motion | control (spatial) | 32.46 | 32.32 |
| | ANVIL baseline | 32.91 | 32.93 |
| | + refinement + confidence | **33.03** | **33.02** |
| bbb_occlusion | control (spatial) | 29.96 | 30.04 |
| | ANVIL baseline | 30.21 | 30.39 |
| | + refinement + confidence | **30.24** | **30.41** |

Synthetic-scene delivery reference is the pristine master; BBB delivery reference is the decoded excerpt (already lossy — absolute values include the master's coding loss, paired comparisons remain valid, and the catalog labels this accordingly).

## 6. Findings (measured facts → observations → open questions)

**Measured facts.**
1. Temporal accumulation improves native-resolution fidelity vs the decoded-frame control on all four scenes (+0.23 to +2.22 dB PSNR; +0.0027 to +0.0565 SSIM) and reduces output flicker (temporal Δ) on all four scenes.
2. Refinement+confidence improves native PSNR over the frozen baseline on all four scenes, but it is **not** the universal winner: estimated geometry yields higher PSNR on `crossing_occluders` and `bbb_occlusion`; its PSNR ties refinement+confidence on `bbb_detail_motion`, while refinement+confidence SSIM (0.9491) is below the baseline SSIM (0.9503). Report individual metrics rather than a single unqualified winner.
3. Estimated sample geometry: on `crossing_occluders` and `bbb_occlusion` the estimator produced phases on 42/42 frames and marginally improved results; on `archive_grid_drift` it slightly degraded PSNR/SSIM (consistent with the estimator's documented parabolic contraction bias on periodic textures); on `bbb_detail_motion` it produced **no** usable estimates (`geometry_estimation_insufficient` events; 0/42 frames) and the arm is byte-identical to its parent — an honest degradation, not a fabricated phase.
4. Lanczos3 beats bicubic for 2× delivery in 7 of 8 measurements.
5. After 2× delivery, ANVIL arms beat the decoded-frame spatial control on both real scenes but **not** on the two synthetic scenes (e.g. baseline 28.58 vs control 29.03 dB on `archive_grid_drift`).

**Observations (interpretation, not measurements).**
- The real-scene delivery win is plausibly because denoising before upscaling prevents noise amplification; on the synthetic scenes the pristine master contains fine periodic detail that no 640×360 observation can recover, and denoising slightly flattens what the scaler then has to work with.
- The geometry arm's mixed results suggest its current estimator is texture-dependent; it neither helps universally nor harms universally.

**Open questions for Spring Training.**
- Can correspondence refinement close the synthetic-scene delivery gap, or is the loss at the observation's resolution limit?
- Is the geometry estimator's bias on periodic texture correctable, and does it matter on natural footage beyond these two excerpts?
- Do these conclusions hold at other noise levels, CRFs, and delivery factors? (This exhibition measured exactly one point in that space.)

**No claim of general image-quality superiority is made.** These are results under this exhibition's specific scenes, degradations, and geometry.

## 7. Negative/inconclusive results preserved

- Estimated geometry on `bbb_detail_motion`: inconclusive (insufficient evidence on every frame; no phase invented; outputs identical to parent arm).
- Geometry on `archive_grid_drift`: negative at native resolution under these conditions.
- Baseline+delivery below spatial-control delivery on both synthetic scenes (table §5).

## 8. Artifacts and retention

- Git-tracked (this directory): `EXPERIMENTS.json`, `BASELINE.json`, `catalog/catalog.json`, `roster/roster.json`, `manifests/**` (every run manifest + metrics summary), `METRICS.json`, this report, `OPERATOR_GUIDE.md`, `SOURCES.md` (tracked sha256 pin for the CC-BY master), `verification/` (browser-verification captures).
- Local-only (gitignored, regenerable): `artifacts/` (masters, LR frames, clips, run frame outputs, display-derivative cache) and `findings/` (human review state). Regenerate with `anvil_exhibit gen-scenes && prep-real && run && measure && catalog build` (real scenes additionally need the CC-BY master acquired and digest-verified per [`SOURCES.md`](SOURCES.md)).

## 9. Validation evidence

- CTest: **33/33 non-disabled tests pass** on the PR head build (includes `anvil_lab_tests`: 2175 assertions over metrics oracles, naming/roster contracts, baseline fail-closed matrix, PNM/PNG/JSON round-trips, scene determinism).
- Python: `test_anvil_contract.py` 56/56 and `test_anvil_lab_contract.py` 13/13 (builds a complete miniature exhibition end-to-end, then exercises the live server: hash-linked derivatives, zero-diff on identical inputs, invalid-pair fail-closed, missing-asset 404s, findings round-trip with exact A/B identity, falsified/moved baseline rejection).
- Browser: 29/29 scripted checks were reported in Chromium **and** 29/29 in Firefox 157 (stable) at 2560×1440 on the earlier implementation head. Re-run after the subsequent PR repair commits; these archived results are not a substitute for exact-head validation (selection, filters, all 7 comparison modes, regions, sequence playback, zoom/pan/fit/1:1, nearest/bilinear, alignment disclosure, refusal on missing assets, baseline `<dialog>` modal, findings round-trip, metrics gating), plus visual inspection of the captures in `verification/` (engine-specific captures archived).
- Baseline verification on this tree: **OK** (worktree matches pinned identity; pinned commit present).

## 10. Roster state after the exhibition

`ANVIL baseline` — no status (canonical). `ANVIL baseline + local refinement + estimated confidence — tryout` and `… + estimated geometry — tryout` on all scenes: **tryout** (first-evaluation invitations recorded with authority = the 2026-10-09 assignment). No candidate was promoted by metrics; disposition changes remain maintainer-controlled with append-only history.

## 11. Post-review qualification status

The independent review identified issues in CI pinned-commit availability, image labels, reference-aware scientific pairing, selected-frame evidence hashes, baseline source provenance, and scene-scoped roster identities. Focused repairs are being applied on PR #4. **Do not treat the original validation claims above as exact-head qualification until CI and the revised catalog/experiments have been re-executed and reviewed.** The run artifacts were generated before those code changes; the numerical results remain historical preliminary evidence, not post-repair remeasurement. Existing local artifacts are gitignored; the clean-checkout regeneration process and official Big Buck Bunny acquisition instructions are documented in OPERATOR_GUIDE.md.

## Latest repair-state boundary (2026-10-09/10)

Independent review identified malformed workflow indentation, mutable display-cache bytes, missing delivery/HR PTS identities, client-trusted findings, stale finding exports, and incomplete executed-runner identity. Repairs now include checkout YAML normalization, cached-derivative SHA verification, timestamp-matched scientific pairs, server-authenticated saved findings, fresh server-backed exports, and executable SHA observation/revalidation for **new** exhibition runs. The tracked catalog's index has been reconciled with recorded native timestamps. The older historical run manifests have **no** `exhibition_attestation` and therefore **do not meet** the strengthened binary-observed output gate. Do not elevate earlier PSNR/SSIM or pre-repair browser screenshots to new qualified results. Verify GitHub Actions at the literal repaired PR head and regenerate experiment evidence before final adjudication; any unperformed browser verification must remain explicitly pending. The exact source-to-binary build reproducibility question is not closed by a binary hash alone.

## Evaluator re-review response — comparison integrity (October 10, 2026)

The subsequent independent review at `0b801950` found four additional defects/gaps. The PR branch now corrects the `pipelineDiff(candidate, baseline)` argument direction independent of A/B selection; propagates numeric temporal window fields into the actual catalog (`past 2 / future 2` for the production baseline, `past 0 / future 0` for decoded controls); and clearly distinguishes `IMAGE B vs its recorded reference` fidelity scores from **A-vs-B original-pixel difference** statistics. Regression tests cover swapped A/B selection, numeric temporal windows, and A-independent reference metrics.

The original `starter` roster writer treated caller-provided `integration_commit`, `merge_evidence: true`, and `authority` as proof. The local CLI cannot authenticate the GitHub maintainer decision or establish a verified merge into `main`, so that transition now **fails closed** and current/historical `starter` entries are flagged for external verification by the roster auditor. Legitimate `starter` promotion remains an **unimplemented authenticated workflow**: keep genuinely proven, unmerged players at `rookie` until that separate integration gate exists. This restriction is deliberate; falsely marking an unverified player a starter would violate research policy.

The tracked candidate catalog was corrected as metadata only; experimental images and numeric quality measurements were **not regenerated**, and remain pre-repair historical results with prior reproducibility limitations. Exact-head CI and current-browser validation must not be inferred from prior workflow successes. No changes were merged into `main`.
