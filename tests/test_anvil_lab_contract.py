"""ANVIL Lab contract tests (Home Field Exhibition + Visual Review Lab).

Builds a miniature but COMPLETE exhibition in a temp directory (tiny
synthetic scenes, all arms, baseline freeze/verify, delivery, metrics,
catalog, roster) and then exercises the live Review Lab server over HTTP.

Covers the assignment's viewer/baseline regression matrix that is best
observed end-to-end:
  - control and reconstructed outputs use matched inputs;
  - the canonical baseline maps to its frozen implementation/config;
  - a falsified or moved baseline fails closed;
  - temporal reconstruction actually participates when enabled;
  - identical images produce zero pixel difference;
  - invalid pairs fail closed; missing assets never substitute silently;
  - exported findings identify the exact compared configurations;
  - roster changes never modify experiment records.
"""
import base64
import binascii
import hashlib
from concurrent.futures import ThreadPoolExecutor
from contextlib import contextmanager
import struct
import zlib
import json
import os
import pathlib
import shutil
import socket
import subprocess
import time
import urllib.error
import urllib.request

import pytest

REPO_ROOT = pathlib.Path(__file__).resolve().parent.parent

EXHIBIT_CANDIDATES = [
    os.environ.get("ANVIL_EXHIBIT"),
    str(REPO_ROOT / "build" / "anvil_exhibit"),
]
SERVER_CANDIDATES = [
    os.environ.get("ANVIL_REVIEW_LAB"),
    str(REPO_ROOT / "build" / "anvil_review_lab"),
]
FFMPEG = shutil.which("ffmpeg")


def _first_existing(candidates):
    for c in candidates:
        if c and pathlib.Path(c).is_file() and os.access(c, os.X_OK):
            return str(pathlib.Path(c).resolve())
    return None


EXHIBIT = _first_existing(EXHIBIT_CANDIDATES)
SERVER = _first_existing(SERVER_CANDIDATES)

pytestmark = pytest.mark.skipif(
    EXHIBIT is None or SERVER is None or FFMPEG is None,
    reason="anvil_exhibit / anvil_review_lab binary or ffmpeg unavailable",
)


def run_tool(*args, cwd=None, check=True):
    proc = subprocess.run([EXHIBIT, *map(str, args)],
                          capture_output=True, text=True, cwd=cwd or REPO_ROOT)
    if check and proc.returncode != 0:
        raise AssertionError(
            f"anvil_exhibit {' '.join(map(str, args))} failed "
            f"({proc.returncode}): {proc.stdout}\n{proc.stderr}")
    return proc


MINI_EXPERIMENTS = {
    "exhibition": "mini_contract",
    "created": "2026-10-09",
    "runner_binary": "build/anvil_runner",
    "targets": {"start": 2, "count": 4},
    "arms": [
        {"id": "control_decoded", "kind": "control", "mods": [],
         "runner_args": ["--past", "0", "--future", "0", "--no-accumulate"]},
        {"id": "baseline", "kind": "anvil_baseline", "mods": [],
         "runner_args": ["--past", "1", "--future", "1"]},
        {"id": "tryout_mini", "kind": "anvil_candidate",
         "mods": ["local refinement", "estimated confidence"],
         "runner_args": ["--past", "1", "--future", "1", "--refinement", "local",
                          "--confidence", "estimate"]},
    ],
    "delivery": {"filters": ["bicubic"], "factor": 2},
}


@pytest.fixture(scope="module")
def mini_exhibition(tmp_path_factory):
    """A complete miniature exhibition: scenes -> freeze -> run -> measure."""
    root = tmp_path_factory.mktemp("mini_exhibition")
    (root / "exhibitions" / "mini").parent.mkdir(parents=True, exist_ok=True)
    exp = root / "exhibitions" / "mini"
    exp.mkdir(parents=True, exist_ok=True)
    (exp / "EXPERIMENTS.json").write_text(json.dumps(MINI_EXPERIMENTS))

    run_tool("gen-scenes", "--root", exp, "--frames", "8",
             "--width", "320", "--height", "180", cwd=root)
    # Freeze the baseline against the CURRENT commit (whatever HEAD is in
    # CI); the pinned identity is that commit's blobs, so verification
    # against this checkout must succeed.
    head = subprocess.run(["git", "rev-parse", "HEAD"], cwd=REPO_ROOT,
                          capture_output=True, text=True, check=True).stdout.strip()
    # A tiny probe run supplies the canonical config + build provenance.
    probe_dir = root / "probe"
    probe_dir.mkdir()
    runner = os.environ.get("ANVIL_RUNNER") or str(REPO_ROOT / "build" / "anvil_runner")
    if not pathlib.Path(runner).is_file():
        pytest.skip("anvil_runner unavailable for the mini pipeline")
    scene_clip = exp / "artifacts" / "scenes" / "archive_grid_drift" / "input_noisy.mp4"
    subprocess.run([runner, "--input", str(scene_clip), "--output-dir", str(probe_dir),
                    "--start-frame", "2", "--frame-count", "1",
                    "--past", "1", "--future", "1"],
                   check=True, capture_output=True)
    run_tool("freeze-baseline", "--root", exp, "--commit", head,
             "--repo-root", REPO_ROOT, "--baseline-manifest",
             probe_dir / "manifest.json", cwd=root)
    run_tool("run", "--root", exp, "--runner", runner, "--repo-root", REPO_ROOT, cwd=root)
    run_tool("measure", "--root", exp, cwd=root)
    run_tool("catalog", "build", "--root", exp, cwd=root)
    return exp


# ------------------------------------------------------------ exhibit CLI

