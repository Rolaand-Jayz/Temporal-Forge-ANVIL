# Source material — tracked acquisition pin (clean-checkout path)

The exhibition's binary media is intentionally not tracked (see
`REPORT.md` §8). This file records the **Git-tracked pin** for the only
external input: the Big Buck Bunny master used by the two real-material
scenes. The synthetic scenes (`archive_grid_drift`, `crossing_occluders`)
are generated locally and need no external material.

## Big Buck Bunny 1080p master (scenes `bbb_detail_motion`, `bbb_occlusion`)

- **License/attribution:** © Blender Foundation | peach.blender.org —
  Creative Commons Attribution 3.0. The repository's sanctioned use of
  this material (with attribution) is recorded in
  `benchmarks/video_corpus/README.md`.
- **Official source directory:**
  `https://download.blender.org/peach/bigbuckbunny_movies/`
  - Verified 2026-10-10: the bare `big_buck_bunny_1080p_h264.mov` URL
    returns **404**; `big_buck_bunny_1080p_h264.mov.zip` returns
    **200** (`application/zip`) and contains
    `big_buck_bunny_1080p_h264.mov` (h264, 1920×1080, 24 fps).
- **Pinned sha256 of the extracted `.mov` (this is the digest of the
  exact bytes the historical exhibition used):**

  ```
  dc2146a2b1172def56730143ad80cd1825b7fad15f1fc9c23a4e7d01a741ac11
  ```

  A download that does not reproduce this digest **must be declared a
  different source master**; downstream re-encodes can then differ even
  with identical commands (FFmpeg/tool-version differences).
- **Enforcement:** the pin is executable, not advisory — it also lives in
  `src/anvil_lab/Sources.hpp` and `anvil_exhibit prep-real` **fails closed**
  on any other digest. `--allow-unpinned-source` exists only for separately
  labeled exploratory datasets, which are recorded as such in `scene.json`,
  stamped `source_canonical: false` in the catalog, and excluded from
  canonical evidence qualification by the Review Lab (see
  `OPERATOR_GUIDE.md` § *Source identity and reference qualification*).
- **Excerpts cut from the master (recorded in each scene's `scene.json`
  when artifacts are regenerated):**
  - `bbb_detail_motion`: start 44 s, 46 frames, crop 1280:720:320:180
  - `bbb_occlusion`: start 369 s, 46 frames, crop 1280:720:320:180

## Reproduction (fresh clone)

```sh
# 1. acquire + verify against the tracked pin
curl --fail --location --retry 3 -o bbb.zip \
  "https://download.blender.org/peach/bigbuckbunny_movies/big_buck_bunny_1080p_h264.mov.zip"
unzip bbb.zip
sha256sum big_buck_bunny_1080p_h264.mov   # must print the pinned digest above

# 2. regenerate the scenes + everything downstream
build/anvil_exhibit gen-scenes  --root exhibitions/home_field_2026-10
build/anvil_exhibit prep-real   --root exhibitions/home_field_2026-10 \
    --source "$PWD/big_buck_bunny_1080p_h264.mov" \
    --excerpt bbb_detail_motion:44 --excerpt bbb_occlusion:369
build/anvil_exhibit run     --root exhibitions/home_field_2026-10 \
    --runner build/anvil_runner --repo-root .
build/anvil_exhibit measure --root exhibitions/home_field_2026-10
build/anvil_exhibit catalog build --root exhibitions/home_field_2026-10
```

Until artifacts are regenerated, the Review Lab reports
`evidence_present: false` on `/api/catalog` (with an explicit note in the
UI) and answers every image request with 404 — it never substitutes
historical or unrelated imagery. Regenerated runs record runner
attestation per `OPERATOR_GUIDE.md`; the **historical** four-scene
results in `REPORT.md` predate that attestation standard and remain
preliminary until a pinned-source, attested regeneration is executed
and reviewed.
