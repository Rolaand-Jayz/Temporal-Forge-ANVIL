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
