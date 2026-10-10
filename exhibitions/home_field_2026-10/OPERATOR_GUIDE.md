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
   reason/authority/evidence. **The local roster writer intentionally rejects
   `starter` promotion:** arbitrary input strings, `integration_commit`, and
   `merge_evidence: true` cannot authenticate a real merge on `main` or a
   maintainer decision. Keep qualified candidates at `rookie` until an
   authenticated maintainer-controlled integration workflow verifies the
   merged commit and authorizes the roster update. No self-asserted starter
   status may be displayed as verified.

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

The real scene inputs are **not included in Git**. Obtain the official CC-BY 3.0 Blender Foundation Big Buck Bunny master from the published upstream directory already used by `benchmarks/video_corpus/prepare_corpus.sh` (verified 2026-10-10: the bare `.mov` URL returns 404; the `.zip` is live):

```sh
mkdir -p exhibitions/home_field_2026-10/artifacts/sources
curl --fail --location --retry 3 \
  --output exhibitions/home_field_2026-10/artifacts/sources/bbb.zip \
  https://download.blender.org/peach/bigbuckbunny_movies/big_buck_bunny_1080p_h264.mov.zip
unzip exhibitions/home_field_2026-10/artifacts/sources/bbb.zip \
  -d exhibitions/home_field_2026-10/artifacts/sources
sha256sum exhibitions/home_field_2026-10/artifacts/sources/big_buck_bunny_1080p_h264.mov
```

The digest **must** equal the pin recorded in [`SOURCES.md`](SOURCES.md)
(`dc2146a2b1172def56730143ad80cd1825b7fad15f1fc9c23a4e7d01a741ac11` — the
exact bytes the historical exhibition used). A mismatch means a different
source master: declare it; do not claim reproduction of the tracked results.
Even with a matching master, FFmpeg/tool-version differences can change
re-encodes, so preserve and compare the generated `scene.json` source
fingerprints with the recorded scene/run manifest input hashes and declare
any mismatch. The reconstructed images under `artifacts/` are ignored by
Git and must be regenerated before visual inspection on a fresh clone.
Until then the lab reports `evidence_present: false` on `/api/catalog`,
shows an explicit missing-evidence note in the UI, and answers image
requests with 404 — never fabricated or substitute outputs.

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

## Source identity and reference qualification

**The source pin is an executable gate, not a checklist item.** `prep-real`
fails closed when the supplied master's SHA-256 is not the canonical digest
recorded in [`SOURCES.md`](SOURCES.md) (executable form:
`src/anvil_lab/Sources.hpp`); internal consistency of scene/manifest/artifact
hashes never establishes that the designated source material was used. The
only escape hatch is `--allow-unpinned-source`, which exists solely for
separately labeled **exploratory datasets**: the scene's `scene.json` records
`real_source.canonical: false` and `dataset_class: "exploratory"`, `catalog
build` stamps every record of that scene `source_canonical: false`, and the
Review Lab independently re-derives the scene's source digest and refuses
canonical evidence labels for *any* record — runs, controls, or references —
of a non-pinned real scene. Exploratory data can be browsed, never promoted
into canonical evidence by relabeling.

**HR-master references qualify on their own terms.** References are not
reconstruction runs and carry no runner attestation; the Review Lab qualifies
an `hr_master` record when (a) every master frame matches the tracked
catalog's per-frame SHA-256 inventory, (b) every catalog timestamp entry maps
onto a verified frame and carries native `pts_us`/`pts_ticks` identity, and
(c) provenance traces to the seeded synthetic generator (`scene.json` seed)
or to the pinned real master with the scene's recorded input clips still
matching their generation-time digests. Synthetic and real references are
held to the same standard; only the provenance source differs.

**Evidence readiness is a per-record inventory.** `/api/catalog` reports
`evidence_present` only when *every* catalog record has every expected frame
present and non-empty, with `evidence_inventory` listing each incomplete
record (`expected_frames` / `present_frames`). Zero-byte (truncated) frames
count as missing. The inventory reports *availability*: frame content
integrity is enforced separately by tracked digests at qualification and
serve time, so a present-but-tampered frame still fails qualification while
remaining visible as available.

## Baseline-output evidence qualification

`ANVIL baseline` always names the original source/configuration definition pinned to `f2f8b992`, not the advancing `main` branch. A **particular output run** qualifies as binary-observed only if its manifest includes `exhibition_attestation.runner_path`, matching `runner_sha256` / `runner_sha256_after`, and the same runner bytes remain accessible during verification. The exhibition tool records these for new executions and refuses silent executable replacement. This attests observed executable bytes, **not** independent source-to-binary reproducible compilation. An unverifiable dirty Git build and the older archived manifests require explicit limitations; do not claim their runner binaries have been authenticated. Re-execute the exhibition to obtain new qualified evidence. A standalone source-tree verification does **not** establish historic output quality.

## Metric panels and comparison direction

**Difference statistics** are computed from the currently selected **IMAGE A versus IMAGE B** on matched original pixels. The **reference-fidelity metric panel**, by contrast, shows **IMAGE B versus its recorded clean-LR or HR reference**, not the current A/B pair. Changing A without changing B does not change that fixed reference score. The selection header shows changes from the **ANVIL baseline to the candidate**, even when the visible A/B arrangement is swapped. The pipeline temporal-window labels must contain numeric values, for example `past 2 / future 2`, never blank strings.

## Review Lab write security and concurrency

The Review Lab accepts **findings writes only from its same-origin web UI**.
The server binds loopback, checks the exact loopback Host and Origin for every
POST, requires `Content-Type: application/json`, and validates an unpredictable
per-process token supplied to the UI by the catalog. Cross-origin browser
requests and unauthenticated scripted writes fail closed; the loopback binding
alone is not authentication. Restarting the lab rotates the token.
Do **not** copy catalog session tokens into reports or version-control files.

The first screenshot-free finding creates the findings directory automatically.
Screenshots are optional; supplied screenshots must decode as complete,
CRC-verified, bounded, non-interlaced 8-bit PNG images with valid scanline
decompression, and are written with atomic replacement. Other bytes are
rejected instead of being saved as apparently valid scientific evidence.

The local HTTP listener maintains eight worker threads and a bounded queue of
32 waiting connections. Excess connections may receive HTTP 503 instead of
increasing the thread count indefinitely; existing client calls can retry.
When the server is stopped, pending workers are joined.

A/B/frame changes discard old displayed pixels and scientific verdicts
immediately. Frame images, PTS/metadata, compatibility, difference and region
results are committed to the UI only for the active selection revision.
Missing frames are displayed as unavailable and are **never silently replaced**
with the prior scene's pixels.

The `--repo-root` option (default: current working directory) must point to the
actual Git repository containing the permanent original baseline and pinned
commit object. A selected baseline-labeled run is qualified only if both its
manifest/config identity and the frozen source/commit verification pass.
The ordinary catalog and the presence of a decoded frame alone do not prove
baseline qualification.

These controls are software requirements and CI contracts, not evidence that
the historical Big Buck Bunny campaign has been regenerated or that the final
UI has passed an independent browser walkthrough. Those qualification gates
remain open pending new measurements and evaluator review.
