// lab.js — ANVIL Visual Review Lab application.
//
// Design rules encoded here:
// - The UI never invents reconstruction identity: every name, pipeline
//   description, and changed/unchanged list comes from the server's
//   validated catalog (/api/catalog, /api/compat).
// - Difference pixels and statistics are computed by the server from the
//   ORIGINAL PNM samples; this canvas is display only.
// - Mismatched pairs remain viewable but are labeled invalid, and recorded
//   metrics are hidden rather than misattributed.
// - Missing assets are surfaced, never silently substituted.
"use strict";

const $ = (id) => document.getElementById(id);
const canvas = $("canvas");
const ctx = canvas.getContext("2d");

const state = {
  data: null,            // {catalog, roster, scenes, metrics, baseline}
  a: null, b: null,      // catalog records
  imgA: null, imgB: null, imgDiff: null,   // HTMLImageElement / ImageBitmap
  frame: 0,
  mode: "slider",
  zoom: 1, panX: 0, panY: 0,
  fitOnLoad: true,
  smoothing: true,
  alignX: 0, alignY: 0,
  fade: 50,
  flickerTimer: null, flickerShowB: true, flickerPlaying: false,
  playTimer: null, playing: false,
  syncPan: true,
  splitPan: { zoom: 1, x: 0, y: 0 },   // independent nav for split side B
  splitIndependent: false,
  sliderFrac: 0.5,
  regions: [], regionBox: null,
  diffStats: null,
  compat: null,
  loading: 0,
};

// ---------------------------------------------------------------- API

async function api(path) {
  const res = await fetch(path);
  if (!res.ok) {
    let msg = res.statusText;
    try { msg = (await res.json()).error || msg; } catch {}
    throw new Error(msg);
  }
  return res;
}

async function apiJson(path) {
  return (await api(path)).json();
}

function imageUrl(id, frame) {
  return `/api/image?id=${encodeURIComponent(id)}&frame=${frame}`;
}

// ---------------------------------------------------------------- records

function recordById(id) {
  return state.data?.catalog.find((r) => r.id === id) || null;
}

function frameInfo(rec, frame) {
  return (rec?.frames?.index || []).find((f) => f.frame === frame) || null;
}

function sharedFrameRange() {
  // The A/B sequence is the intersection of both records' ranges; the
  // current frame is preserved whenever it stays inside the intersection.
  if (!state.a || !state.b) return null;
  const s = Math.max(state.a.frames.start, state.b.frames.start);
  const e = Math.min(state.a.frames.start + state.a.frames.count,
                     state.b.frames.start + state.b.frames.count);
  return e > s ? { start: s, end: e - 1 } : null;
}

function clampFrame(f) {
  const rec = state.a;   // A drives the scrubber domain
  if (!rec) return 0;
  const lo = rec.frames.start;
  const hi = rec.frames.start + rec.frames.count - 1;
  return Math.max(lo, Math.min(hi, f));
}

// ---------------------------------------------------------------- loading

let loadSeq = 0;
async function loadImages() {
  const seq = ++loadSeq;
  const jobs = [];
  const mk = (id) => new Promise((resolve, reject) => {
    const im = new Image();
    im.onload = () => resolve(im);
    im.onerror = () => reject(new Error(`asset unavailable for '${id}' frame ${state.frame}`));
    im.src = imageUrl(id, state.frame);
  });
  state.loading = 2;
  setStatus("loading frames…");
  try {
    if (state.a) { jobs.push(mk(state.a.id).then((i) => { if (seq === loadSeq) state.imgA = i; })); }
    if (state.b) { jobs.push(mk(state.b.id).then((i) => { if (seq === loadSeq) state.imgB = i; })); }
    await Promise.all(jobs);
  } catch (e) {
    setStatus(`⚠ ${e.message}`, true);
    showBanner("pair", `Asset missing: ${e.message} — no substitution performed.`, "invalid");
  }
  state.loading = 0;
  if (seq !== loadSeq) return;
  if (state.fitOnLoad) fitView();
  await refreshDiff();
  render();
  setStatus("");
}

async function refreshDiff() {
  state.imgDiff = null;
  state.diffStats = null;
  if (!state.a || !state.b) return;
  if (!["diff", "heatmap"].includes(state.mode)) { updateDiffStatsPanel(); return; }
  const kind = state.mode === "heatmap" ? "heatmap" : "absdiff";
  const gain = state.mode === "heatmap" ? $("heat-gain").value : $("diff-gain").value;
  const channel = state.mode === "heatmap" ? "max" : $("diff-channel").value;
  try {
    const res = await api(`/api/diff?id_a=${encodeURIComponent(state.a.id)}&id_b=${encodeURIComponent(state.b.id)}`
      + `&frame=${state.frame}&mode=${kind}&gain=${gain}&channel=${channel}`);
    const statsHeader = res.headers.get("X-Anvil-Stats");
    if (statsHeader) { try { state.diffStats = JSON.parse(statsHeader); } catch {} }
    const blob = await res.blob();
    const url = URL.createObjectURL(blob);
    await new Promise((resolve, reject) => {
      const im = new Image();
      im.onload = () => { state.imgDiff = im; resolve(); };
      im.onerror = reject;
      im.src = url;
    });
    if (state.mode === "heatmap") renderHeatLegend();
  } catch (e) {
    state.diffError = e.message;
    showBanner("pair", `Difference unavailable: ${e.message}`, "invalid");
  }
  updateDiffStatsPanel();
}

async function refreshRegions() {
  state.regions = [];
  if (!state.a || !state.b || state.mode !== "regions") { renderRegionList(); return; }
  try {
    const j = await apiJson(`/api/regions?id_a=${encodeURIComponent(state.a.id)}&id_b=${encodeURIComponent(state.b.id)}`
      + `&frame=${state.frame}&threshold=${$("region-threshold").value}`
      + `&min_size=${$("region-minsize").value}&merge=${$("region-merge").value}`);
    state.regions = j.regions || [];
  } catch (e) {
    setStatus(`regions: ${e.message}`, true);
  }
  renderRegionList();
  render();
}

async function refreshCompat() {
  if (!state.a || !state.b) { $("pair-banner").classList.add("hidden"); state.compat = null; return; }
  try {
    state.compat = await apiJson(`/api/compat?id_a=${encodeURIComponent(state.a.id)}&id_b=${encodeURIComponent(state.b.id)}&frame=${state.frame}`);
  } catch (e) {
    state.compat = { valid_pair: false, problems: [e.message], warnings: [] };
  }
  const c = state.compat;
  const el = $("pair-banner");
  el.classList.remove("hidden", "valid", "invalid", "warnonly");
  const probs = c.problems || [];
  const warns = c.warnings || [];
  const evidenceWarnings = c.evidence_problems || [];
  if (c.qualified_experiment && probs.length === 0 && warns.length === 0) {
    el.classList.add("valid");
    el.textContent = "✓ QUALIFIED experiment — pixel pairing, experiment eligibility, and executed-run attestation verified.";
  } else if (probs.length === 0) {
    el.classList.add("warnonly");
    el.innerHTML = "⚠ " + (c.valid_pair ? "Pixel pair compatible" : "Exploratory/non-experiment pair")
      + " — " + esc([...warns, ...evidenceWarnings].join(" · ")
        || "not eligible or not qualified as scientific evidence")
      + ". Historical scores are NOT qualified.";
  } else {
    el.classList.add("invalid");
    el.innerHTML = "✗ INVALID EXPERIMENTAL PAIR — " + esc(probs.join(" · "))
      + ". Exploratory viewing only; this is not a valid paired experiment and recorded metrics are hidden.";
  }
  $("head-validation").textContent = c.qualified_experiment ? "evidence: QUALIFIED" : "evidence: UNQUALIFIED";
  $("head-validation").className = "head-state " + (c.qualified_experiment ? "ok" : "bad");
  updateMetricsPanel();
  updateCompareHeader();
}

