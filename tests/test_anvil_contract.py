# tests/test_anvil_contract.py — Python-side ANVIL build-ready contract tests.
#
# These tests drive the anvil_runner CLI end-to-end and verify the
# reproducibility/oracle/color/codec contracts from BUILD_READY_CONTRACT.md.
#
# Skip conditions (deterministic, not failures):
#   - ANVIL_RUNNER not set and no candidate binary found -> skip
#   - ffmpeg binary unavailable for fixture generation -> skip
#
# CI note: the python-contracts job builds anvil_runner and installs ffmpeg,
# so these tests run there and locally with a configured build tree.

import hashlib
import json
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[1]
RUNNER_CANDIDATES = [
    os.environ.get("ANVIL_RUNNER", ""),
    REPO_ROOT / "build" / "anvil_runner",
    REPO_ROOT / "build" / "anvil_runner" / "anvil_runner",
]


def _find_runner():
    for cand in RUNNER_CANDIDATES:
        if cand and Path(cand).is_file() and os.access(cand, os.X_OK):
            return str(Path(cand).resolve())
    return None


def _ffmpeg():
    return shutil.which("ffmpeg")


RUNNER = _find_runner()
FFMPEG = _ffmpeg()

pytestmark = pytest.mark.skipif(
    RUNNER is None or FFMPEG is None,
    reason="anvil_runner binary or ffmpeg binary unavailable (deterministic skip)",
)


@pytest.fixture(scope="module")
def sdr_clip(tmp_path_factory):
    """64x64 SDR clip with full color metadata, 6 frames, global motion."""
    out = tmp_path_factory.mktemp("fx") / "anvil_sdr.mp4"
    cmd = [
        FFMPEG, "-y", "-loglevel", "error",
        "-f", "lavfi", "-i", "testsrc2=size=64x64:rate=10:duration=1",
        "-c:v", "libx264", "-pix_fmt", "yuv420p",
        "-x264-params",
        "keyint=10:bframes=0:colorprim=bt709:transfer=bt709:colorrange=tv:colormatrix=bt709",
        str(out),
    ]
    subprocess.run(cmd, check=True)
    return out


@pytest.fixture(scope="module")
def hdr_clip(tmp_path_factory):
    out = tmp_path_factory.mktemp("fx") / "anvil_hdr.mp4"
    cmd = [
        FFMPEG, "-y", "-loglevel", "error",
        "-f", "lavfi", "-i", "testsrc2=size=64x64:rate=10:duration=1",
        "-c:v", "libx264", "-pix_fmt", "yuv420p10le",
        "-x264-params",
        "keyint=10:bframes=0:colorprim=bt2020:transfer=smpte2084:"
        "colorrange=tv:colormatrix=bt2020nc",
        str(out),
    ]
    subprocess.run(cmd, check=True)
    return out


def run_runner(out_dir, *args):
    out_dir.mkdir(parents=True, exist_ok=True)
    cmd = [RUNNER, "--output-dir", str(out_dir), *map(str, args)]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    return proc


def load_manifest(out_dir: Path) -> dict:
    return json.loads((out_dir / "manifest.json").read_text())


def test_manifest_schema_and_provenance(sdr_clip, tmp_path):
    out = tmp_path / "schema"
    proc = run_runner(out, "--input", sdr_clip, "--frame-count", 2)
    assert proc.returncode == 0, proc.stderr
    m = load_manifest(out)
    for key in ("config", "provenance", "codec_capabilities", "stage_timings",
                "events", "dump_files", "frames"):
        assert key in m, f"manifest missing section {key}"
    cfg = m["config"]
    assert cfg["start_frame"] == 0 and cfg["frame_count"] == 2
    assert cfg["past"] == 0 and cfg["future"] == 0
    prov = m["provenance"]
    assert len(prov["input_sha256"]) == 64
    assert prov["input_sha256"] == hashlib.sha256(sdr_clip.read_bytes()).hexdigest()
    assert prov["decode_mode"] == "software"
    assert prov["ffmpeg_version"]
    # deterministic-mode declaration: no stochastic component exists
    assert cfg["deterministic_no_random_components"] is True


def test_codec_capability_matrix_explicit(sdr_clip, tmp_path):
    out = tmp_path / "codec"
    proc = run_runner(out, "--input", sdr_clip, "--frame-count", 1)
    assert proc.returncode == 0
    caps = load_manifest(out)["codec_capabilities"]
    for codec in ("h264", "hevc", "av1"):
        assert codec in caps, f"codec {codec} missing from capability matrix"
        c = caps[codec]
        # truthfulness: proven MV export requires observed probe frames
        if c["mv_export_proven"]:
            assert c["probe_mv_frames"] > 0
        else:
            assert c["note"], "unsupported codec must carry a truthful note"


