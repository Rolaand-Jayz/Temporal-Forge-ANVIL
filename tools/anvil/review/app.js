"use strict";
// Same-frame ANVIL inspector. No external libraries or synthetic images.
const $ = id => document.getElementById(id);
const state = {pairs: [], selected: -1, swapped: false, mode: "wipe",
               zoom: 1, panX: 0, panY: 0, mix: 50, flicker: false,
               flickerTimer: null, drag: null, imageLoaded: 0};
const text = (node, value) => {node.textContent = String(value);};
const fmt = num => Number.isFinite(Number(num)) ? Number(num).toFixed(2) : "—";
function setEmpty(message) {
  $("viewport").classList.add("no-data");
  text($("empty").querySelector("h2"), message);
}
function applyView() {
  $("stage").style.transform = "translate(" + state.panX + "px," +
    state.panY + "px) scale(" + state.zoom + ")";
  $("stage").style.setProperty("--split", state.mix + "%");
  $("stage").style.setProperty("--fade", state.mix / 100);
  $("zoom").value = Math.round(state.zoom * 100);
  text($("zoomValue"), Math.round(state.zoom * 100) + "%");
  $("mix").value = state.mix;
  text($("mixValue"), state.mix + "%");
}
function stopFlicker() {
  if (state.flickerTimer !== null) clearInterval(state.flickerTimer);
  state.flickerTimer = null;
  $("stage").style.setProperty("--flicker", 0);
}
function setMode(mode) {
  if (!["wipe", "split", "fade", "flicker", "difference"].includes(mode)) return;
  state.mode = mode;
  $("stage").dataset.mode = mode;
  $("viewport").dataset.mode = mode;
  document.querySelectorAll(".mode-buttons button").forEach(button => {
    button.setAttribute("aria-pressed", String(button.dataset.mode === mode));
  });
  const controllable = mode === "wipe" || mode === "fade";
  $("mix").disabled = !controllable;
  text($("mixLabel"), mode === "fade" ? "B OPACITY" :
       mode === "wipe" ? "COMPARISON DIVIDER" : "COMPARE MODE");
  stopFlicker();
  if (mode === "flicker") {
    state.flickerTimer = setInterval(() => {
      state.flicker = !state.flicker;
      $("stage").style.setProperty("--flicker", state.flicker ? 1 : 0);
    }, 480);
  }
  if (mode === "difference" && state.selected >= 0) drawDifference();
}
function resetView() {
  state.zoom = 1; state.panX = 0; state.panY = 0; state.mix = 50;
  applyView();
}
function swap() {
  state.swapped = !state.swapped;
  if (state.selected >= 0) showFrame(state.selected, false);
}
function fact(label, value) {
  const row = document.createElement("div");
  const dt = document.createElement("dt"), dd = document.createElement("dd");
  text(dt, label); text(dd, value); row.append(dt, dd);
  $("facts").append(row);
}
function labelArm(node, arm, letter) {
  text(node.querySelector(".tag"), letter + " / " + arm.status.toUpperCase());
  text(node.querySelector("h3"), arm.label);
  text(node.querySelector("p"), arm.stage);
}
function drawDifference() {
  const imgA = $("imageA"), imgB = $("imageB");
  if (!imgA.complete || !imgB.complete || !imgA.naturalWidth || !imgB.naturalWidth) return;
  const w = imgA.naturalWidth, h = imgA.naturalHeight;
  if (w !== imgB.naturalWidth || h !== imgB.naturalHeight) {
    setEmpty("Dimension mismatch — evidence rejected"); return;
  }
  const canvas = $("difference"), ctx = canvas.getContext("2d", {willReadFrequently: true});
  canvas.width = w; canvas.height = h;
  ctx.drawImage(imgA, 0, 0);
  const a = ctx.getImageData(0, 0, w, h);
  ctx.clearRect(0, 0, w, h);
  ctx.drawImage(imgB, 0, 0);
  const b = ctx.getImageData(0, 0, w, h);
  const output = ctx.createImageData(w, h);
  for (let i = 0; i < a.data.length; i += 4) {
    for (let c = 0; c < 3; c++)
      output.data[i + c] = Math.min(255, Math.abs(a.data[i + c] - b.data[i + c]) * 3);
    output.data[i + 3] = 255;
  }
  ctx.putImageData(output, 0, 0);
}
function showFrame(index, reset = true) {
  if (index < 0 || index >= state.pairs.length) return;
  state.selected = index;
  const pair = state.pairs[index];
  const a = state.swapped ? pair.b : pair.a;
  const b = state.swapped ? pair.a : pair.b;
  state.imageLoaded = 0;
  $("viewport").classList.add("no-data");
  text($("empty").querySelector("h2"), "Loading authenticated image pair");
  $("frameSelect").value = pair.key;
  const onLoad = () => {
    state.imageLoaded++;
    if (state.imageLoaded === 2) {
      $("viewport").classList.remove("no-data");
      if (state.mode === "difference") drawDifference();
    }
  };
  function setImage(node, arm) {
    node.onload = onLoad;
    node.onerror = () => setEmpty("Capture image unavailable — pair rejected");
    node.src = arm.url;
    node.alt = arm.label + " — frame " + pair.frame_index;
  }
  setImage($("imageA"), a); setImage($("imageB"), b);
  text($("badgeA"), "A / " + a.label);
  text($("badgeB"), "B / " + b.label);
  labelArm($("armA"), a, "A");
  labelArm($("armB"), b, "B");
  text($("frameIdentity"), "Frame " + pair.frame_index + " · PTS " + pair.pts_us + " µs");
  text($("resolution"), pair.output_width + " × " + pair.output_height);
  $("facts").replaceChildren();
  fact("Source", pair.source_id || "Unspecified");
  fact("Frame", pair.frame_index);
  fact("Presentation", pair.output_width + " × " + pair.output_height);
  fact("Decoded input", pair.source_width + " × " + pair.source_height);
  fact("Timestamp", pair.pts_us + " µs");
  fact("Native ticks", pair.pts_ticks + " (" + pair.tb_num + "/" + pair.tb_den + ")");
  fact("ANVIL CPU stage", fmt(pair.anvil_stage_ms) + " ms");
  fact("FSR4 dispatch", fmt(pair.fsr_dispatch_ms) + " ms");
  fact("Source revision", (pair.git_sha || "Not recorded") + (pair.git_dirty === true ? " (modified source)" : ""));
  fact("Build identity", pair.provenance_source || "Not recorded");
  fact("Source modifications SHA-256", pair.git_dirty_hash || "Not recorded / clean");
  fact("Executable SHA-256", pair.binary_sha256 || "Not recorded");
  fact("Input SHA-256", pair.input_sha256 || "Not recorded");
  fact("Live profile", pair.anvil_live_profile);
  fact("Attestation", "Producer HMAC; not independent GPU proof");
  fact("A SHA-256", (a.sha256 || "").slice(0, 16) + "…");
  fact("B SHA-256", (b.sha256 || "").slice(0, 16) + "…");
  text($("evidenceLine"), "PRODUCER-KEY AUTHENTICATED · SHA-256 + complete PNG decoded · independent GPU proof pending");
  document.querySelectorAll(".frame-card").forEach((card, i) => {
    card.classList.toggle("selected", i === index);
    card.setAttribute("aria-pressed", String(i === index));
  });
  if (reset) resetView();
}
function buildFilmstrip() {
  const select = $("frameSelect"), strip = $("filmstrip");
  select.replaceChildren(); strip.replaceChildren();
  for (let i = 0; i < state.pairs.length; i++) {
    const pair = state.pairs[i];
    const option = document.createElement("option");
    option.value = pair.key;
    text(option, "Frame " + pair.frame_index + " — " + pair.pts_us + " µs");
    select.append(option);
    const button = document.createElement("button");
    button.type = "button"; button.className = "frame-card";
    button.setAttribute("aria-pressed", "false");
    const img = document.createElement("img");
    img.src = pair.b.url; img.loading = "lazy"; img.alt = "";
    const title = document.createElement("strong");
    text(title, "Frame " + pair.frame_index);
    const summary = document.createElement("small");
    text(summary, pair.output_width + "×" + pair.output_height + " · " +
                  fmt(pair.fsr_dispatch_ms) + " ms FSR");
    button.append(img, title, summary);
    button.addEventListener("click", () => showFrame(i));
    strip.append(button);
  }
  text($("sampleCount"), state.pairs.length + " authenticated frame" +
       (state.pairs.length === 1 ? "" : "s"));
}
function startDrag(event) {
  if (state.selected < 0 || event.button !== 0) return;
  state.drag = {x:event.clientX,y:event.clientY,px:state.panX,py:state.panY};
  $("viewport").setPointerCapture(event.pointerId);
  $("viewport").classList.add("dragging");
}
function moveDrag(event) {
  if (!state.drag) return;
  state.panX = state.drag.px + (event.clientX - state.drag.x);
  state.panY = state.drag.py + (event.clientY - state.drag.y);
  applyView();
}
function endDrag() { state.drag = null; $("viewport").classList.remove("dragging"); }
document.querySelectorAll(".mode-buttons button").forEach(button =>
  button.addEventListener("click", () => setMode(button.dataset.mode)));