// ---------------------------------------------------------------- selection

function armLabel(r) {
  const status = r.roster_status ? ` — ${r.roster_status}` : "";
  return r.display_name;
}

function selectImage(side, id, { keepFrame = true } = {}) {
  const rec = recordById(id);
  if (!rec) return;
  const prevFrame = state.frame;
  state[side] = rec;
  state.fitOnLoad = false;
  if (keepFrame) {
    state.frame = clampFrame(prevFrame);
  }
  if (state.splitIndependent && state.mode === "split") {
    state.splitPan = { zoom: state.zoom, x: state.panX, y: state.panY };
  }
  updateSelectorButtons();
  updateSeqBar();
  refreshCompat().then(() => {
    loadImages();
    refreshRegions();
    updateMetaPanels();
    updatePipelinePanel();
  });
}

function swapAB() {
  const a = state.a;
  state.a = state.b;
  state.b = a;
  state.alignX = 0; state.alignY = 0;
  $("align-x").value = 0; $("align-y").value = 0;
  updateAlignBanner();
  updateSelectorButtons();
  refreshCompat().then(() => { loadImages(); refreshRegions(); updateMetaPanels(); updatePipelinePanel(); });
}

function defaultSelection() {
  const cat = state.data.catalog;
  // Default A: the canonical ANVIL baseline of the first scene with one.
  const sceneOrder = state.data.scenes.map((s) => s.scene_id);
  const byScene = (id) => id.split("/")[0];
  let a = null, b = null;
  for (const s of sceneOrder) {
    a = cat.find((r) => r.kind === "anvil_baseline" && byScene(r.id) === s);
    if (a) break;
  }
  if (!a) a = cat.find((r) => r.kind === "anvil_baseline")
    || cat.find((r) => r.kind === "reference") || cat[0];
  const scene = byScene(a.id);
  b = cat.find((r) => r.kind === "anvil_candidate" && byScene(r.id) === scene
               && r.frames.count === a.frames.count)
   || cat.find((r) => r.kind === "control" && byScene(r.id) === scene && r !== a)
   || cat.find((r) => r.id !== a.id && byScene(r.id) === scene);
  state.a = a;
  state.b = b;
  state.frame = a.frames.start + Math.floor(a.frames.count / 2);
  updateSelectorButtons();
  updateSeqBar();
}

// ---------------------------------------------------------------- selector UI

function openSelector(side) {
  const panel = $("selector-panel");
  const btn = side === "a" ? $("sel-btn-a") : $("sel-btn-b");
  panel.dataset.side = side;
  panel.classList.remove("hidden");
  const rect = btn.getBoundingClientRect();
  panel.style.left = Math.max(8, rect.left) + "px";
  panel.style.top = (rect.bottom + 6) + "px";
  const list = $("sp-list");
  renderSelectorList();
  $("sp-search").focus();
}

function renderSelectorList() {
  const panel = $("selector-panel");
  const side = panel.dataset.side;
  const q = $("sp-search").value.trim().toLowerCase();
  const fScene = $("sp-filter-scene").value;
  const fKind = $("sp-filter-kind").value;
  const fStatus = $("sp-filter-status").value;
  const fRes = $("sp-filter-res").value;
  const fSrc = $("sp-filter-source").value;
  const list = $("sp-list");
  list.innerHTML = "";
  const current = side === "a" ? state.a?.id : state.b?.id;
  const other = side === "a" ? state.b?.id : state.a?.id;
  // Mark records whose current frame is unavailable for the active frame —
  // the reviewer sees incompatibility instead of silent substitution.
  for (const r of state.data.catalog) {
    const sceneId = r.scene.id;
    const res = `${r.output.width}×${r.output.height}`;
    const src = r.scene.synthetic ? "synthetic" : "real";
    const algo = r.pipeline?.stages?.map((s) => `${s.name}:${s.mode}`).join(" ").toLowerCase() || "";
    if (fScene && sceneId !== fScene) continue;
    if (fKind && r.kind !== fKind) continue;
    if (fStatus && r.roster_status !== fStatus) continue;
    if (fRes && res !== fRes) continue;
    if (fSrc && src !== fSrc) continue;
    if (q && !(r.display_name.toLowerCase().includes(q)
               || r.description.toLowerCase().includes(q)
               || algo.includes(q))) continue;
    const item = document.createElement("div");
    item.className = "sp-item" + (r.id === current ? " selected" : "");
    const thumb = document.createElement("img");
    thumb.className = "sp-thumb";
    thumb.loading = "lazy";
    thumb.src = imageUrl(r.id, r.frames.start + Math.floor(r.frames.count / 2));
    thumb.alt = "";
    const names = document.createElement("div");
    names.className = "sp-names";
    const nm = document.createElement("div");
    nm.className = "sp-name";
    nm.textContent = r.display_name + (r.id === other ? "  (other side)" : "");
    names.appendChild(nm);
    const ds = document.createElement("div");
    ds.className = "sp-desc";
    ds.textContent = r.description;
    names.appendChild(ds);
    const tags = document.createElement("div");
    tags.className = "sp-tags";
    const tag = (t, cls = "") => {
      const e = document.createElement("span");
      e.className = "sp-tag" + (cls ? " " + cls : "");
      e.textContent = t;
      tags.appendChild(e);
    };
    if (r.roster_status) tag(r.roster_status, "status-" + r.roster_status);
    tag(r.kind.replace(/_/g, " "));
    tag(sceneId);
    tag(`${r.input.width}×${r.input.height} → ${r.output.width}×${r.output.height}`
        + (r.scale.factor !== 1 ? ` (${r.scale.factor}×)` : ""));
    if (r.scene.synthetic === false) tag("real");
    names.appendChild(tags);
    const right = document.createElement("div");
    right.className = "sp-right";
    const framesOk = r.frames.start <= state.frame
      && state.frame < r.frames.start + r.frames.count;
    right.textContent = `${r.frames.count} frames\n${res}`
      + (framesOk ? "" : `\n⚠ frame ${state.frame} not in range`);
    right.style.whiteSpace = "pre-line";
    if (!framesOk) item.classList.add("unavailable");
    item.append(thumb, names, right);
    item.onclick = () => {
      // Missing frame in range: report, never silently change frame/scene.
      if (!framesOk) {
        alert(`'${r.display_name}' has no frame ${state.frame} `
          + `(range ${r.frames.start}..${r.frames.start + r.frames.count - 1}).\n`
          + `The current frame is preserved. Pick a frame inside that range first, `
          + `or select a record from the same scene with matching frame coverage.`);
        return;
      }
      selectImage(side, r.id);
      closeSelector();
    };
    list.appendChild(item);
  }
  if (!list.children.length) {
    list.innerHTML = `<div class="note">No records match the current filters.</div>`;
  }
}

function closeSelector() {
  $("selector-panel").classList.add("hidden");
}

