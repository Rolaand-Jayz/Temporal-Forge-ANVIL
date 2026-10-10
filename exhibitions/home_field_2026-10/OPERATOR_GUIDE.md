# ANVIL Visual Review Lab — Operator Guide

A local, self-contained workstation for inspecting ANVIL experiment
evidence: what produced every image, what changed relative to the baseline,
and where each candidate stands. Modeled on the Diffchecker image-compare
workflow, in ANVIL's own dark/green identity.

## Launch

```sh
# from the repository root (build first if needed: cmake --build build --target anvil_review_lab)
build/anvil_review_lab --root exhibitions/home_field_2026-10 \
                       --web-dir tools/anvil_lab/web --port 8787
# open http://127.0.0.1:8787
```

The server binds loopback only. It serves the recorded catalog; it never
writes evidence (only your findings file and display-derivative cache under
`exhibitions/home_field_2026-10/`).

## Reading the screen

- **Header** — current scene, exact frame + native-tick timestamp, experiment identity, pairing validity state, and the ⛓ Baseline button (permanent identity + verification verdict).
- **IMAGE A — Reference / IMAGE B — Candidate** — the two selectors. Names are the candidates' full contract names (e.g. `ANVIL baseline + local refinement + estimated confidence — tryout`); the small caption shows scene, resolution, and roster status (green = active roster, red = cut).
- **Comparison identity header** — *Comparing*, **Changed** / **Unchanged** stage lists derived from the recorded configurations (never from filenames), and the comparison's purpose.
- **Validity banner** — green: matched pair, recorded metrics shown; amber: cautions; red: INVALID experimental pair — still viewable, but labeled as exploratory and recorded metrics are hidden.
- **Right sidebar** — pipeline stages (execution order, modes, active/inactive, provenance, build identity), per-image metadata & artifact hashes, recorded measurements, findings, and exports.

## Selecting images

1. Click **IMAGE A** or **IMAGE B**. The panel lists every record with a thumbnail, its full name, its reconstruction description (e.g. `640x360 input → temporal reconstruction → 640x360 output`), and tags (kind, scene, resolution, roster status).
2. Filter by scene / kind / roster status / resolution / synthetic-vs-real, or search names and algorithms.
3. Click a record. Frame, scene, zoom, pan, and mode are preserved whenever the record covers them; if the current frame does not exist for a record you are told, and **nothing is silently substituted**.
4. **⇄ Swap** exchanges A and B.

Defaults: A = the canonical `ANVIL baseline` of the first scene; B = a compatible tryout candidate. The clean reference, decoded-frame control, spatial controls, delivery variants, and HR masters are all directly selectable under truthful labels.

## Comparison modes (keys 1–7)

| Mode | What it does |
|---|---|
| **Slider** | Pixel-registered A|B wipe; drag the green divider. |
| **Split** | Side-by-side; synchronized zoom/pan by default, or independent navigation (toggle *Sync zoom/pan*). |
| **Fade** | True opacity blend; 50% = equal contribution. |
| **Flicker** | Timed A/B alternation (interval slider, play/pause, manual toggle). |
| **Subtract** | Absolute pixel difference computed by the server from the ORIGINAL PNM samples; gain and per-channel inspection. |
| **Heatmap** | Threshold-colored difference heat (black → ANVIL green → yellow → red) with gain and an explicit "visualization only" note. |
| **Regions** | Connected difference regions: numbered boxes, click-to-focus, adjustable threshold / minimum size / merge distance, overlay toggle. |

## Zoom, pan, alignment

Mouse-wheel zoom (anchored under the cursor), drag to pan, **Fit**, **1:1**
(actual pixels), display interpolation toggle (bilinear ↔ nearest —
matters when pixel-peeping), full screen. **Align B** nudges B by whole
pixels for display only; an amber disclosure appears whenever it is active
— original evidence and measurements are never touched.

## Temporal sequences

◀◀ / ▶▶| step frames; the scrubber and exact frame box address frames by
index; the readout shows the frame's µs + native-tick timestamp; ▶ plays
A and B in lockstep. Zoom and crop persist across frames.