def test_mini_pipeline_produced_everything(mini_exhibition):
    for rel in ["BASELINE.json", "catalog/catalog.json", "METRICS.json"]:
        assert (mini_exhibition / rel).is_file(), rel
    manifests = list((mini_exhibition / "manifests").rglob("*.json"))
    # 2 scenes x (reference + 3 arms) = 8 run manifests
    assert len(manifests) >= 8
    catalog = json.loads((mini_exhibition / "catalog" / "catalog.json").read_text())
    ids = {r["id"] for r in catalog}
    assert "archive_grid_drift/baseline" in ids
    assert "archive_grid_drift/control_decoded" in ids
    assert any("/bicubic" in i for i in ids)


def test_baseline_verify_ok_and_falsified_fails(mini_exhibition):
    ok = run_tool("verify-baseline", "--root", mini_exhibition,
                  "--repo-root", REPO_ROOT, check=False)
    assert ok.returncode == 0, ok.stdout + ok.stderr
    # Falsified identity: corrupt the config hash -> load must fail closed.
    falsified = json.loads((mini_exhibition / "BASELINE.json").read_text())
    falsified["config_hash"] = "0" * 64
    tmp = mini_exhibition.parent / "falsified_root"
    shutil.rmtree(tmp, ignore_errors=True)
    shutil.copytree(mini_exhibition, tmp,
                    ignore=shutil.ignore_patterns("artifacts"))
    (tmp / "BASELINE.json").write_text(json.dumps(falsified))
    bad = run_tool("verify-baseline", "--root", tmp, "--repo-root", REPO_ROOT,
                   check=False)
    assert bad.returncode == 1
    # A moved baseline (implementation file changed under the tree) fails.
    moved = mini_exhibition.parent / "moved_root"
    shutil.rmtree(moved, ignore_errors=True)
    shutil.copytree(mini_exhibition, moved,
                    ignore=shutil.ignore_patterns("artifacts"))
    pinned = json.loads((mini_exhibition / "BASELINE.json").read_text())
    pinned_path = REPO_ROOT / next(iter(pinned["implementation"]["files"]))
    original = pinned_path.read_bytes()
    try:
        pinned_path.write_bytes(original + b"\n// moved by test\n")
        moved_run = run_tool("verify-baseline", "--root", moved,
                             "--repo-root", REPO_ROOT, check=False)
        assert moved_run.returncode == 1
        assert "CHANGED" in moved_run.stdout
    finally:
        pinned_path.write_bytes(original)


def test_baseline_output_requires_attested_runner_bytes(mini_exhibition):
    manifest_file = (mini_exhibition / "manifests" / "archive_grid_drift"
                     / "baseline.json")
    m = json.loads(manifest_file.read_text())
    att = m["exhibition_attestation"]
    assert att["runner_sha256"] == att["runner_sha256_after"]
    assert len(att["runner_sha256"]) == 64
    assert pathlib.Path(att["runner_path"]).is_file()

    ok = run_tool("verify-baseline", "--root", mini_exhibition,
                  "--repo-root", REPO_ROOT, "--manifest", manifest_file,
                  check=False)
    assert ok.returncode == 0, ok.stdout + ok.stderr

    for damage in ["missing", "falsified"]:
        broken = dict(m)
        if damage == "missing":
            broken.pop("exhibition_attestation")
        else:
            forged = dict(att)
            forged["runner_sha256"] = "0" * 64
            broken["exhibition_attestation"] = forged
        dest = mini_exhibition.parent / ("tampered_runner_" + damage + ".json")
        dest.write_text(json.dumps(broken))
        invalid = run_tool("verify-baseline", "--root", mini_exhibition,
                           "--repo-root", REPO_ROOT, "--manifest", dest,
                           check=False)
        assert invalid.returncode == 1
        assert "attestation" in invalid.stdout or "executable" in invalid.stdout


def test_run_manifest_proves_temporal_reconstruction(mini_exhibition):
    m = json.loads((mini_exhibition / "manifests" / "archive_grid_drift"
                    / "baseline.json").read_text())
    assert m["config"]["accumulate_enabled"] is True
    valid = sum(f["valid_samples"] for f in m["frames"])
    total = sum(f["total_samples"] for f in m["frames"])
    assert total > 0 and valid > 0, "accumulation produced zero temporal samples"
    control = json.loads((mini_exhibition / "manifests" / "archive_grid_drift"
                          / "control_decoded.json").read_text())
    assert control["config"]["accumulate_enabled"] is False
    assert sum(f["total_samples"] for f in control["frames"]) == 0


def test_catalog_naming_contracts(mini_exhibition):
    catalog = json.loads((mini_exhibition / "catalog" / "catalog.json").read_text())
    by_id = {r["id"]: r for r in catalog}
    base = by_id["archive_grid_drift/baseline"]
    assert base["display_name"] == "ANVIL baseline"
    assert base["roster_status"] == ""
    cand = by_id["archive_grid_drift/tryout_mini"]
    sibling = by_id["crossing_occluders/tryout_mini"]
    assert cand["candidate_key"] == sibling["candidate_key"], \
        "same technical method across scenes must share one roster identity"
    assert cand["display_name"] == (
        "ANVIL baseline + local refinement + estimated confidence — tryout")
    assert cand["description"].find("upscaled") == -1, \
        "same-resolution output must not be called upscaled"
    control = by_id["archive_grid_drift/control_decoded"]
    assert "temporal reconstruction" not in control["description"]
    assert "decoded-frame control" in control["description"]
    spatial = [r for r in catalog if r["display_name"].startswith("Spatial control")]
    assert spatial, "control-arm delivery must appear as an external spatial control"
    for r in spatial:
        assert not r["display_name"].startswith("ANVIL")