function initSelectorFilters() {
  const scenes = [...new Set(state.data.catalog.map((r) => r.scene.id))];
  const reses = [...new Set(state.data.catalog.map((r) => `${r.output.width}×${r.output.height}`))];
  for (const s of scenes) {
    const o = document.createElement("option");
    o.textContent = s;
    $("sp-filter-scene").appendChild(o);
  }
  for (const r of reses) {
    const o = document.createElement("option");
    o.textContent = r;
    $("sp-filter-res").appendChild(o);
  }
  ["sp-search", "sp-filter-scene", "sp-filter-kind", "sp-filter-status",
   "sp-filter-res", "sp-filter-source"].forEach((id) => {
    $(id).addEventListener("input", renderSelectorList);
  });
}

// ---------------------------------------------------------------- rendering

function viewportSize() {
  const v = $("viewport");
  return { w: v.clientWidth, h: v.clientHeight };
}

function fitView() {
  const img = state.imgA || state.imgB;
  if (!img) return;
  const { w, h } = viewportSize();
  state.zoom = Math.min(w / img.naturalWidth, h / img.naturalHeight) * 0.96;
  state.panX = 0;
  state.panY = 0;
  state.sliderFrac = 0.5;
  updateZoomReadout();
}

function drawImageTransformed(img, { zoom = state.zoom, x = state.panX, y = state.panY, alpha = 1, clip = null } = {}) {
  if (!img) return;
  const { w, h } = viewportSize();
  ctx.save();
  if (clip) ctx.beginPath(), ctx.rect(clip.x0, 0, clip.x1 - clip.x0, h), ctx.clip();
  ctx.globalAlpha = alpha;
  ctx.imageSmoothingEnabled = state.smoothing;
  ctx.imageSmoothingQuality = "high";
  const dw = img.naturalWidth * zoom, dh = img.naturalHeight * zoom;
  ctx.drawImage(img, w / 2 + x - dw / 2, h / 2 + y - dh / 2, dw, dh);
  ctx.restore();
}

function alignOffset() {
  // Manual alignment is DISPLAY-ONLY: applied to B's draw position.
  return { x: state.alignX * state.zoom, y: state.alignY * state.zoom };
}

function render() {
  const { w, h } = viewportSize();
  const dpr = window.devicePixelRatio || 1;
  if (canvas.width !== Math.round(w * dpr) || canvas.height !== Math.round(h * dpr)) {
    canvas.width = Math.round(w * dpr);
    canvas.height = Math.round(h * dpr);
  }
  ctx.setTransform(dpr, 0, 0, dpr, 0, 0);
  ctx.clearRect(0, 0, w, h);
  const mode = state.mode;
  const off = alignOffset();

  if (mode === "slider") {
    const splitX = Math.round(w * state.sliderFrac);
    drawImageTransformed(state.imgA);
    drawImageTransformed(state.imgB, { x: state.panX + off.x, y: state.panY + off.y, clip: { x0: splitX, x1: w } });
    // Divider drawn by the DOM handle; paint side labels.
    ctx.fillStyle = "rgba(55,209,122,.9)";
    ctx.font = "700 11px ui-monospace, monospace";
    ctx.fillText("A", 10, 20);
    ctx.fillStyle = "rgba(245,162,59,.95)";
    ctx.fillText("B", w - 20, 20);
  } else if (mode === "split") {
    const half = w / 2;
    const zB = state.splitIndependent ? state.splitPan.zoom : state.zoom;
    const xB = state.splitIndependent ? state.splitPan.x : state.panX;
    const yB = state.splitIndependent ? state.splitPan.y : state.panY;
    drawImageTransformed(state.imgA, { clip: { x0: 0, x1: half - 1 } });
    drawImageTransformed(state.imgB, { zoom: zB, x: xB + off.x, y: yB + off.y, clip: { x0: half + 1, x1: w } });
    ctx.fillStyle = "rgba(55,209,122,.9)";
    ctx.fillRect(half - 1, 0, 2, h);
    ctx.font = "700 11px ui-monospace, monospace";
    ctx.fillStyle = "rgba(55,209,122,.9)"; ctx.fillText("A · reference", 10, 20);
    ctx.fillStyle = "rgba(245,162,59,.95)"; ctx.fillText("B · candidate", half + 10, 20);
  } else if (mode === "fade") {
    drawImageTransformed(state.imgA);
    drawImageTransformed(state.imgB, { alpha: state.fade / 100, x: state.panX + off.x, y: state.panY + off.y });
  } else if (mode === "flicker") {
    const img = state.flickerShowB ? state.imgB : state.imgA;
    drawImageTransformed(img, state.flickerShowB ? { x: state.panX + off.x, y: state.panY + off.y } : {});
    ctx.font = "700 11px ui-monospace, monospace";
    ctx.fillStyle = state.flickerShowB ? "rgba(245,162,59,.95)" : "rgba(55,209,122,.9)";
    ctx.fillText(state.flickerShowB ? "B" : "A", w - 20, 20);
  } else if (mode === "diff" || mode === "heatmap") {
    if (state.imgDiff) {
      drawImageTransformed(state.imgDiff);
    } else {
      ctx.fillStyle = "#9aa69c";
      ctx.font = "12px ui-monospace, monospace";
      const msg = state.diffError || "computing difference…";
      ctx.fillText(msg, 16, 26);
    }
    if (mode === "heatmap") {
      ctx.fillStyle = "rgba(232,236,232,.9)";
      ctx.font = "10px ui-monospace, monospace";
      ctx.fillText("visualization only — numeric measurements live in the Measurements panel / metrics.json", 16, h - 34);
    }
  } else if (mode === "regions") {
    drawImageTransformed(state.imgA);
    if ($("region-show").checked) drawRegionBoxes();
  }
  updateZoomReadout();
}

function imageToScreen(img, ix, iy, { zoom = state.zoom, x = state.panX, y = state.panY } = {}) {
  const { w, h } = viewportSize();
  const dw = img.naturalWidth * zoom, dh = img.naturalHeight * zoom;
  return { sx: w / 2 + x - dw / 2 + ix * zoom, sy: h / 2 + y - dh / 2 + iy * zoom };
}

function drawRegionBoxes() {
  const img = state.imgA;
  if (!img) return;
  const w = viewportSize().w;
  ctx.font = "700 11px ui-monospace, monospace";
  for (const r of state.regions) {
    const p0 = imageToScreen(img, r.x0, r.y0);
    const p1 = imageToScreen(img, r.x1, r.y1);
    const sel = state.regionBox === r.id;
    ctx.strokeStyle = sel ? "#f5d13b" : "rgba(55,209,122,.95)";
    ctx.lineWidth = sel ? 3 : 1.5;
    ctx.strokeRect(p0.sx, p0.sy, p1.sx - p0.sx, p1.sy - p0.sy);
    const label = `#${r.id} ${r.pixels}px`;
    const tw = ctx.measureText(label).width + 8;
    ctx.fillStyle = sel ? "#f5d13b" : "rgba(55,209,122,.95)";
    ctx.fillRect(p0.sx, p0.sy - 16, tw, 15);
    ctx.fillStyle = "#0a0d0b";
    ctx.fillText(label, p0.sx + 4, p0.sy - 5);
  }
  void w;
}

