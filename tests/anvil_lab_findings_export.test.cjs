// Regression: the exported JSON must reflect the server's latest persisted
// findings, not the cache that was loaded when the page first opened.
// Pure Node test: no browser dependencies and no generated media required.
"use strict";
const { test } = require("node:test");
const assert = require("node:assert/strict");
const fs = require("node:fs");

test("exportFindings reads the persisted server state after a save", async () => {
  const src = fs.readFileSync("tools/anvil_lab/web/lab.js", "utf8");
  const start = src.indexOf("async function exportFindings()");
  const stop = src.indexOf("// ---------------------------------------------------------------- exports", start);
  assert.ok(start >= 0 && stop > start, "actual export function must exist");
  let calls = 0;
  const persisted = [{ observation: "old" }, { observation: "just saved" }];
  const apiJson = async (path) => {
    assert.equal(path, "/api/findings");
    calls++;
    return { findings: persisted };
  };
  let visible;
  const renderFindings = (list) => { visible = list; };
  const window = { __findingsCache: [{ observation: "old" }] };
  let exported;
  const URL = {
    createObjectURL(blob) { exported = blob; return "blob:unit-test"; },
  };
  let clicked = false;
  const document = {
    createElement(tag) {
      assert.equal(tag, "a");
      return {
        set href(value) { assert.equal(value, "blob:unit-test"); },
        set download(value) { assert.match(value, /^anvil_findings_/); },
        click() { clicked = true; },
      };
    },
  };
  const factory = new Function(
    "apiJson", "renderFindings", "window", "document", "URL", "Blob", "Date",
    src.slice(start, stop) + "\nreturn exportFindings;"
  );
  const runExport = factory(
    apiJson, renderFindings, window, document, URL, Blob, Date
  );
  await runExport();
  assert.equal(calls, 1, "export must refresh server state");
  assert.equal(clicked, true);
  assert.deepEqual(visible, persisted);
  assert.deepEqual(window.__findingsCache, persisted);
  const contents = JSON.parse(await exported.text());
  assert.deepEqual(contents.findings, persisted);
});

test("reference metrics identify IMAGE B and never imply an A/B PSNR score", () => {
  const src = fs.readFileSync("tools/anvil_lab/web/lab.js", "utf8");
  const begin = src.indexOf("function updateMetricsPanel()");
  const end = src.indexOf("function fmt(", begin);
  assert.ok(begin >= 0 && end > begin);
  const grid = { innerHTML: "" };
  const $ = (id) => { assert.equal(id, "metrics-grid"); return grid; };
  const state = {
    a: { id: "scene/baseline", display_name: "ANVIL baseline" },
    b: { id: "scene/tryout_mini",
         display_name: "ANVIL baseline + refinement — tryout",
         scene: { id: "scene" }, scale: { factor: 1 } },
    compat: { valid_pair: true, qualified_experiment: true },
    data: { metrics: { rows: [{
      scene: "scene", arm: "tryout_mini",
      native: { reference: "clean LR reference",
                psnr_db_mean: 30, ssim_mean: 0.92,
                edge_diff_mean: 3, temporal_delta_mean: 2,
                reference_validity: "full-reference" },
    }] } },
  };
  const fmt = (n, suffix) => n.toFixed(2) + (suffix || "");
  const esc = (v) => String(v);
  const render = new Function("state", "$", "fmt", "esc",
    src.slice(begin, end) + "\nreturn updateMetricsPanel;");
  render(state, $, fmt, esc)();
  assert.match(grid.innerHTML, /IMAGE B vs recorded reference/);
  assert.match(grid.innerHTML, /B vs reference PSNR/);
  assert.match(grid.innerHTML, /clean LR reference/);
  assert.match(grid.innerHTML, /Difference statistics panel/);
  assert.doesNotMatch(grid.innerHTML, /A vs B PSNR/);
  const before = grid.innerHTML;
  state.a = { id: "scene/other-candidate", display_name: "other candidate" };
  render(state, $, fmt, esc)();
  assert.equal(grid.innerHTML, before, "B's reference score must not depend on IMAGE A");
  state.compat.qualified_experiment = false;
  render(state, $, fmt, esc)();
  assert.match(grid.innerHTML, /UNQUALIFIED \/ historical reference scores hidden/);
});