def test_roster_change_never_touches_experiment_records(mini_exhibition):
    catalog_path = mini_exhibition / "catalog" / "catalog.json"
    catalog = json.loads(catalog_path.read_text())
    candidate_key = next(r["candidate_key"] for r in catalog
                         if r["id"] == "archive_grid_drift/tryout_mini")
    before = catalog_path.read_bytes()
    manifests_before = {
        p: p.read_bytes()
        for p in (mini_exhibition / "manifests").rglob("*.json")
    }
    t = run_tool("roster", "set", "--root", mini_exhibition,
                 json.dumps({"candidate_id": candidate_key,
                             "to": "bench",
                             "reason": "contract-test transition",
                             "authority": "pytest",
                             "evidence_ref": "tests/test_anvil_lab_contract.py"}))
    assert t.returncode == 0
    roster = json.loads((mini_exhibition / "roster" / "roster.json").read_text())
    assert roster["entries"][candidate_key]["status"] == "bench"
    assert len(roster["history"]) == 1
    # Immutable evidence untouched.
    assert catalog_path.read_bytes() == before
    for p, b in manifests_before.items():
        assert p.read_bytes() == b
    # starter without merge evidence is refused.
    s = run_tool("roster", "set", "--root", mini_exhibition,
                 json.dumps({"candidate_id": candidate_key,
                             "to": "starter", "reason": "r", "authority": "t",
                             "evidence_ref": "e"}), check=False)
    assert s.returncode == 1
    assert "authenticated maintainer" in s.stderr
    assert "starter promotion blocked" in s.stderr


# ---------------------------------------------------------------- server

def free_port():
    s = socket.socket()
    s.bind(("127.0.0.1", 0))
    port = s.getsockname()[1]
    s.close()
    return port