function updateZoomReadout() {
  const pct = Math.round(state.zoom * 100);
  $("zoom-level").textContent = pct >= 100 ? `${pct}% (1:${(state.zoom).toFixed(2)})` : `${pct}%`;
}

function renderHeatLegend() {
  // The scale/legend anchors: diff value → normalized via gain, saturating.
  const gain = parseFloat($("heat-gain").value);
  const maxval = state.diffStats?.maxval || 255;
  const norm = (v) => Math.min(1, (v * gain) / maxval);
  $("hud-stats").textContent = state.diffStats
    ? `mean|Δ| ${state.diffStats.mean_abs.toFixed(2)} · max|Δ| ${state.diffStats.max_abs.toFixed(0)} · `
      + `hot ${(100 * state.diffStats.hot_ratio).toFixed(2)}% (≥16) `
      + `· legend: 0 → ${norm(16).toFixed(2)}@16 → 1.0@${Math.round(maxval / gain)} (sat)`
    : "";
}

// ---------------------------------------------------------------- panels

function updateSelectorButtons() {
  for (const [side, rec] of [["a", state.a], ["b", state.b]]) {
    if (!rec) continue;
    $(`sel-name-${side}`).textContent = rec.display_name;
    $(`sel-meta-${side}`).textContent =
      `${rec.scene.id} · ${rec.output.width}×${rec.output.height}`
      + (rec.roster_status ? ` · ${rec.roster_status}` : "");
    $(`sel-meta-${side}`).style.color =
      rec.roster_status === "cut" ? "var(--red)"
      : rec.roster_status ? "var(--accent)" : "var(--ink-dim)";
  }
  const both = state.a && state.b;
  $("btn-swap").disabled = !both;
}

function updateSeqBar() {
  const rec = state.a;
  if (!rec) return;
  const lo = rec.frames.start;
  const hi = lo + rec.frames.count - 1;
  const scrub = $("seq-scrub");
  scrub.min = lo;
  scrub.max = hi;
  scrub.value = clampFrame(state.frame);
  $("seq-frame").min = lo;
  $("seq-frame").max = hi;
  $("seq-frame").value = state.frame;
  $("seq-total").textContent = hi;
  const fi = frameInfo(rec, state.frame);
  $("seq-ts").textContent = fi
    ? `pts ${fi.pts_us} µs · ${fi.pts_ticks} ticks`
    : (rec.kind === "reference" && rec.zero_padded ? "master frame (no stream pts)" : "");
  $("head-scene").textContent = state.a?.scene.label || "—";
  $("head-frame").textContent = `frame ${state.frame}` + (fi ? ` · ${fi.pts_us}µs` : "");
  $("head-experiment").textContent = `Home Field Exhibition 2026-10 · A=${state.a?.id || "—"} · B=${state.b?.id || "—"}`;
}

function updateCompareHeader() {
  $("ch-a").textContent = state.a?.display_name || "—";
  $("ch-b").textContent = state.b?.display_name || "—";
  const d = state.compat?.pipeline_diff;
  if (d) {
    $("ch-changed").innerHTML = "<b>Changed:</b> " + esc(d.changed.map((c) =>
      c.stage + (c.change === "parameter" ? " (parameter)" : ` (${c.change})`)).join(", ") || "nothing");
    $("ch-unchanged").innerHTML = "<b>Unchanged:</b> " + esc(d.unchanged.join(", ") || "—");
    $("ch-purpose").innerHTML = "<b>Purpose:</b> " + esc(purposeText());
  } else {
    $("ch-changed").innerHTML = "<b>Changed:</b> —";
    $("ch-unchanged").innerHTML = "<b>Unchanged:</b> —";
    $("ch-purpose").innerHTML = "<b>Purpose:</b> —";
  }
}

function purposeText() {
  if (!state.a || !state.b) return "—";
  const kinds = [state.a.kind, state.b.kind].sort().join("+");
  const map = {
    "anvil_baseline+anvil_candidate": "isolate the candidate's changed stages relative to the canonical baseline",
    "anvil_baseline+control": "measure what temporal reconstruction itself changes vs decoded frames",
    "anvil_candidate+control": "measure the candidate's total effect vs decoded frames",
    "anvil_baseline+delivery_variant": "inspect one arm's native vs delivered output",
    "anvil_candidate+reference": "compare a candidate against the clean reference",
    "anvil_baseline+reference": "compare the baseline against the clean reference",
  };
  return map[kinds] || "visual comparison";
}

function stageRows(rec) {
  return (rec.pipeline?.stages || []).map((s) => `
    <div class="stage-row">
      <span class="stage-dot${s.active ? "" : " off"}"></span>
      <span class="stage-name">${esc(s.name)}</span>
      <span class="stage-mode">${esc(s.mode)}${s.active ? "" : " · inactive"}</span>
    </div>`).join("");
}

function updatePipelinePanel() {
  const rec = state.b || state.a;
  if (!rec) return;
  $("pd-title").textContent = (state.b || state.a).display_name;
  $("pd-summary").textContent = rec.description
    + (rec.scale.factor !== 1 ? ` (delivery ${rec.scale.factor}×)` : " (no spatial scaling)");
  $("pd-stages").innerHTML = stageRows(rec)
    + (rec.delivery ? `<div class="stage-row"><span class="stage-dot"></span>`
      + `<span class="stage-name">delivery_${rec.delivery.filter}</span>`
      + `<span class="stage-mode">${esc(rec.delivery.filter_implementation)}</span></div>` : "");
  const id = rec.identity || {};
  $("pd-identity").innerHTML = [
    id.git_sha ? `source ${id.git_sha}${id.git_dirty === "true" ? " (dirty)" : ""}` : "",
    id.config_hash ? `config ${id.config_hash.slice(0, 16)}…` : "",
    id.compiler_id ? `built with ${id.compiler_id} (${id.build_type})` : "",
    id.ffmpeg_version ? `ffmpeg ${id.ffmpeg_version}` : "",
    rec.temporal ? `temporal samples: ${rec.temporal.valid_samples}/${rec.temporal.total_samples} valid`
      + (rec.temporal.geometry_estimated_frames ? ` · geometry estimated on ${rec.temporal.geometry_estimated_frames} frames` : "") : "",
  ].filter(Boolean).map(esc).join("<br>");
}

function metaRows(rec, side) {
  if (!rec) return "";
  const rows = [
    ["label", rec.display_name],
    ["kind", rec.kind],
    ["roster", rec.roster_status || "—"],
    ["scene", rec.scene.id + (rec.scene.synthetic ? "" : " (real)")],
    ["in / out", `${rec.input.width}×${rec.input.height} → ${rec.output.width}×${rec.output.height}`],
    ["scale", `${rec.scale.factor}× — ${rec.scale.description}`],
    ["format", rec.output.format],
    ["bit depth", `${rec.output.maxval >= 1023 ? "10" : rec.output.maxval >= 255 ? "8" : "?"}-bit (maxval ${rec.output.maxval})`],
    ["color", rec.color?.pixel_format ? `${rec.color.pixel_format} ${rec.color.range || ""} ${rec.color.matrix || ""} ${rec.color.transfer || ""}` : "—"],
    ["frames", `${rec.frames.count} @ ${rec.frames.dir}`],
    ["input clip", rec.input.clip ? `${rec.input.clip} (${(rec.input.clip_sha256 || "").slice(0, 12)}…)` : "—"],
    ["run sha", rec.identity?.manifest_sha256 ? rec.identity.manifest_sha256.slice(0, 24) + "…" : "—"],
  ];
  return `<div class="meta-title ${side}">${side.toUpperCase()} — ${esc(rec.id)}</div>`
    + rows.map(([k, v]) => `<div class="k">${esc(k)}</div><div class="v">${esc(String(v))}</div>`).join("");
}