## Saving and exporting findings

1. Frame the observation (any mode/zoom/region), open **Human findings**.
2. Pick a category (improvement, regression, ghosting, blurring/detail
   loss, ringing, temporal instability, alignment issue, uncertain) and
   write what you see. Saving records scene, exact frame, both image IDs,
   the roster status at review time, the region if one is selected, and
   (optionally) a screenshot — the file is
   `findings/findings.json` (portable JSON; import/export from the UI).
3. Findings inform review; they never promote or cut a candidate. Roster
   transitions are made deliberately with
   `anvil_exhibit roster set --root <exhibition> '<transition JSON>'`, which requires
   reason/authority/evidence (and merge evidence for `starter`).

## Exports

Annotated comparison screenshot (identity footer baked in), difference
image (server-computed), current-view crop, and a paired frame contact
sheet (A row above B row). Every export carries the exact compared IDs.

## Recording-baseline checks (CLI)

```sh
build/anvil_exhibit verify-baseline --root exhibitions/home_field_2026-10 --repo-root .
# --manifest exhibitions/home_field_2026-10/manifests/<scene>/baseline.json
# Only NEW exhibition runs record the exact invoked executable SHA-256;
# the original archived manifests are historical, not binary-attested.
```

## Regenerating the exhibition

The real scene inputs are **not included in Git**. Obtain the official CC-BY 3.0 Blender Foundation Big Buck Bunny source directly from the same published upstream URL already used by `benchmarks/video_corpus/prepare_corpus.sh`:

```sh
mkdir -p exhibitions/home_field_2026-10/artifacts/sources
curl --fail --location --retry 3 --continue-at - \
  --output exhibitions/home_field_2026-10/artifacts/sources/big_buck_bunny_1080p_h264.mov \
  https://download.blender.org/peach/bigbuckbunny_movies/big_buck_bunny_1080p_h264.mov
sha256sum exhibitions/home_field_2026-10/artifacts/sources/big_buck_bunny_1080p_h264.mov
```

The SHA-256 printed above records the downloaded input. **The original source master's SHA-256 has not been independently pinned in this Git-tracked package**, so do not claim full bit-for-bit reproduction merely because a download succeeded. Preserve and compare the generated `scene.json` source fingerprints with the recorded scene/run manifest input hashes and declare any mismatch; FFmpeg/tool-version differences can change re-encodes. The reconstructed images under `artifacts/` are ignored by Git and must be regenerated before visual inspection on a fresh clone. Until then the lab must show missing-image states, not fabricated outputs.

```sh
build/anvil_exhibit gen-scenes  --root exhibitions/home_field_2026-10
build/anvil_exhibit prep-real   --root exhibitions/home_field_2026-10 \
    --source exhibitions/home_field_2026-10/artifacts/sources/big_buck_bunny_1080p_h264.mov \
    --excerpt bbb_detail_motion:44 --excerpt bbb_occlusion:369
build/anvil_exhibit run      --root exhibitions/home_field_2026-10 --runner build/anvil_runner --repo-root .
build/anvil_exhibit measure  --root exhibitions/home_field_2026-10
build/anvil_exhibit catalog build --root exhibitions/home_field_2026-10
```

Frames and clips live under `artifacts/` (gitignored); manifests, metrics,
catalog, roster, and reports are tracked.

## Baseline-output evidence qualification

`ANVIL baseline` always names the original source/configuration definition pinned to `f2f8b992`, not the advancing `main` branch. A **particular output run** qualifies as binary-observed only if its manifest includes `exhibition_attestation.runner_path`, matching `runner_sha256` / `runner_sha256_after`, and the same runner bytes remain accessible during verification. The exhibition tool records these for new executions and refuses silent executable replacement. This attests observed executable bytes, **not** independent source-to-binary reproducible compilation. An unverifiable dirty Git build and the older archived manifests require explicit limitations; do not claim their runner binaries have been authenticated. Re-execute the exhibition to obtain new qualified evidence. A standalone source-tree verification does **not** establish historic output quality.