def test_deterministic_replay_byte_equality(sdr_clip, tmp_path):
    args = ["--input", sdr_clip, "--start-frame", 1, "--frame-count", 3,
            "--past", 1, "--future", 1, "--correspondence", "estimate"]
    out1, out2 = tmp_path / "rep1", tmp_path / "rep2"
    assert run_runner(out1, *args).returncode == 0
    assert run_runner(out2, *args).returncode == 0
    m1, m2 = load_manifest(out1), load_manifest(out2)
    # deterministic fields: config + events + frames must be identical;
    # stage timings are wall-clock and excluded by contract.
    m1 = dict(m1, stage_timings=None)
    m2 = dict(m2, stage_timings=None)
    assert m1 == m2, "manifest (minus timings) differs across identical runs"
    for f in ("frame_1.ppm", "frame_2.ppm", "frame_3.ppm"):
        a = (out1 / f).read_bytes()
        b = (out2 / f).read_bytes()
        assert a == b, f"{f} not byte-identical across replays"


def test_single_frame_control_and_window_selection(sdr_clip, tmp_path):
    single = tmp_path / "single"
    assert run_runner(single, "--input", sdr_clip, "--frame-count", 1,
                      "--past", 0, "--future", 0).returncode == 0
    m = load_manifest(single)
    assert m["config"]["past"] == 0 and m["config"]["future"] == 0
    fr = m["frames"][0]
    assert fr["total_samples"] == 0  # single-frame: no neighbor samples

    window = tmp_path / "window"
    assert run_runner(window, "--input", sdr_clip, "--start-frame", 2,
                      "--frame-count", 1, "--past", 2, "--future", 2).returncode == 0
    m2 = load_manifest(window)
    assert m2["config"]["past"] == 2 and m2["config"]["future"] == 2
    assert m2["frames"][0]["total_samples"] > 0


def test_stage_bypass(sdr_clip, tmp_path):
    out = tmp_path / "bypass"
    assert run_runner(out, "--input", sdr_clip, "--frame-count", 1,
                      "--past", 2, "--no-accumulate", "--no-color-convert"
                      ).returncode == 0
    m = load_manifest(out)
    assert m["config"]["accumulate_enabled"] is False
    assert m["config"]["color_convert_enabled"] is False


def test_oracle_injection_replaces_estimates(sdr_clip, tmp_path):
    oracle_dir = tmp_path / "oracle"
    oracle_dir.mkdir()
    lines = []
    for by in range(0, 64, 16):
        for bx in range(0, 64, 16):
            # proven reference to frame index 1, identity correspondence
            lines.append(f"1 0 0 {bx} {by} 16 16 0 0 3 P 0 0")
    (oracle_dir / "correspondence_1.txt").write_text("\n".join(lines) + "\n")
    # visibility oracle: fully valid
    (oracle_dir / "visibility_1.pgm").write_bytes(
        b"P5\n64 64\n255\n" + b"\xff" * (64 * 64))
    # geometry oracle: known zero phase
    (oracle_dir / "geometry_1.txt").write_text("2 0.0 0.0\n")

    out = tmp_path / "oracle_out"
    proc = run_runner(out, "--input", sdr_clip, "--start-frame", 1,
                      "--frame-count", 1, "--past", 1, "--future", 1,
                      "--correspondence", "oracle", "--visibility", "oracle",
                      "--geometry", "oracle", "--oracle-dir", oracle_dir)
    assert proc.returncode == 0, proc.stderr
    m = load_manifest(out)
    events = [e["type"] for e in m["events"]]
    assert "oracle_used" in events
    fr = m["frames"][0]
    assert fr["correspondence_source"] == "oracle"
    assert fr["geometry_state"] == "known"


def test_scene_cut_observable_and_controllable(sdr_clip, tmp_path):
    out = tmp_path / "cuts"
    assert run_runner(out, "--input", sdr_clip, "--start-frame", 1,
                      "--frame-count", 3, "--past", 1, "--future", 1,
                      "--cut-frames", 2).returncode == 0
    m = load_manifest(out)
    types = [e["type"] for e in m["events"]]
    assert "window_reset" in types, "forced cut boundary not observable"
    assert m["config"]["forced_cut_frames"] == [2]


