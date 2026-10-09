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