$("swap").addEventListener("click", swap);
$("reset").addEventListener("click", resetView);
$("mix").addEventListener("input", e => {state.mix = Number(e.target.value); applyView();});
$("zoom").addEventListener("input", e => {state.zoom = Number(e.target.value)/100; applyView();});
$("frameSelect").addEventListener("change", e => {
  const index = state.pairs.findIndex(p => p.key === e.target.value);
  showFrame(index);
});
$("viewport").addEventListener("pointerdown", startDrag);
$("viewport").addEventListener("pointermove", moveDrag);
$("viewport").addEventListener("pointerup", endDrag);
$("viewport").addEventListener("pointercancel", endDrag);
$("viewport").addEventListener("wheel", e => {
  if (state.selected < 0) return;
  e.preventDefault();
  state.zoom = Math.max(1, Math.min(8, state.zoom + (e.deltaY < 0 ? .25 : -.25)));
  applyView();
}, {passive: false});
document.addEventListener("keydown", e => {
  if (["INPUT","SELECT","TEXTAREA"].includes(document.activeElement.tagName)) return;
  if (e.key >= "1" && e.key <= "5")
    setMode(["wipe","split","fade","flicker","difference"][Number(e.key)-1]);
  else if (e.key.toLowerCase() === "s") swap();
  else if (e.key === "ArrowRight") showFrame(state.selected + 1);
  else if (e.key === "ArrowLeft") showFrame(state.selected - 1);
  else return;
  e.preventDefault();
});
async function loadCatalog() {
  try {
    const response = await fetch("/api/catalog", {cache:"no-store"});
    if (!response.ok) throw new Error("HTTP " + response.status);
    const data = await response.json();
    if (!Array.isArray(data.pairs) || !Array.isArray(data.rejected))
      throw new Error("Invalid response");
    state.pairs = data.pairs;
    buildFilmstrip();
    text($("verification"), data.pairs.length + " authenticated / " +
                            data.rejected.length + " rejected");
    $("frameSelect").disabled = data.pairs.length === 0;
    if (!data.pairs.length) setEmpty("No authenticated capture pairs");
    else showFrame(0);
  } catch (err) {
    text($("verification"), "Catalog unavailable");
    setEmpty("Could not verify capture catalog");
    console.error(err);
  }
}
applyView(); setMode("wipe"); loadCatalog();