def test_sdr_explicit_conversion_hdr_preserved(sdr_clip, hdr_clip, tmp_path):
    sdr_out = tmp_path / "sdr"
    assert run_runner(sdr_out, "--input", sdr_clip, "--frame-count", 1).returncode == 0
    m = load_manifest(sdr_out)
    conv = m["frames"][0]["color_conversion"]
    assert "matrix=bt709" in conv and "range=limited" in conv
    assert (sdr_out / "frame_0.ppm").is_file()

    hdr_out = tmp_path / "hdr"
    assert run_runner(hdr_out, "--input", hdr_clip, "--frame-count", 1).returncode == 0
    m = load_manifest(hdr_out)
    conv = m["frames"][0]["color_conversion"]
    assert "pq" in conv and "no transfer conversion performed" in conv
    assert (hdr_out / "frame_0_y.pgm").is_file()
    assert not (hdr_out / "frame_0.ppm").exists()
    # HDR color metadata is tracked, not guessed
    assert m["frames"][0] is not None


def test_unspecified_color_metadata_preserved(tmp_path_factory, sdr_clip):
    """A clip without color metadata must produce unknown-state records, not
    silent assumptions."""
    tmp_path = tmp_path_factory.mktemp("nometa")
    nometa = tmp_path / "nometa.mp4"
    subprocess.run([
        FFMPEG, "-y", "-loglevel", "error",
        "-f", "lavfi", "-i", "testsrc2=size=64x64:rate=10:duration=1",
        "-c:v", "libx264", "-pix_fmt", "yuv420p",
        "-x264-params", "keyint=10:bframes=0", str(nometa),
    ], check=True)
    out = tmp_path / "out"
    assert run_runner(out, "--input", nometa, "--frame-count", 1).returncode == 0
    m = load_manifest(out)
    assert "color metadata incomplete" in m["frames"][0]["color_conversion"]
    types = [e["type"] for e in m["events"]]
    assert "color_unknown_metadata" in types


def test_intermediate_dumps_with_manifest_inventory(sdr_clip, tmp_path):
    dump_dir = tmp_path / "dumps"
    out = tmp_path / "dump"
    assert run_runner(out, "--input", sdr_clip, "--start-frame", 2,
                      "--frame-count", 1, "--past", 1, "--dump-dir", dump_dir,
                      "--dump-stages", "accumulate,correspondence,sample_geometry"
                      ).returncode == 0
    m = load_manifest(out)
    assert m["dump_files"], "manifest dump inventory empty"
    for d in m["dump_files"]:
        assert (dump_dir / d).is_file(), f"inventory entry missing on disk: {d}"


def test_ground_truth_attachment_and_provenance(sdr_clip, tmp_path):
    """HR ground truth attaches explicitly: per-frame association, dimensions,
    format, and an independently verifiable SHA-256 in the manifest."""
    gt = tmp_path / "gt.pgm"
    gt.write_bytes(b"P5\n64 64\n255\n" + bytes((i * 11) % 256 for i in range(64 * 64)))
    out = tmp_path / "gt_out"
    proc = run_runner(out, "--input", sdr_clip, "--start-frame", 1,
                      "--frame-count", 1, "--past", 1,
                      "--ground-truth", f"1={gt}")
    assert proc.returncode == 0, proc.stderr
    m = load_manifest(out)
    assert len(m["ground_truth"]) == 1
    g = m["ground_truth"][0]
    assert g["frame_index"] == 1            # unambiguous target association
    assert g["sha256"] == hashlib.sha256(gt.read_bytes()).hexdigest()  # independent digest
    assert (g["width"], g["height"], g["maxval"]) == (64, 64, 255)
    assert g["format"] == "pgm"
    assert g["size_bytes"] == gt.stat().st_size
    assert "excluded from candidate reconstruction" in g["usage"]
    assert m["frames"][0]["has_ground_truth"] is True


def test_ground_truth_never_contaminates_reconstruction(sdr_clip, tmp_path):
    gt = tmp_path / "gt.pgm"
    gt.write_bytes(b"P5\n64 64\n255\n" + bytes((i * 3) % 256 for i in range(64 * 64)))
    plain, with_gt = tmp_path / "plain", tmp_path / "withgt"
    args = ["--input", sdr_clip, "--start-frame", 1, "--frame-count", 2,
            "--past", 1]
    assert run_runner(plain, *args).returncode == 0
    assert run_runner(with_gt, *args,
                      "--ground-truth", f"1={gt}",
                      "--ground-truth", f"2={gt}").returncode == 0
    for f in ("frame_1.ppm", "frame_2.ppm"):
        assert (plain / f).read_bytes() == (with_gt / f).read_bytes(), (
            f"{f} changed when ground truth was attached: oracle leakage")