async function updateMetaPanels() {
  $("meta-grid-a").innerHTML = metaRows(state.a, "a");
  $("meta-grid-b").innerHTML = metaRows(state.b, "b");
  // Frame-level detail (size + artifact hash) for the active frame.
  for (const side of ["a", "b"]) {
    const rec = state[side];
    if (!rec) continue;
    try {
      const m = await apiJson(`/api/meta?id=${encodeURIComponent(rec.id)}&frame=${state.frame}`);
      const d = m.frame_detail;
      if (d && m && state[side] === rec) {
        const grid = $(`meta-grid-${side}`);
        grid.innerHTML += `<div class="k">frame ${d.frame_index}</div><div class="v">${esc(d.file)} · ${d.file_size_bytes}B · ${d.artifact_sha256.slice(0, 16)}…</div>`;
      }
    } catch {}
  }
}

function updateMetricsPanel() {
  const grid = $("metrics-grid");
  const valid = state.compat?.qualified_experiment;
  // These are per-arm reference scores, NOT a numeric score for A-vs-B.
  // A-vs-B pixel differences are reported separately in the Difference panel.
  if (!valid) {
    grid.innerHTML = `<div class="k">status</div><div class="v">UNQUALIFIED / historical reference scores hidden — ${esc((state.compat?.evidence_problems || []).join(" · ") || (state.compat?.problems || []).join(" · ") || "pair not an independently qualified experiment")}</div>`;
    return;
  }
  const sceneId = state.b?.scene?.id;
  const armId = state.b?.id.split("/").slice(1).join("/").replace(/\/(bicubic|lanczos3)$/, "");
  const filter = state.b?.id.endsWith("/bicubic") ? "bicubic" : state.b?.id.endsWith("/lanczos3") ? "lanczos3" : null;
  const row = state.data.metrics?.rows?.find((r) => r.scene === sceneId && r.arm === armId);
  if (!row) {
    grid.innerHTML = `<div class="k">status</div><div class="v">no recorded metrics for this record</div>`;
    return;
  }
  const m = filter ? row[`delivery_${filter}`] : row.native;
  if (!m) {
    grid.innerHTML = `<div class="k">status</div><div class="v">no recorded reference metrics for IMAGE B</div>`;
    return;
  }
  const reference = filter ? "HR master" : (row.native?.reference || "recorded clean LR reference");
  const rows = [
    ["score type", "IMAGE B vs recorded reference — NOT IMAGE A vs IMAGE B"],
    ["IMAGE B", state.b.display_name],
    ["reference", reference],
    ["A/B pixel difference", "See separate Difference statistics panel"],
    ...(filter ? [["delivery scaling", `${m.filter} ${state.b.scale?.factor || "?"}×`]] : []),
    ["B vs reference PSNR", fmt(m.psnr_db_mean, " dB")],
    ["B vs reference SSIM", m.ssim_mean == null ? "—" : m.ssim_mean.toFixed(4)],
    ...(!filter ? [
      ["B vs reference edge Δ", m.edge_diff_mean == null ? "—" : m.edge_diff_mean.toFixed(3)],
      ["B temporal Δ", m.temporal_delta_mean == null ? "—" : m.temporal_delta_mean.toFixed(3)],
    ] : []),
    ["validity", m.reference_validity || "reference conditions unspecified"],
  ];
  grid.innerHTML = rows.map(([k, v]) => `<div class="k">${esc(k)}</div><div class="v">${esc(String(v))}</div>`).join("");
}

function fmt(v, suffix) {
  return v == null ? "—" : (v >= 999 ? "∞ (identical)" : v.toFixed(2)) + (suffix || "");
}

function updateDiffStatsPanel() {
  const el = $("diff-stats");
  if (!state.diffStats) { el.innerHTML = ""; return; }
  const s = state.diffStats;
  el.innerHTML = `
    <div class="meta-title">${s.qualified_experiment ? "QUALIFIED A/B difference" : "EXPLORATORY PIXEL DESCRIPTORS — NOT SCIENTIFIC METRICS"}</div>
    <div class="k">qualification</div><div class="v">${esc(s.qualified_experiment ? "qualified" : (s.qualification_reason || "unqualified"))}</div>
    <div class="k">mean |Δ|</div><div class="v">${s.mean_abs.toFixed(3)} / ${s.maxval}</div>
    <div class="k">max |Δ|</div><div class="v">${s.max_abs.toFixed(0)}</div>
    <div class="k">nonzero px</div><div class="v">${s.nonzero_pixels.toLocaleString()}</div>
    <div class="k">hot px (≥16)</div><div class="v">${s.hot_pixels.toLocaleString()} (${(100 * s.hot_ratio).toFixed(2)}%)</div>
    <div class="k">identical</div><div class="v">${s.identical ? "yes — zero difference" : "no"}</div>
    <div class="k">pair valid</div><div class="v">${s.pair_valid ? "yes" : "NO — visualization only"}</div>`;
}

// ---------------------------------------------------------------- regions UI

function renderRegionList() {
  const el = $("region-list");
  if (state.mode !== "regions" || !state.regions.length) {
    el.classList.add("hidden");
    return;
  }
  el.classList.remove("hidden");
  el.innerHTML = `<div class="rl-title">difference regions (${state.regions.length})</div>`
    + state.regions.map((r) => `
      <div class="rl-item" data-id="${r.id}">
        <span class="rl-num">#${r.id}</span>
        <span>${r.x0},${r.y0} – ${r.x1},${r.y1}</span>
        <span>${r.pixels.toLocaleString()}px</span>
        <span>μ${r.mean_abs.toFixed(1)}</span>
      </div>`).join("");
  el.querySelectorAll(".rl-item").forEach((item) => {
    item.onclick = () => focusRegion(parseInt(item.dataset.id, 10));
  });
}

function focusRegion(id) {
  const r = state.regions.find((x) => x.id === id);
  if (!r || !state.imgA) return;
  state.regionBox = id;
  const cx = (r.x0 + r.x1) / 2, cy = (r.y0 + r.y1) / 2;
  const { w, h } = viewportSize();
  const iw = state.imgA.naturalWidth, ih = state.imgA.naturalHeight;
  // Fit region with context, then center.
  state.zoom = Math.min(w / (r.x1 - r.x0 + 40), h / (r.y1 - r.y0 + 40), 8);
  state.panX = w / 2 - (iw / 2 + (cx - iw / 2)) * state.zoom - iw * state.zoom / 2 + iw * state.zoom / 2;
  // center on (cx, cy): panX = w/2 - (cx * zoom) - (iw/2 - cx)*0 ... simpler:
  state.panX = w / 2 - cx * state.zoom;
  state.panY = h / 2 - cy * state.zoom;
  render();
}

// ---------------------------------------------------------------- banners/status

function showBanner(which, message, kind) {
  const el = $(which === "pair" ? "pair-banner" : "align-banner");
  el.classList.remove("hidden", "valid", "invalid", "warnonly");
  if (kind) el.classList.add(kind);
  el.textContent = message;
}

