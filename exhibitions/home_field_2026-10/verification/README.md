# Browser verification captures — 2026-10-09

Rendered by real (headless) Chromium at 2560×1440 against the live
`anvil_review_lab` server on this exhibition's data; downscaled to 1280
wide for storage. Scripted verification: 29/29 checks passed in Chromium and, on 2026-10-09, 29/29 in Firefox 157 (stable) with zero page errors
(selection, filters, frame preservation, swap, all 7 comparison modes,
server diff statistics, regions, sequence stepping/playback, timestamps,
zoom/pan/fit/1:1/nearest, alignment disclosure, missing-asset refusal,
metadata/provenance panels, baseline modal, findings round-trip, metrics
gating). See REPORT.md §9.

- `00_initial_2560x1440.png` — default layout, viewport-dominant.
- `02_selector_panel.png` — searchable/filterable candidate panel with thumbnails.
- `06_bbb_slider_baseline_vs_control.png` — slider wipe: denoised baseline (A) vs decoded control (B).
- `08_bbb_heatmap.png` — server-computed difference heatmap with legend note.
- `10_bbb_split.png` — synchronized side-by-side.
- `01_initial_firefox.png`, `08_heatmap_firefox.png` — the same verification suite executed under Firefox (Playwright's Firefox build, stable channel): identical results, including the `<dialog>` baseline modal and server-header stats parsing.

## Regenerated-evidence captures — 2026-10-10 (`regen_*.png`)

Rendered by real headless **Chromium (`chromium-1248`) and Firefox
(`firefox-1532`)** via Playwright at 1600×900 against live
`anvil_review_lab` servers on the **regenerated attested evidence**
(digest-verified pinned BBB master; clean-tree runner with recorded
byte attestation). Scripted interaction matrix: **29/29 checks passed in
each engine with zero console/page errors**, covering selection (search +
scene filter), qualified-pair banner, swap, all 7 comparison modes, frame
stepping, sequence playback (verified stop), rapid frame scrubbing with
stale-verdict guard, zoom/fit/1:1, nearest/bilinear, server diff
statistics, alignment disclosure, metadata panels, baseline `<dialog>`
modal, HR-master-vs-delivery qualification, metrics gating, the
partial-evidence banner (per-record inventory counts), missing-asset
refusal ("no substitution"), and the findings round-trip (save → reload →
JSON export containing the observation; download verified). The
partial-evidence root for the missing-data checks was a hardlinked copy
with one run frame removed.

- `regen_00_initial_chromium.png` / `regen_01_initial_firefox.png` — default layout on complete evidence (no banner).
- `regen_02_selector_{chromium,firefox}.png` — searchable selector panel.
- `regen_06_slider_{chromium,firefox}.png` — slider wipe.
- `regen_08_heatmap_{chromium,firefox}.png` — server-computed difference heatmap.
- `regen_10_split_{chromium,firefox}.png` — synchronized side-by-side.
- `regen_11_partial_banner_{chromium,firefox}.png` — missing-evidence banner with per-record counts on the partial root.