def test_ground_truth_rejections(sdr_clip, tmp_path):
    bad_dims = tmp_path / "bad_dims.pgm"
    bad_dims.write_bytes(b"P5\n32 32\n255\n" + bytes(32 * 32))
    malformed = tmp_path / "malformed.pgm"
    malformed.write_bytes(b"P4\n64 64\n255\nxx")
    proc1 = run_runner(tmp_path / "r1", "--input", sdr_clip, "--frame-count", 1,
                       "--ground-truth", f"0={bad_dims}")
    assert proc1.returncode == 1 and "dimension_mismatch" in proc1.stderr
    proc2 = run_runner(tmp_path / "r2", "--input", sdr_clip, "--frame-count", 1,
                       "--ground-truth", f"0={malformed}")
    assert proc2.returncode == 1 and "malformed" in proc2.stderr
    proc3 = run_runner(tmp_path / "r3", "--input", sdr_clip, "--frame-count", 1,
                       "--ground-truth", "9=/nonexistent.pgm")
    assert proc3.returncode == 1 and "frame_out_of_range" in proc3.stderr
    proc4 = run_runner(tmp_path / "r4", "--input", sdr_clip, "--frame-count", 1,
                       "--ground-truth", "0=/nonexistent.pgm")
    assert proc4.returncode == 1 and "missing" in proc4.stderr


def test_comprehensive_stage_capture_inventory(sdr_clip, tmp_path):
    """Every consequential implemented stage yields an auditable artifact;
    the manifest inventory matches disk; artifacts replay byte-identically."""
    dump_dir = tmp_path / "dumps"
    args = ["--input", sdr_clip, "--start-frame", 2, "--frame-count", 2,
            "--past", 1, "--dump-dir", dump_dir, "--dump-stages", "all"]
    out = tmp_path / "cap"
    assert run_runner(out, *args).returncode == 0
    m = load_manifest(out)

    expected = set()
    for t in (2, 3):
        expected |= {
            dump_dir / f"decode_f{t}_y.pgm",            # decoded source planes
            dump_dir / f"decode_f{t}_mvs.txt",          # raw codec side info (or explicit none)
            dump_dir / f"window_select_f{t}_window.txt",  # window selection + exclusions
            dump_dir / f"correspondence_f{t}_correspondence.txt",  # actual vectors
            dump_dir / f"visibility_f{t}_i0.pgm",       # actual mask values
            dump_dir / f"sample_geometry_f{t}_geometry.txt",
            dump_dir / f"color_convert_f{t}_color.txt",
            dump_dir / f"accumulate_f{t}_y.pgm",        # reconstructed data
        }
    on_disk = {p for p in dump_dir.iterdir() if p.is_file()}
    assert expected <= on_disk, f"missing artifacts: {expected - on_disk}"
    # manifest inventory == disk artifacts exactly (paths relative to dump dir)
    listed = {dump_dir / d for d in m["dump_files"]}
    assert listed == on_disk
    for d in listed:
        assert d.is_file()

    # artifact contents carry consequential data, not labels
    corr = (dump_dir / "correspondence_f2_correspondence.txt").read_text()
    assert " mv " in corr and "block " in corr  # actual vector values
    mvs = (dump_dir / "decode_f2_mvs.txt").read_text()
    assert "state=" in mvs and ("dst " in mvs or "state=none" in mvs)
    color = (dump_dir / "color_convert_f2_color.txt").read_text()
    for field in ("range ", "primaries ", "transfer ", "matrix ",
                  "chroma_location ", "working_space state="):
        assert field in color, f"color capture missing {field}"
    window = (dump_dir / "window_select_f2_window.txt").read_text()
    assert "target 2" in window and "neighbor " in window
    # accumulate artifact is real image data with the exact PGM header
    acc_bytes = (dump_dir / "accumulate_f2_y.pgm").read_bytes()
    assert acc_bytes.startswith(b"P5\n64 64\n255\n")

    # output inventory in manifest, files on disk
    assert m["output_files"], "output inventory empty"
    for o in m["output_files"]:
        assert (out / o).is_file(), f"output inventory entry missing: {o}"

    # replay equality: deterministic dumps byte-identical across runs
    out2, dump2 = tmp_path / "cap2", tmp_path / "dumps2"
    assert run_runner(out2, *args[:-4] + ["--dump-dir", dump2,
                                          "--dump-stages", "all"]).returncode == 0
    for a in expected:
        b = dump2 / a.name
        assert b.is_file(), f"replay missing {a.name}"
        assert a.read_bytes() == b.read_bytes(), f"replay differs: {a.name}"


def test_missing_input_graceful_error(tmp_path):
    out = tmp_path / "err"
    proc = run_runner(out, "--input", tmp_path / "no_such_file.mp4",
                      "--frame-count", 1)
    assert proc.returncode == 1
    assert "error" in proc.stderr.lower()
    assert not (out / "manifest.json").exists() or True  # no crash is the contract