function updateAlignBanner() {
  const el = $("align-banner");
  if (state.alignX === 0 && state.alignY === 0) {
    el.classList.add("hidden");
  } else {
    el.classList.remove("hidden");
    el.textContent = `Manual alignment offset active (B shifted ${state.alignX}, ${state.alignY}px). `
      + `This is a DISPLAY aid only — original evidence and all measurements are unchanged.`;
  }
}

function setStatus(msg, isErr = false) {
  $("sb-center").textContent = msg || "";
  $("sb-center").style.color = isErr ? "var(--red)" : "var(--ink-faint)";
}

function esc(s) {
  return String(s ?? "").replace(/[&<>"']/g, (c) => ({
    "&": "&amp;", "<": "&lt;", ">": "&gt;", '"': "&quot;", "'": "&#39;",
  }[c]));
}

// ---------------------------------------------------------------- findings

async function loadFindings() {
  try {
    const j = await apiJson("/api/findings");
    // One source of truth for visible cards and user-exportable findings.
    window.__findingsCache = j.findings || [];
    renderFindings(window.__findingsCache);
  } catch (e) {
    // A transient fetch error must not erase previously loaded evidence.
    setStatus("Unable to refresh findings: " + e.message, true);
  }
}

function renderFindings(list) {
  $("findings-count").textContent = list.length;
  $("findings-list").innerHTML = list.slice().reverse().map((f) => `
    <div class="finding-card">
      <div class="fc-head">
        <span class="fc-cat ${esc(f.category)}">${esc(f.category.replace(/_/g, " "))}</span>
        <span>${esc(f.saved_at || "")}</span>
      </div>
      <div>${esc(f.observation)}</div>
      <div class="fc-head" style="margin-top:4px">
        <span>A ${esc(f.image_a)} → B ${esc(f.image_b)}</span>
        <span>frame ${f.frame}</span>
      </div>
      ${f.roster_status_at_review ? `<div class="fc-head"><span>status at review: ${esc(f.roster_status_at_review)}</span>${f.screenshot_file ? `<span>📸 ${esc(f.screenshot_file)}</span>` : ""}</div>` : ""}
    </div>`).join("");
}

async function saveFinding() {
  if (!state.a || !state.b) return alert("Select both images first.");
  const category = $("f-category").value;
  const observation = $("f-observation").value.trim();
  if (!category) return alert("Choose a finding category.");
  if (!observation) return alert("Write the observation first.");
  const body = {
    scene_id: state.a.scene.id,
    frame: state.frame,
    image_a: state.a.id,
    image_b: state.b.id,
    pair_valid: state.compat?.valid_pair === true,
    category,
    observation,
    region: state.regionBox ? state.regions.find((r) => r.id === state.regionBox) : null,
    zoom: state.zoom,
    mode: state.mode,
  };
  if ($("f-shot").checked) {
    body.screenshot_png_base64 = canvas.toDataURL("image/png").split(",")[1];
    body.screenshot_caption = `${state.a.display_name} vs ${state.b.display_name} · frame ${state.frame} · ${state.mode}`;
  }
  try {
    await api(new Request("/api/findings", {
      method: "POST",
      headers: { "Content-Type": "application/json" },
      body: JSON.stringify(body),
    }));
    $("f-observation").value = "";
    setStatus("finding saved");
    await loadFindings();
  } catch (e) {
    alert("Save failed: " + e.message);
  }
}

async function exportFindings() {
  // Never export a stale startup snapshot after saving a new observation.
  // Fetch the persisted server state immediately before creating the file.
  let persisted;
  try {
    persisted = (await apiJson("/api/findings")).findings || [];
  } catch (e) {
    alert("Cannot export findings: " + e.message);
    return;
  }
  window.__findingsCache = persisted;
  renderFindings(persisted);
  const blob = new Blob([JSON.stringify({ exported_by: "ANVIL Visual Review Lab", findings: persisted }, null, 2)],
                        { type: "application/json" });
  const a = document.createElement("a");
  a.href = URL.createObjectURL(blob);
  a.download = `anvil_findings_${new Date().toISOString().replace(/[:.]/g, "-")}.json`;
  a.click();
}

// ---------------------------------------------------------------- exports

function downloadCanvas(filename) {
  const a = document.createElement("a");
  a.href = canvas.toDataURL("image/png");
  a.download = filename;
  a.click();
}

function annotateAndDownload() {
  // Compose an annotated export: current view + identity footer.
  const tmp = document.createElement("canvas");
  tmp.width = canvas.width;
  tmp.height = canvas.height + 46 * (window.devicePixelRatio || 1);
  const c = tmp.getContext("2d");
  c.fillStyle = "#0a0d0b";
  c.fillRect(0, 0, tmp.width, tmp.height);
  c.drawImage(canvas, 0, 0);
  const dpr = window.devicePixelRatio || 1;
  c.font = `${11 * dpr}px ui-monospace, monospace`;
  c.fillStyle = "#9aa69c";
  const line = `ANVIL Visual Review Lab · ${state.a?.display_name} vs ${state.b?.display_name} · `
    + `frame ${state.frame} · mode ${state.mode} · zoom ${Math.round(state.zoom * 100)}% · `
    + `pair ${state.compat?.valid_pair ? "VALID" : "INVALID"} · ${new Date().toISOString()}`;
  c.fillText(line, 8 * dpr, canvas.height + 18 * dpr);
  c.fillStyle = "#37d17a";
  c.fillText(`A: ${state.a?.id}`, 8 * dpr, canvas.height + 34 * dpr);
  c.fillStyle = "#f5a23b";
  c.fillText(`B: ${state.b?.id}`, 8 * dpr, canvas.height + 46 * dpr - 2 * dpr);
  const a = document.createElement("a");
  a.href = tmp.toDataURL("image/png");
  a.download = `anvil_compare_frame${state.frame}_${(state.b?.id || "b").replace(/[^\w.-]/g, "_")}.png`;
  a.click();
}

// ---------------------------------------------------------------- modes

const modeOptionIds = { fade: "opt-fade", flicker: "opt-flicker", diff: "opt-diff", heatmap: "opt-heat", regions: "opt-regions" };

function setMode(mode) {
  state.mode = mode;
  document.querySelectorAll("#mode-buttons .mode").forEach((b) => {
    b.classList.toggle("on", b.dataset.mode === mode);
  });
  Object.values(modeOptionIds).forEach((id) => $(id).classList.add("hidden"));
  if (modeOptionIds[mode]) $(modeOptionIds[mode]).classList.remove("hidden");
  $("slider-handle").classList.toggle("hidden", mode !== "slider");
  if (mode === "flicker" && !state.flickerPlaying) startFlicker();
  if (mode !== "flicker") stopFlicker();
  if (mode === "regions") refreshRegions();
  else { state.regions = []; renderRegionList(); }
  refreshDiff().then(render);
}

function startFlicker() {
  stopFlicker();
  state.flickerPlaying = true;
  $("flicker-play").textContent = "❚❚";
  const period = parseInt($("flicker-interval").value, 10);
  state.flickerTimer = setInterval(() => {
    state.flickerShowB = !state.flickerShowB;
    render();
  }, period);
}

function stopFlicker() {
  state.flickerPlaying = false;
  $("flicker-play").textContent = "▶";
  if (state.flickerTimer) clearInterval(state.flickerTimer);
  state.flickerTimer = null;
}