@pytest.fixture(scope="module")
def lab_server(mini_exhibition):
    port = free_port()
    proc = subprocess.Popen(
        [SERVER, "--root", str(mini_exhibition), "--port", str(port),
         "--web-dir", str(REPO_ROOT / "tools" / "anvil_lab" / "web")],
        cwd=REPO_ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    base = f"http://127.0.0.1:{port}"
    for _ in range(80):
        try:
            urllib.request.urlopen(base + "/api/catalog", timeout=1)
            break
        except Exception:
            time.sleep(0.25)
    else:
        proc.terminate()
        pytest.fail("review lab server did not come up")
    yield base, mini_exhibition
    try:
        urllib.request.urlopen(base + "/api/shutdown", timeout=2)
    except Exception:
        pass
    proc.terminate()
    proc.wait(timeout=5)


def get(base, path, expect=200):
    req = urllib.request.Request(base + path)
    with urllib.request.urlopen(req, timeout=30) as r:
        body = r.read()
        assert r.status == expect, f"{path} -> {r.status}"
        return r, body



def post_headers(base):
    _, raw = get(base, "/api/catalog")
    token = json.loads(raw)["csrf_token"]
    assert len(token) >= 32
    return {"Content-Type": "application/json",
            "Origin": base, "X-Anvil-CSRF": token}


def test_serves_catalog_and_web_ui(lab_server):
    base, _ = lab_server
    _, body = get(base, "/api/catalog")
    d = json.loads(body)
    assert len(d["catalog"]) >= 8
    _, html = get(base, "/")
    assert b"ANVIL" in html and b"Visual Review Lab" in html
    _, css = get(base, "/static/lab.css")
    assert b"--accent" in css


def test_image_derivatives_are_hash_linked(lab_server):
    base, _ = lab_server
    r, png = get(base, "/api/image?id=archive_grid_drift/baseline&frame=3")
    assert png[:8] == b"\x89PNG\r\n\x1a\n"
    assert r.headers.get("X-Anvil-Original-Sha256")
    assert r.headers.get("X-Anvil-Derivative-Sha256")
    # Missing frame -> 404 with a JSON error, never a substitute.
    try:
        urllib.request.urlopen(base + "/api/image?id=archive_grid_drift/baseline&frame=99")
        assert False, "expected 404"
    except urllib.error.HTTPError as e:
        assert e.code == 404
        assert b"error" in e.read()


def test_tampered_display_cache_is_regenerated(lab_server):
    base, root = lab_server
    url = "/api/image?id=archive_grid_drift/baseline&frame=3"
    response, original_png = get(base, url)
    expected_sha = response.headers["X-Anvil-Derivative-Sha256"]
    cache = root / "artifacts" / "derivatives" / "archive_grid_drift" / "baseline" / "frame_0003.png"
    assert cache.exists()
    cache.write_bytes(original_png + b"malicious-corruption")
    try:
        response2, recovered_png = get(base, url)
        assert recovered_png == original_png, "cache must rebuild from verified PNM"
        assert response2.headers["X-Anvil-Derivative-Sha256"] == expected_sha
        assert cache.read_bytes() == original_png
    finally:
        if cache.exists() and cache.read_bytes() != original_png:
            cache.write_bytes(original_png)


def test_delivery_and_hr_reference_have_verified_timestamps(lab_server):
    base, _ = lab_server
    _, raw = get(base, "/api/catalog")
    by_id = {x["id"]: x for x in json.loads(raw)["catalog"]}
    native = by_id["archive_grid_drift/baseline"]
    delivered = by_id["archive_grid_drift/baseline/bicubic"]
    hr = by_id["archive_grid_drift/hr_master"]
    assert native["frames"]["index"]
    assert delivered["frames"]["index"] == native["frames"]["index"]
    assert hr["frames"]["index"]
    _, raw = get(base, "/api/compat?id_a=archive_grid_drift/baseline/bicubic"
                 "&id_b=archive_grid_drift/hr_master&frame=3")
    pair = json.loads(raw)
    assert pair["valid_pair"], pair["problems"]


def test_pair_rejects_missing_or_wrong_timestamps(lab_server):
    _, root = lab_server
    catalog_file = root / "catalog" / "catalog.json"
    original = catalog_file.read_bytes()
    catalog = json.loads(original)
    rec = next(x for x in catalog if x["id"] == "archive_grid_drift/baseline/bicubic")
    index = next(x for x in rec["frames"]["index"] if x["frame"] == 3)
    index["pts_ticks"] += 1
    catalog_file.write_text(json.dumps(catalog))
    port = free_port()
    proc = subprocess.Popen(
        [SERVER, "--root", str(root), "--port", str(port),
         "--web-dir", str(REPO_ROOT / "tools" / "anvil_lab" / "web")],
        cwd=REPO_ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    try:
        url = f"http://127.0.0.1:{port}"
        for _ in range(80):
            try:
                _, raw = get(url, "/api/compat?id_a=archive_grid_drift/baseline/bicubic"
                             "&id_b=archive_grid_drift/hr_master&frame=3")
                break
            except (urllib.error.URLError, ConnectionError):
                time.sleep(0.25)
        else:
            pytest.fail("new review server did not start")
        pair = json.loads(raw)
        assert pair["valid_pair"] is False
        assert any("timestamp" in p for p in pair["problems"])
    finally:
        catalog_file.write_bytes(original)
        proc.terminate()
        proc.wait(timeout=5)


def test_identical_images_zero_difference(lab_server):
    base, _ = lab_server
    r, _ = get(base, "/api/diff?id_a=archive_grid_drift/baseline"
               "&id_b=archive_grid_drift/baseline&frame=3&mode=absdiff&gain=1")
    stats = json.loads(r.headers["X-Anvil-Stats"])
    assert stats["identical"] is True
    assert stats["nonzero_pixels"] == 0
    assert stats["mean_abs"] == 0.0


def test_invalid_pair_fails_closed(lab_server):
    base, _ = lab_server
    _, body = get(base, "/api/compat?id_a=archive_grid_drift/baseline"
                   "&id_b=crossing_occluders/baseline")
    d = json.loads(body)
    assert d["valid_pair"] is False
    assert any("scene" in p for p in d["problems"])


def test_pipeline_diff_is_candidate_minus_baseline_in_both_select_orders(lab_server):
    base, _ = lab_server
    for left, right in [("baseline", "tryout_mini"),
                        ("tryout_mini", "baseline")]:
        _, body = get(base, "/api/compat?id_a=archive_grid_drift/" + left
                      + "&id_b=archive_grid_drift/" + right + "&frame=3")
        result = json.loads(body)
        assert result["valid_pair"], result["problems"]
        changed = result["pipeline_diff"]["changed"]
        by_stage = {c["stage"]: c for c in changed}
        refinement = by_stage["correspondence_refinement"]
        assert "reference: none" in refinement["detail"], refinement
        assert "this configuration: local" in refinement["detail"], refinement
        confidence = by_stage["confidence"]
        assert "reference: unit" in confidence["detail"], confidence
        assert "this configuration: estimate" in confidence["detail"], confidence


def test_actual_catalog_has_numeric_temporal_windows(mini_exhibition):
    records = json.loads((mini_exhibition / "catalog" / "catalog.json").read_text())
    by_id = {r["id"]: r for r in records}
    def window(record):
        return next(stage for stage in record["pipeline"]["stages"]
                    if stage["name"] == "window_select")
    baseline = window(by_id["archive_grid_drift/baseline"])
    control = window(by_id["archive_grid_drift/control_decoded"])
    assert baseline["mode"] == "past 1 / future 1"
    assert control["mode"] == "past 0 / future 0"
    assert baseline["active"] is True
    assert control["active"] is False


def test_clean_reference_pair_is_scientifically_valid(lab_server):
    base, _ = lab_server
    _, body = get(base, "/api/compat?id_a=archive_grid_drift/baseline"
                   "&id_b=archive_grid_drift/reference_clean&frame=3")
    pair = json.loads(body)
    assert pair["valid_pair"] is True, pair["problems"]
    assert pair["warnings"], "clean/noisy reference lineage should be disclosed"


def test_selected_frame_hash_mismatch_is_rejected(lab_server):
    base, root = lab_server
    catalog = json.loads((root / "catalog" / "catalog.json").read_text())
    candidate = next(x for x in catalog
                     if x["id"] == "archive_grid_drift/baseline")
    filename = "frame_3.ppm"
    img = root / candidate["frames"]["dir"] / filename
    original = img.read_bytes()
    try:
        img.write_bytes(original + b"tampered")
        _, body = get(base, "/api/compat?id_a=archive_grid_drift/baseline"
                       "&id_b=archive_grid_drift/control_decoded&frame=3")
        pair = json.loads(body)
        assert pair["valid_pair"] is False
        assert any("SHA-256" in x for x in pair["problems"])
        try:
            urllib.request.urlopen(base + "/api/image?id=archive_grid_drift/baseline&frame=3")
            assert False, "tampered source must not receive a validated derivative"
        except urllib.error.HTTPError as e:
            assert e.code in (404, 409)
    finally:
        img.write_bytes(original)


def test_findings_roundtrip_identifies_exact_configs(lab_server):
    base, _ = lab_server
    finding = {
        "scene_id": "archive_grid_drift",
        "frame": 3,
        "image_a": "archive_grid_drift/baseline",
        "image_b": "archive_grid_drift/tryout_mini",
        "pair_valid": True,
        "category": "improvement",
        "observation": "contract-test observation",
        # 1x1 red PNG (valid padded base64).
        "screenshot_png_base64":
            "iVBORw0KGgoAAAANSUhEUgAAAAEAAAABCAYAAAAfFcSJAAAADUlEQVR4nGP4"
            "z8DwHwAFAAH/iZk9HQAAAABJRU5ErkJggg==",
    }
    req = urllib.request.Request(base + "/api/findings",
                                 data=json.dumps(finding).encode(),
                                 headers=post_headers(base))
    try:
        with urllib.request.urlopen(req, timeout=10) as r:
            assert r.status == 200
    except urllib.error.HTTPError as exc:
        pytest.fail("valid roundtrip finding unexpectedly rejected: HTTP "
                    + str(exc.code) + " " + exc.read().decode(errors="replace"))
    _, body = get(base, "/api/findings")
    findings = json.loads(body)["findings"]
    match = [f for f in findings if f["observation"] == "contract-test observation"]
    assert match, "finding not persisted"
    f = match[-1]
    assert f["image_a"] == "archive_grid_drift/baseline"
    assert f["image_b"] == "archive_grid_drift/tryout_mini"
    assert f["screenshot_sha256"]
    # Unknown category rejected.
    bad = dict(finding, category="nonsense")
    req = urllib.request.Request(base + "/api/findings",
                                 data=json.dumps(bad).encode(),
                                 headers=post_headers(base))
    try:
        urllib.request.urlopen(req, timeout=10)
        assert False, "expected 400"
    except urllib.error.HTTPError as e:
        assert e.code == 400


def test_findings_reject_forged_source_and_override_pair_claim(lab_server):
    base, _ = lab_server

    def submit(data):
        req = urllib.request.Request(
            base + "/api/findings", data=json.dumps(data).encode(),
            headers=post_headers(base))
        return urllib.request.urlopen(req, timeout=10)

    valid = {
        "scene_id": "archive_grid_drift", "frame": 3,
        "image_a": "archive_grid_drift/baseline",
        "image_b": "archive_grid_drift/tryout_mini",
        "pair_valid": False,
        "category": "uncertain_needs_investigation",
        "observation": "forged-client-pair-claim",
        "image_a_sha256": "not-a-real-hash",
        "roster_status_at_review": "starter",
    }
    try:
        with submit(valid) as response:
            assert response.status == 200
    except urllib.error.HTTPError as exc:
        pytest.fail("otherwise valid finding refused: HTTP "
                    + str(exc.code) + ": " + exc.read().decode(errors="replace"))
    _, body = get(base, "/api/findings")
    saved = next(x for x in json.loads(body)["findings"]
                 if x["observation"] == "forged-client-pair-claim")
    assert saved["pair_valid"] is True, saved["pair_validation"]["problems"]
    assert len(saved["image_a_sha256"]) == 64
    assert saved["image_a_sha256"] != "not-a-real-hash"
    roster = json.loads((lab_server[1] / "roster" / "roster.json").read_text())
    catalog = json.loads((lab_server[1] / "catalog" / "catalog.json").read_text())
    actual_key = next(r["candidate_key"] for r in catalog
                      if r["id"] == "archive_grid_drift/tryout_mini")
    assert saved["roster_status_at_review"] == roster["entries"][actual_key]["status"]
    assert saved["roster_status_at_review"] != "starter", (
        "forged client status must not override live roster disposition")

    for variant in [
        dict(valid, image_a="invalid/id"),
        dict(valid, scene_id="wrong-scene"),
        dict(valid, frame=999999),
        dict(valid, frame="not-a-frame"),
    ]:
        try:
            with submit(variant):
                assert False, "invalid finding must be rejected"
        except urllib.error.HTTPError as exc:
            assert exc.code in (400, 409), (variant, exc.code)


def test_baseline_endpoint_verifies(lab_server):
    base, root = lab_server
    _, body = get(base, "/api/baseline")
    d = json.loads(body)
    assert d["verification_ok"] is True, d["problems"]
    assert d["baseline"]["name"] == "ANVIL baseline"


def test_contact_sheet(lab_server):
    base, _ = lab_server
    _, png = get(base, "/api/contactsheet?id_a=archive_grid_drift/baseline"
                 "&id_b=archive_grid_drift/tryout_mini&cols=3&thumb_w=160")
    assert png[:8] == b"\x89PNG\r\n\x1a\n"


def test_duplicate_json_keys_cannot_forge_provenance(lab_server):
    base, root = lab_server
    # Raw body bypasses Python dict de-duplication; both top-level and
    # nested collisions must be rejected before any findings are saved.
    for trailing in [
        '"pair_valid":true,"pair_valid":false',
        '"roster_status_at_review":"rookie","roster_status_at_review":"starter"',
        '"context":{"scene":"real","scene":"forged"}',
    ]:
        body = ('{"scene_id":"archive_grid_drift","frame":3,'
                '"image_a":"archive_grid_drift/baseline",'
                '"image_b":"archive_grid_drift/tryout_mini",'
                '"category":"improvement","observation":"duplicate-attack",'
                + trailing + '}').encode()
        req = urllib.request.Request(base + "/api/findings", data=body,
                                     headers=post_headers(base))
        with pytest.raises(urllib.error.HTTPError) as exc:
            urllib.request.urlopen(req, timeout=10)
        assert exc.value.code == 400
    _, raw = get(base, "/api/findings")
    assert all(x.get("observation") != "duplicate-attack"
               for x in json.loads(raw)["findings"])


def test_http_post_body_split_across_reads_preserves_headers(lab_server):
    # Reproduce the old parser bug: a partial POST body caused parseRequest()
    # to revisit wire headers and reject them as duplicates in req.headers.
    base, _ = lab_server
    port = int(base.rsplit(":", 1)[1])
    token = post_headers(base)["X-Anvil-CSRF"]
    finding = {"scene_id": "archive_grid_drift", "frame": 3,
               "image_a": "archive_grid_drift/baseline",
               "image_b": "archive_grid_drift/tryout_mini",
               "category": "improvement",
               "observation": "split-body-http-regression"}
    body = json.dumps(finding).encode()
    header = (
        "POST /api/findings HTTP/1.1\r\n"
        f"Host: 127.0.0.1:{port}\r\n"
        f"Origin: {base}\r\n"
        f"X-Anvil-CSRF: {token}\r\n"
        "Content-Type: application/json\r\n"
        f"Content-Length: {len(body)}\r\n"
        "Connection: close\r\n\r\n"
    ).encode()
    with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
        sock.settimeout(5)
        sock.sendall(header + body[:5])
        time.sleep(0.1)  # force the server to parse before the body finishes
        sock.sendall(body[5:])
        response = sock.recv(4096)
    assert response.startswith(b"HTTP/1.1 200"), response[:200]
    _, raw = get(base, "/api/findings")
    saved = [x for x in json.loads(raw)["findings"]
             if x.get("observation") == "split-body-http-regression"]
    assert len(saved) == 1


def test_http_content_length_malformed_requests_survive(lab_server):
    base, _ = lab_server
    port = int(base.rsplit(":", 1)[1])
    requests = [
        b"POST /api/findings HTTP/1.1\r\nHost: 127.0.0.1\r\n"
        + b"Content-Length: " + b"9" * 200 + b"\r\n\r\n",
        b"POST /api/findings HTTP/1.1\r\nHost: localhost\r\n"
        b"Content-Length: 1\r\ncontent-length: 2\r\n\r\n",
        b"POST /api/findings HTTP/1.1\r\nHost: localhost\r\n"
        b"Content-Length: bananas\r\n\r\n",
    ]
    for request in requests:
        with socket.create_connection(("127.0.0.1", port), timeout=5) as sock:
            sock.settimeout(5)
            sock.sendall(request)
            response = sock.recv(8192)
        assert b"400" in response or b"413" in response, response[:200]
        _, raw = get(base, "/api/catalog")
        assert json.loads(raw)["catalog"], "server must survive malformed requests"


@contextmanager
def isolated_review_lab(root):
    port = free_port()
    proc = subprocess.Popen(
        [SERVER, "--root", str(root), "--port", str(port),
         "--repo-root", str(REPO_ROOT),
         "--web-dir", str(REPO_ROOT / "tools" / "anvil_lab" / "web")],
        cwd=REPO_ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    base=f"http://127.0.0.1:{port}"
    try:
        for _ in range(100):
            try:
                get(base,"/api/catalog")
                break
            except (urllib.error.URLError, ConnectionError):
                time.sleep(0.1)
        else:
            pytest.fail("isolated review lab failed to start")
        yield base
    finally:
        proc.terminate()
        proc.wait(timeout=8)


def test_screenshot_optional_fresh_findings_dir(tmp_path,mini_exhibition):
    isolated=tmp_path/"lab"
    shutil.copytree(mini_exhibition,isolated)
    shutil.rmtree(isolated/"findings",ignore_errors=True)
    finding={"scene_id":"archive_grid_drift","frame":3,
        "image_a":"archive_grid_drift/baseline",
        "image_b":"archive_grid_drift/tryout_mini",
        "category":"improvement","observation":"first-no-screenshot"}
    with isolated_review_lab(isolated) as base:
        req=urllib.request.Request(base+"/api/findings",
            data=json.dumps(finding).encode(),headers=post_headers(base))
        with urllib.request.urlopen(req,timeout=10) as response:
            assert response.status==200
        assert (isolated/"findings"/"findings.json").exists()
    with isolated_review_lab(isolated) as base:
        _,raw=get(base,"/api/findings")
        assert any(x["observation"]=="first-no-screenshot"
                   for x in json.loads(raw)["findings"])
        finding["observation"]="second-with-screenshot"
        finding["screenshot_png_base64"]=base64.b64encode(png_bytes()).decode()
        req=urllib.request.Request(base+"/api/findings",
            data=json.dumps(finding).encode(),headers=post_headers(base))
        with urllib.request.urlopen(req,timeout=10) as response:
            assert response.status==200


def test_canonical_baseline_run_rejects_forged_config_with_matching_catalog(
        tmp_path, mini_exhibition):
    isolated=tmp_path/"baseline-forgery"
    shutil.copytree(mini_exhibition,isolated)
    manifest_path=(isolated/"artifacts"/"runs"/"archive_grid_drift"
                  /"baseline"/"native"/"manifest.json")
    catalog_path=isolated/"catalog"/"catalog.json"
    with isolated_review_lab(isolated) as base:
        _,raw=get(base,"/api/compat?id_a=archive_grid_drift/baseline"
                    "&id_b=archive_grid_drift/tryout_mini&frame=3")
        original=json.loads(raw)
        # May be unqualified due to a separate provenance limitation, but
        # it must never fail because it mismatches the frozen config.
        assert not any("run configuration hash" in x
                       for x in original.get("evidence_problems",[]))
    manifest=json.loads(manifest_path.read_text())
    manifest["config"]["past"]=7
    manifest_path.write_text(json.dumps(manifest))
    names=["past","future","side_info_normalization_mode","correspondence_mode",
       "refinement_mode","visibility_mode","confidence_mode","geometry_mode",
       "accumulate_enabled","color_convert_enabled","forced_cut_frames",
       "auto_scene_cut","auto_cut_threshold","excluded_neighbors","seed",
       "output_backend"]
    semantic={k:manifest["config"][k] for k in names if k in manifest["config"]}
    altered_hash=hashlib.sha256(json.dumps(semantic,indent=2).encode()).hexdigest()
    catalog=json.loads(catalog_path.read_text())
    target=next(x for x in catalog if x["id"]=="archive_grid_drift/baseline")
    target["identity"]["config_hash"]=altered_hash
    target["identity"]["manifest_sha256"]=hashlib.sha256(manifest_path.read_bytes()).hexdigest()
    catalog_path.write_text(json.dumps(catalog))
    with isolated_review_lab(isolated) as base:
        _,raw=get(base,"/api/compat?id_a=archive_grid_drift/baseline"
                    "&id_b=archive_grid_drift/tryout_mini&frame=3")
        pair=json.loads(raw)
        assert pair["pixel_pair_valid"] is True
        assert pair["qualified_experiment"] is False
        assert any("immutable baseline" in x for x in pair["evidence_problems"])


def test_bounded_http_workers_survive_connection_bursts(lab_server):
    base, _ = lab_server
    port = int(base.rsplit(":", 1)[1])
    request = (b"GET /api/catalog HTTP/1.1\r\nHost: 127.0.0.1:"
               + str(port).encode()
               + b"\r\nConnection: close\r\n\r\n")
    def client(_):
        try:
            with socket.create_connection(("127.0.0.1", port), timeout=3) as conn:
                conn.settimeout(5)
                conn.sendall(request)
                data=conn.recv(1000)
                return b"200" in data or b"503" in data
        except (ConnectionError, TimeoutError, OSError):
            return False
    for _ in range(125):
        assert client(0), "sequential connection should never crash the server"
    with ThreadPoolExecutor(max_workers=40) as pool:
        results=list(pool.map(client,range(80)))
    assert sum(results)>=40, "worker pool must handle or reject bursts explicitly"
    _, body=get(base,"/api/catalog")
    assert json.loads(body)["catalog"], "server must remain alive"


def test_cross_origin_findings_writes_rejected(lab_server):
    base, root = lab_server
    finding = {"scene_id": "archive_grid_drift", "frame": 3,
               "image_a": "archive_grid_drift/baseline",
               "image_b": "archive_grid_drift/tryout_mini",
               "category": "improvement", "observation": "cross-site-injection"}
    valid = post_headers(base)
    for headers in [
        {"Content-Type": "text/plain", "Origin": "https://attacker.example"},
        {"Content-Type": "application/json", "Origin": "https://attacker.example",
         "X-Anvil-CSRF": valid["X-Anvil-CSRF"]},
        {"Content-Type": "application/json", "X-Anvil-CSRF": valid["X-Anvil-CSRF"]},
        {"Content-Type": "application/json", "Origin": base, "X-Anvil-CSRF": "forged"},
        {"Content-Type": "application/json", "Origin": "http://evil.example:8787",
         "Host": "evil.example:8787", "X-Anvil-CSRF": valid["X-Anvil-CSRF"]},
    ]:
        req = urllib.request.Request(base + "/api/findings",
                data=json.dumps(finding).encode(), headers=headers)
        try:
            urllib.request.urlopen(req, timeout=10)
            assert False, "cross-origin or untrusted POST must not be accepted"
        except urllib.error.HTTPError as err:
            assert err.code in (400, 403), (headers, err.code)
    _, body = get(base, "/api/findings")
    assert not any(x.get("observation") == "cross-site-injection"
                   for x in json.loads(body)["findings"])


def png_bytes(w=1, h=1, pixels=None):
    if pixels is None: pixels = b"\x00" + b"\xff\x00\x00\xff" * w
    image = pixels * h
    def chunk(tag, data):
        return (struct.pack(">I", len(data)) + tag + data
                + struct.pack(">I", binascii.crc32(tag + data) & 0xffffffff))
    return (b"\x89PNG\r\n\x1a\n"
            + chunk(b"IHDR", struct.pack(">IIBBBBB", w,h,8,6,0,0,0))
            + chunk(b"IDAT", zlib.compress(image))
            + chunk(b"IEND", b""))


def test_findings_validate_screenshot_png_and_padding(lab_server):
    base, root = lab_server
    valid = post_headers(base)
    sample = {"scene_id": "archive_grid_drift", "frame": 3,
              "image_a": "archive_grid_drift/baseline",
              "image_b": "archive_grid_drift/tryout_mini",
              "category": "improvement", "observation": "screenshot-png-contract"}
    bad_pngs = [
       base64.b64encode(b"arbitrary bytes not a real PNG").decode(),
       base64.b64encode(png_bytes()[:-8]).decode(),  # missing IEND
       base64.b64encode(png_bytes(w=9000)).decode(), # geometry too big
       base64.b64encode(png_bytes()[:-1] + b"x").decode(), # CRC mismatch
       base64.b64encode(png_bytes()).decode()[:-4] + "====", # extra padding
       base64.b64encode(png_bytes()).decode()[:12] + "="
          + base64.b64encode(png_bytes()).decode()[13:], # inner padding
    ]
    for bad in bad_pngs:
        payload=dict(sample,screenshot_png_base64=bad)
        req=urllib.request.Request(base+"/api/findings",
                data=json.dumps(payload).encode(),headers=valid)
        try:
            urllib.request.urlopen(req,timeout=10)
            assert False, "invalid PNG content/padding must be rejected"
        except urllib.error.HTTPError as err:
            assert err.code == 400, err.code
    good=base64.b64encode(png_bytes()).decode()
    req=urllib.request.Request(base+"/api/findings",
          data=json.dumps(dict(sample,screenshot_png_base64=good)).encode(),
          headers=valid)
    try:
        with urllib.request.urlopen(req,timeout=10) as result:
            assert result.status == 200
    except urllib.error.HTTPError as exc:
        pytest.fail("valid, CRC-checked PNG unexpectedly rejected: HTTP "
                    + str(exc.code) + " " + exc.read().decode(errors="replace"))
    _,raw=get(base,"/api/findings")
    saved=next(x for x in json.loads(raw)["findings"]
               if x["observation"] == "screenshot-png-contract")
    shot=root/saved["screenshot_file"]
    assert shot.read_bytes()==png_bytes()


def test_no_browser_get_shutdown(lab_server):
    base, _ = lab_server
    try:
        urllib.request.urlopen(base + "/api/shutdown", timeout=10)
        assert False, "GET must never control the server lifecycle"
    except urllib.error.HTTPError as exc:
        assert exc.code == 404
    _, raw = get(base, "/api/catalog")
    assert json.loads(raw)["catalog"]


def test_scientific_qualification_separated_from_pixel_pair(lab_server):
    base, root = lab_server
    uri = ("/api/compat?id_a=archive_grid_drift/baseline"
           "&id_b=archive_grid_drift/tryout_mini&frame=3")
    _, raw = get(base, uri)
    initial = json.loads(raw)
    assert initial["pixel_pair_valid"] is True
    assert initial["experiment_eligible"] is True
    manifest = (root / "artifacts" / "runs" / "archive_grid_drift"
                / "baseline" / "native" / "manifest.json")
    original = manifest.read_bytes()
    try:
        modified = json.loads(original)
        modified.pop("exhibition_attestation", None)
        manifest.write_text(json.dumps(modified))
        _, raw = get(base, uri)
        result = json.loads(raw)
        assert result["pixel_pair_valid"] is True
        assert result["experiment_eligible"] is True
        assert result["qualified_experiment"] is False
        assert result["evidence_problems"]
    finally:
        manifest.write_bytes(original)


def test_reference_self_and_cross_scene_experiment_eligibility(lab_server):
    base, _ = lab_server
    for pair in [
        ("archive_grid_drift/reference_clean", "archive_grid_drift/reference_clean"),
        ("archive_grid_drift/baseline", "archive_grid_drift/baseline"),
    ]:
        _, raw = get(base, "/api/compat?id_a=" + pair[0]
                     + "&id_b=" + pair[1] + "&frame=3")
        d = json.loads(raw)
        assert d["pixel_pair_valid"] is True
        assert d["experiment_eligible"] is False
        assert d["qualified_experiment"] is False
        assert d["valid_pair"] is False
    _, raw = get(base, "/api/compat?id_a=archive_grid_drift/baseline"
                 "&id_b=archive_grid_drift/reference_clean&frame=3")
    pair = json.loads(raw)
    assert pair["experiment_eligible"] is True
    # Cross-scene input can produce same-size PNG but must never become a
    # scientific metric regardless of original-pixel diff availability.
    res, _ = get(base, "/api/diff?id_a=archive_grid_drift/baseline"
                 "&id_b=crossing_occluders/baseline&frame=3&mode=absdiff")
    stats = json.loads(res.headers["X-Anvil-Stats"])
    assert stats["qualified_experiment"] is False
    assert "EXPLORATORY" in stats["source"]
    assert stats["qualification_reason"]


def test_nonbaseline_comparisons_report_direction(lab_server):
    base, _ = lab_server
    _, raw = get(base, "/api/compat?id_a=archive_grid_drift/tryout_mini"
                 "&id_b=archive_grid_drift/control_decoded&frame=3")
    comparison = json.loads(raw)
    assert comparison["pipeline_diff_direction"] == "IMAGE B minus IMAGE A"
    assert comparison["pipeline_diff"]["changed"]
    assert comparison["pipeline_diff"]["unchanged"]


def test_unknown_paths_404(lab_server):
    base, _ = lab_server
    for path in ["/api/nope", "/static/../../etc/passwd", "/static/secret.txt"]:
        try:
            urllib.request.urlopen(base + path, timeout=5)
            assert False, f"{path} should 404"
        except urllib.error.HTTPError as e:
            assert e.code == 404


def test_catalog_only_checkout_reports_missing_evidence(mini_exhibition,
                                                         tmp_path):
    """A clean checkout (catalog tracked, artifacts absent) must boot, say
    so explicitly via /api/catalog, and never serve or substitute imagery."""
    stripped = tmp_path / "catalog_only"
    shutil.copytree(mini_exhibition, stripped,
                    ignore=shutil.ignore_patterns("artifacts"))
    port = free_port()
    proc = subprocess.Popen(
        [SERVER, "--root", str(stripped), "--port", str(port),
         "--web-dir", str(REPO_ROOT / "tools" / "anvil_lab" / "web")],
        cwd=REPO_ROOT, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    base = f"http://127.0.0.1:{port}"
    try:
        for _ in range(80):
            try:
                urllib.request.urlopen(base + "/api/catalog", timeout=1)
                break
            except Exception:
                time.sleep(0.25)
        else:
            pytest.fail("catalog-only server did not come up")
        _, body = get(base, "/api/catalog")
        d = json.loads(body)
        assert d["evidence_present"] is False
        assert "regenerated" in d["missing_evidence_note"]
        assert "SOURCES.md" in d["missing_evidence_note"]
        try:
            urllib.request.urlopen(
                base + "/api/image?id=archive_grid_drift/baseline&frame=3",
                timeout=5)
            assert False, "expected 404 for absent evidence image"
        except urllib.error.HTTPError as e:
            assert e.code == 404
    finally:
        try:
            urllib.request.urlopen(base + "/api/shutdown", timeout=2)
        except Exception:
            pass
        proc.terminate()
        proc.wait(timeout=5)