function setFrame(f, { fromScrub = false } = {}) {
  state.frame = clampFrame(f);
  if (!fromScrub) $("seq-scrub").value = state.frame;
  $("seq-frame").value = state.frame;
  updateSeqBar();
  // Pair validity and artifact SHA checks are frame-specific. Never carry a
  // green validity banner across frames without revalidation.
  refreshCompat().then(() => {
    loadImages();
    refreshRegions();
    updateMetaPanels();
  });
}

function togglePlay() {
  if (state.playing) {
    state.playing = false;
    $("seq-play").textContent = "▶";
    clearInterval(state.playTimer);
  } else {
    state.playing = true;
    $("seq-play").textContent = "❚❚";
    state.playTimer = setInterval(() => {
      const rec = state.a;
      if (!rec) return;
      const hi = rec.frames.start + rec.frames.count - 1;
      setFrame(state.frame >= hi ? rec.frames.start : state.frame + 1);
    }, 240);
  }
}

// ---------------------------------------------------------------- baseline modal

async function showBaseline() {
  try {
    const j = await apiJson("/api/baseline");
    const b = j.baseline || {};
    const v = j.verification_ok;
    $("baseline-body").innerHTML = `
      <p><b>${esc(b.name || "ANVIL baseline")}</b> — frozen ${esc(b.frozen || "?")}</p>
      <p>${esc(b.description || "")}</p>
      <p>pinned commit <b>${esc(b.pinned_commit || "?")}</b> · config hash <b>${esc((b.config_hash || "").slice(0, 24))}…</b></p>
      <p>implementation identity: <span class="${v ? "ok" : "bad"}">${v ? "VERIFIED — current worktree matches the pinned baseline" : "MISMATCH — this worktree is not the pinned baseline (fail-closed)"}</span></p>
      <p>problems: ${j.problems?.length ? `<span class="bad">${esc(j.problems.join(" · "))}</span>` : "<span class='ok'>none</span>"}</p>
      <p style="color:var(--ink-faint)">reproduction: ${esc(b.reproduction || "")}</p>`;
  } catch (e) {
    $("baseline-body").textContent = "baseline verification unavailable: " + e.message;
  }
  $("baseline-modal").showModal();
}

// ---------------------------------------------------------------- init & events

function initEvents() {
  $("sel-btn-a").onclick = (e) => { e.stopPropagation(); openSelector("a"); };
  $("sel-btn-b").onclick = (e) => { e.stopPropagation(); openSelector("b"); };
  document.addEventListener("click", (e) => {
    if (!$("selector-panel").contains(e.target)) closeSelector();
  });
  $("btn-swap").onclick = swapAB;
  $("btn-baseline").onclick = showBaseline;
  $("baseline-close").onclick = () => $("baseline-modal").close();

  document.querySelectorAll("#mode-buttons .mode").forEach((b) => {
    b.onclick = () => setMode(b.dataset.mode);
  });

  // fade
  $("fade-slider").oninput = (e) => {
    state.fade = parseInt(e.target.value, 10);
    $("fade-val").textContent = state.fade + "%";
    if (state.mode === "fade") render();
  };
  // flicker
  $("flicker-interval").oninput = (e) => {
    $("flicker-val").textContent = e.target.value + "ms";
    if (state.flickerPlaying) startFlicker();
  };
  $("flicker-play").onclick = () => (state.flickerPlaying ? stopFlicker() : startFlicker());
  $("flicker-toggle").onclick = () => { state.flickerShowB = !state.flickerShowB; render(); };
  // diff
  const diffRerender = () => {
    $("diff-gain-val").textContent = $("diff-gain").value + "×";
    refreshDiff().then(render);
  };
  $("diff-gain").oninput = diffRerender;
  $("diff-channel").onchange = diffRerender;
  $("heat-gain").oninput = () => {
    $("heat-gain-val").textContent = $("heat-gain").value + "×";
    refreshDiff().then(render);
  };
  // regions
  const regionRefresh = () => refreshRegions();
  $("region-threshold").oninput = regionRefresh;
  $("region-minsize").onchange = regionRefresh;
  $("region-merge").onchange = regionRefresh;
  $("region-show").onchange = render;

  // zoom / nav
  $("zoom-in").onclick = () => zoomBy(1.25);
  $("zoom-out").onclick = () => zoomBy(0.8);
  $("zoom-fit").onclick = () => { fitView(); render(); };
  $("zoom-100").onclick = () => { state.zoom = 1; state.panX = state.panY = 0; render(); };
  $("smoothing").onclick = () => {
    state.smoothing = !state.smoothing;
    $("smoothing").textContent = state.smoothing ? "Bilinear" : "Nearest";
    $("smoothing").classList.toggle("on", state.smoothing);
    render();
  };
  $("fullscreen").onclick = () => {
    if (document.fullscreenElement) document.exitFullscreen();
    else $("viewport-wrap").requestFullscreen();
  };
  $("align-x").oninput = (e) => { state.alignX = parseInt(e.target.value || "0", 10); updateAlignBanner(); render(); };
  $("align-y").oninput = (e) => { state.alignY = parseInt(e.target.value || "0", 10); updateAlignBanner(); render(); };
  $("align-reset").onclick = () => {
    state.alignX = state.alignY = 0;
    $("align-x").value = $("align-y").value = 0;
    updateAlignBanner(); render();
  };
  $("sync-pan").onchange = (e) => {
    state.syncPan = e.target.checked;
    if (!state.syncPan) {
      state.splitIndependent = true;
      state.splitPan = { zoom: state.zoom, x: state.panX, y: state.panY };
    } else {
      state.splitIndependent = false;
      state.splitPan = { zoom: state.zoom, x: state.panX, y: state.panY };
    }
    render();
  };

  // viewport interactions
  const vp = $("viewport");
  vp.addEventListener("wheel", (e) => {
    e.preventDefault();
    zoomBy(e.deltaY < 0 ? 1.12 : 1 / 1.12, e);
  }, { passive: false });
  let drag = null;
  vp.addEventListener("pointerdown", (e) => {
    if (e.target.closest("#slider-handle") || e.target.closest("#region-list")) return;
    const side = state.mode === "split" && state.splitIndependent
      && e.clientX >= vp.getBoundingClientRect().left + vp.clientWidth / 2 ? "b" : "a";
    drag = { x: e.clientX, y: e.clientY, side, panX: state.panX, panY: state.panY,
             splitX: state.splitPan.x, splitY: state.splitPan.y };
    vp.setPointerCapture(e.pointerId);
    vp.classList.add("dragging");
  });
  vp.addEventListener("pointermove", (e) => {
    updateHud(e);
    if (!drag) return;
    const dx = e.clientX - drag.x, dy = e.clientY - drag.y;
    if (state.mode === "split" && state.splitIndependent && drag.side === "b") {
      state.splitPan.x = drag.splitX + dx;
      state.splitPan.y = drag.splitY + dy;
    } else {
      state.panX = drag.panX + dx;
      state.panY = drag.panY + dy;
    }
    render();
  });
  const endDrag = () => { drag = null; vp.classList.remove("dragging"); };
  vp.addEventListener("pointerup", endDrag);
  vp.addEventListener("pointercancel", endDrag);

  // slider handle
  const handle = $("slider-handle");
  let sliderDrag = false;
  const moveHandle = (clientX) => {
    const r = $("viewport").getBoundingClientRect();
    state.sliderFrac = Math.max(0.02, Math.min(0.98, (clientX - r.left) / r.width));
    positionSlider();
    render();
  };
  handle.addEventListener("pointerdown", (e) => { sliderDrag = true; handle.setPointerCapture(e.pointerId); });
  handle.addEventListener("pointermove", (e) => { if (sliderDrag) moveHandle(e.clientX); });
  handle.addEventListener("pointerup", () => { sliderDrag = false; });

  // region click-to-focus: clicking near/inside a box selects it
  vp.addEventListener("click", (e) => {
    if (state.mode !== "regions" || !state.imgA) return;
    const img = state.imgA;
    const { w, h } = viewportSize();
    const dw = img.naturalWidth * state.zoom, dh = img.naturalHeight * state.zoom;
    const ix = (e.clientX - vp.getBoundingClientRect().left - (w / 2 + state.panX - dw / 2)) / state.zoom;
    const iy = (e.clientY - vp.getBoundingClientRect().top - (h / 2 + state.panY - dh / 2)) / state.zoom;
    for (const r of state.regions) {
      if (ix >= r.x0 - 6 && ix <= r.x1 + 6 && iy >= r.y0 - 6 && iy <= r.y1 + 6) {
        state.regionBox = r.id;
        render();
        return;
      }
    }
  });

  // sequence
  $("seq-prev").onclick = () => setFrame(state.frame - 1);
  $("seq-next").onclick = () => setFrame(state.frame + 1);
  $("seq-play").onclick = togglePlay;
  $("seq-scrub").oninput = (e) => setFrame(parseInt(e.target.value, 10), { fromScrub: true });
  $("seq-frame").onchange = (e) => setFrame(parseInt(e.target.value, 10));

  // findings
  $("f-save").onclick = saveFinding;
  $("f-export").onclick = exportFindings;
  $("f-import").onclick = () => $("f-import-file").click();
  $("f-import-file").onchange = async (e) => {
    const file = e.target.files[0];
    if (!file) return;
    const text = await file.text();
    try {
      const parsed = JSON.parse(text);
      const list = Array.isArray(parsed) ? parsed : parsed.findings || [];
      for (const f of list) {
        await api(new Request("/api/findings", {
          method: "POST",
          headers: { "Content-Type": "application/json" },
          body: JSON.stringify(f),
        }));
      }
      loadFindings();
      setStatus(`imported ${list.length} findings`);
    } catch (err) {
      alert("import failed: " + err.message);
    }
  };

  // exports
  $("ex-screenshot").onclick = annotateAndDownload;
  $("ex-crop").onclick = () => downloadCanvas(`anvil_crop_frame${state.frame}.png`);
  $("ex-diff").onclick = () => {
    if (!state.a || !state.b) return;
    const kind = state.mode === "heatmap" ? "heatmap" : "absdiff";
    const gain = state.mode === "heatmap" ? $("heat-gain").value : $("diff-gain").value;
    window.open(`/api/diff?id_a=${encodeURIComponent(state.a.id)}&id_b=${encodeURIComponent(state.b.id)}`
      + `&frame=${state.frame}&mode=${kind}&gain=${gain}&channel=${$("diff-channel").value}`, "_blank");
  };
  $("ex-contact").onclick = () => {
    if (!state.a || !state.b) return;
    window.open(`/api/contactsheet?id_a=${encodeURIComponent(state.a.id)}&id_b=${encodeURIComponent(state.b.id)}`, "_blank");
  };

  // keyboard
  document.addEventListener("keydown", (e) => {
    if (["INPUT", "TEXTAREA", "SELECT"].includes(document.activeElement?.tagName)) return;
    const modes = ["slider", "split", "fade", "flicker", "diff", "heatmap", "regions"];
    if (e.key >= "1" && e.key <= "7") setMode(modes[parseInt(e.key, 10) - 1]);
    else if (e.key === "[") setFrame(state.frame - 1);
    else if (e.key === "]") setFrame(state.frame + 1);
    else if (e.key === " ") { e.preventDefault(); togglePlay(); }
    else if (e.key === "+" || e.key === "=") zoomBy(1.25);
    else if (e.key === "-") zoomBy(0.8);
    else if (e.key === "0") { fitView(); render(); }
    else if (e.key.toLowerCase() === "s") swapAB();
    else if (e.key.toLowerCase() === "f") vp.requestFullscreen?.();
  });

  window.addEventListener("resize", () => { positionSlider(); render(); });
}

function zoomBy(factor, anchorEvent) {
  const vp = $("viewport");
  const r = vp.getBoundingClientRect();
  const ax = anchorEvent ? anchorEvent.clientX - r.left : r.width / 2;
  const ay = anchorEvent ? anchorEvent.clientY - r.top : r.height / 2;
  const zoomB = state.mode === "split" && state.splitIndependent
    && anchorEvent && ax >= r.width / 2;
  if (zoomB) {
    const old = state.splitPan.zoom;
    state.splitPan.zoom = Math.max(0.02, Math.min(64, old * factor));
    state.splitPan.x = ax - (ax - state.splitPan.x) * (state.splitPan.zoom / old);
    state.splitPan.y = ay - (ay - state.splitPan.y) * (state.splitPan.zoom / old);
  } else {
    const old = state.zoom;
    state.zoom = Math.max(0.02, Math.min(64, old * factor));
    state.panX = ax - (ax - state.panX) * (state.zoom / old);
    state.panY = ay - (ay - state.panY) * (state.zoom / old);
  }
  render();
}

function positionSlider() {
  const handle = $("slider-handle");
  const r = $("viewport").getBoundingClientRect();
  handle.style.left = Math.round(r.width * state.sliderFrac) + "px";
}

function updateHud(e) {
  const vp = $("viewport");
  const img = state.imgA;
  const rect = vp.getBoundingClientRect();
  const x = e.clientX - rect.left, y = e.clientY - rect.top;
  $("hud-pos").textContent = `viewport ${Math.round(x)},${Math.round(y)}`;
  if (img) {
    const { w, h } = viewportSize();
    const dw = img.naturalWidth * state.zoom, dh = img.naturalHeight * state.zoom;
    const ix = Math.floor((x - (w / 2 + state.panX - dw / 2)) / state.zoom);
    const iy = Math.floor((y - (h / 2 + state.panY - dh / 2)) / state.zoom);
    if (ix >= 0 && iy >= 0 && ix < img.naturalWidth && iy < img.naturalHeight) {
      $("hud-pos").textContent = `image ${ix},${iy} · zoom ${Math.round(state.zoom * 100)}%`;
    }
  }
}

async function init() {
  try {
    state.data = await apiJson("/api/catalog");
    window.__findingsCache = (await apiJson("/api/findings")).findings || [];
    renderFindings(window.__findingsCache);
  } catch (e) {
    document.body.innerHTML = `<div style="padding:40px;font-family:monospace;color:#ff3d2f">
      ANVIL Visual Review Lab cannot reach the local server: ${e.message}<br>
      Start it with: anvil_review_lab --root exhibitions/home_field_2026-10</div>`;
    return;
  }
  initSelectorFilters();
  defaultSelection();
  updateSelectorButtons();
  updateSeqBar();
  initEvents();
  state.fitOnLoad = true;
  await refreshCompat();
  await loadImages();
  refreshRegions();
  updateMetaPanels();
  updatePipelinePanel();
  positionSlider();
  render();
}

init();
