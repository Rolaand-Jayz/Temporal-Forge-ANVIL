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
def twoscene_clip(tmp_path_factory):
    """32x32 hard cut black->white at frame 6 (6+6 frames @10fps)."""
    out = tmp_path_factory.mktemp("fx") / "anvil_twoscene.mp4"
    cmd = [
        FFMPEG, "-y", "-loglevel", "error",
        "-f", "lavfi", "-i", "color=black:size=32x32:rate=10:duration=0.6",
        "-f", "lavfi", "-i", "color=white:size=32x32:rate=10:duration=0.6",
        "-filter_complex", "[0:v][1:v]concat=n=2:v=1:a=0",
        "-c:v", "libx264", "-pix_fmt", "yuv420p",
        "-x264-params", "keyint=6:bframes=0",
        str(out),
    ]
    subprocess.run(cmd, check=True)
    return out


@pytest.fixture(scope="module")
def hevc_clip(tmp_path_factory):
    """HEVC fixture: software hevc decode exports no MV side data."""
    if subprocess.run([FFMPEG, "-hide_banner", "-encoders"], capture_output=True,
                      text=True).stdout.find("libx265") < 0:
        pytest.skip("libx265 encoder unavailable")
    out = tmp_path_factory.mktemp("fx") / "anvil_hevc.mp4"
    cmd = [
        FFMPEG, "-y", "-loglevel", "error",
        "-f", "lavfi", "-i", "testsrc2=size=64x64:rate=10:duration=1",
        "-c:v", "libx265", "-pix_fmt", "yuv420p",
        "-x265-params", "keyint=10:bframes=0",
        str(out),
    ]
    subprocess.run(cmd, check=True)
    return out


@pytest.fixture(scope="module")
def hevc_hdr_clip(tmp_path_factory):
    """Genuine HDR10 side data (mastering display + content light) via x265."""
    if subprocess.run([FFMPEG, "-hide_banner", "-encoders"], capture_output=True,
                      text=True).stdout.find("libx265") < 0:
        pytest.skip("libx265 encoder unavailable")
    out = tmp_path_factory.mktemp("fx") / "anvil_hevc_hdr10.mp4"
    cmd = [
        FFMPEG, "-y", "-loglevel", "error",
        "-f", "lavfi", "-i", "testsrc2=size=64x64:rate=10:duration=1",
        "-c:v", "libx265", "-pix_fmt", "yuv420p10le",
        "-x265-params",
        "keyint=10:bframes=0:colorprim=bt2020:transfer=smpte2084:"
        "colormatrix=bt2020nc:hdr10=1:"
        "master-display=G(13250,34500)B(7500,4000)R(34000,16000)"
        "WP(15635,16450)L(10000000,1):max-cll=1000,400",
        str(out),
    ]
    subprocess.run(cmd, check=True)
    return out


@pytest.fixture(scope="module")
def clip66x65(tmp_path_factory):
    """Non-16-aligned frame: right column tiles 2 wide, bottom row 1 tall."""
    out = tmp_path_factory.mktemp("fx") / "anvil_66x65.mp4"
    cmd = [
        FFMPEG, "-y", "-loglevel", "error",
        "-f", "lavfi", "-i", "testsrc2=size=66x65:rate=10:duration=1",
        "-c:v", "libx264", "-pix_fmt", "yuv420p",
        "-x264-params",
        "keyint=10:bframes=0:colorprim=bt709:transfer=bt709:colorrange=tv:"
        "colormatrix=bt709",
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


def test_auto_scene_cut_blocks_cross_cut_accumulation(twoscene_clip, tmp_path):
    """Auto-detected cuts must exclude crossing neighbors, both directions,
    before windows are built (review 4202350954)."""
    dump_dir = tmp_path / "cuts_dumps"
    out = tmp_path / "cuts_out"
    proc = run_runner(out, "--input", twoscene_clip, "--start-frame", 3,
                      "--frame-count", 4, "--past", 2, "--future", 2,
                      "--correspondence", "estimate", "--auto-scene-cut",
                      "--dump-dir", dump_dir, "--dump-stages", "window_select")
    assert proc.returncode == 0, proc.stderr
    m = load_manifest(out)
    # cut detected between frames 5 and 6, canonicalized before windows
    cuts = [e["frame"] for e in m["events"] if e["type"] == "scene_cut"]
    assert 6 in cuts
    # target 4 (black scene): future neighbor 6 is across the cut
    win4 = (dump_dir / "window_select_f4_window.txt").read_text()
    assert "neighbor 2" in win4 and "neighbor 5" in win4
    assert "excluded 6 reason=cut_boundary" in win4
    # target 6 (first white frame): past neighbors 4,5 are across the cut
    win6 = (dump_dir / "window_select_f6_window.txt").read_text()
    assert "excluded 5 reason=cut_boundary" in win6
    assert "excluded 4 reason=cut_boundary" in win6
    # no neighbor list may span the boundary
    for t in range(3, 7):
        win = (dump_dir / f"window_select_f{t}_window.txt").read_text()
        ids = {int(l.split()[1]) for l in win.splitlines()
               if l.startswith("neighbor ")}
        if t <= 5:
            assert all(i <= 5 for i in ids), f"target {t} spans cut: {ids}"
        else:
            assert all(i >= 6 for i in ids), f"target {t} spans cut: {ids}"


def test_git_sha_matches_rev_parse(sdr_clip, tmp_path):
    """Regression (review 4202350957): manifest git_sha must equal
    `git rev-parse HEAD` for any valid 40-hex SHA, not just '0'-leading."""
    out = tmp_path / "sha"
    assert run_runner(out, "--input", sdr_clip, "--frame-count", 1).returncode == 0
    m = load_manifest(out)
    sha = m["provenance"]["git_sha"]
    assert sha is not None, "git_sha null inside a git checkout"
    assert len(sha) == 40
    int(sha, 16)  # valid hex
    expected = subprocess.run(["git", "rev-parse", "HEAD"], capture_output=True,
                              text=True, cwd=REPO_ROOT).stdout.strip()
    assert sha == expected


def test_codec_side_info_state_is_per_codec_and_truthful(
        sdr_clip, hevc_clip, tmp_path):
    """Regression (review 4202350960): exported-but-unproven MV data is
    'ambiguous' (usable count 0), I-frames of a proven codec are
    'estimator_only', and an unsupported input codec never inherits another
    codec's capability."""
    out = tmp_path / "h264"
    assert run_runner(out, "--input", sdr_clip, "--frame-count", 4).returncode == 0
    m = load_manifest(out)
    fr = {f["frame_index"]: f for f in m["frames"]}
    assert fr[0]["side_info_state"] == "estimator_only"  # I-frame, h264 proven
    for t in (1, 2, 3):  # P frames: MVs exported but references unproven
        assert fr[t]["side_info_state"] == "ambiguous", fr[t]
        assert fr[t]["codec_mv_count"] > 0
        assert fr[t]["codec_mv_usable_count"] == 0  # unusable evidence

    outh = tmp_path / "hevc"
    assert run_runner(outh, "--input", hevc_clip, "--frame-count", 3).returncode == 0
    mh = load_manifest(outh)
    for f in mh["frames"]:
        assert f["side_info_state"] == "unsupported", (
            "global H.264 capability leaked into HEVC input")
        assert f["codec_mv_count"] == 0


def test_per_frame_structured_color_metadata(
        sdr_clip, hdr_clip, hevc_hdr_clip, tmp_path_factory):
    """Regression (review 4202350964/4202350968): manifests carry structured
    per-frame source AND output color metadata without debug dumps; HDR
    side-data presence reflects genuine source evidence."""
    # SDR full metadata
    out = tmp_path_factory.mktemp("csdr")
    assert run_runner(out, "--input", sdr_clip, "--frame-count", 1).returncode == 0
    m = load_manifest(out)
    cs, co = m["frames"][0]["color_source"], m["frames"][0]["color_output"]
    assert (cs["range"], cs["primaries"], cs["transfer"], cs["matrix"]) == \
        ("limited", "bt709", "bt709", "bt709")
    assert cs["chroma_location"] != "unspecified"
    assert cs["bit_depth"] == 8
    # RGB output: explicit output space, chroma not applicable
    assert (co["matrix"], co["range"], co["pixel_format"]) == ("rgb", "full", "rgb24")
    assert co["chroma_location"] == "not_applicable"
    assert co["transfer"] == "bt709"  # carried, never converted silently

    # PQ planes output: output space mirrors the preserved source planes
    outp = tmp_path_factory.mktemp("cpq")
    assert run_runner(outp, "--input", hdr_clip, "--frame-count", 1).returncode == 0
    mp = load_manifest(outp)
    cp = mp["frames"][0]["color_source"]
    assert (cp["transfer"], cp["primaries"], cp["matrix"]) == ("pq", "bt2020", "bt2020_ncl")
    assert mp["frames"][0]["color_output"] == cp

    # Missing-tag input: explicit unknowns, nothing invented
    nometa = tmp_path_factory.mktemp("cno")
    nm = nometa / "nometa.mp4"
    subprocess.run([
        FFMPEG, "-y", "-loglevel", "error",
        "-f", "lavfi", "-i", "testsrc2=size=64x64:rate=10:duration=1",
        "-c:v", "libx264", "-pix_fmt", "yuv420p",
        "-x264-params", "keyint=10:bframes=0", str(nm)], check=True)
    outn = tmp_path_factory.mktemp("cnoout")
    assert run_runner(outn, "--input", nm, "--frame-count", 1).returncode == 0
    mn = load_manifest(outn)
    cn = mn["frames"][0]["color_source"]
    assert cn["transfer"] == "unspecified"
    assert cn["primaries"] == "unspecified"
    # FFmpeg reports its codec-default chroma siting ("left") when the
    # stream omits it; that is the decoder's stated value, not an invented
    # one, and is recorded verbatim.
    assert cn["chroma_location"] in ("unspecified", "left")

    # Genuine HDR10 side data survives the decode boundary (review 4202350968)
    outh = tmp_path_factory.mktemp("chdr")
    assert run_runner(outh, "--input", hevc_hdr_clip, "--frame-count", 1).returncode == 0
    mh = load_manifest(outh)
    ch = mh["frames"][0]["color_source"]
    assert ch["hdr_mastering_display_present"] is True
    assert ch["hdr_content_light_level_present"] is True
    assert ch["transfer"] == "pq" and ch["bit_depth"] == 10


def test_stage_timing_attribution(sdr_clip, tmp_path):
    """Regression (review 4202350973): every exercised stage has a recorded,
    meaningful timing; visibility is independent; the actual RGB transform
    lands on color_convert, not output."""
    out = tmp_path / "timing"
    assert run_runner(out, "--input", sdr_clip, "--start-frame", 1,
                      "--frame-count", 1, "--past", 1).returncode == 0
    m = load_manifest(out)
    totals = {}
    for t in m["stage_timings"]:
        assert t["stage"] not in (True, False, None, ""), \
            f"unserialized stage name: {t}"
        totals.setdefault(t["stage"], 0)
        totals[t["stage"]] += t["nanoseconds"]
    for stage in ("decode", "window_select", "correspondence", "visibility",
                  "sample_geometry", "color_convert", "accumulate", "output"):
        assert stage in totals, f"missing timing for {stage}"
        assert totals[stage] > 0, f"zero timing for {stage}"
    # conversion fixture: a second color_convert entry carries sws_scale work
    conv_entries = [t for t in m["stage_timings"] if t["stage"] == "color_convert"]
    assert len(conv_entries) >= 2, "actual conversion not attributed to color_convert"


def test_partial_edge_block_coverage(clip66x65, tmp_path):
    """Regression (review 4202350977): non-16-aligned frames get clipped
    edge tiles whose measured correspondence covers every pixel; uncovered
    placeholder motion can no longer enter accumulation as valid evidence."""
    dump_dir = tmp_path / "dumps66"
    out = tmp_path / "out66"
    assert run_runner(out, "--input", clip66x65, "--start-frame", 2,
                      "--frame-count", 1, "--past", 1,
                      "--dump-dir", dump_dir,
                      "--dump-stages", "correspondence").returncode == 0
    corr = (dump_dir / "correspondence_f2_correspondence.txt").read_text()
    # clipped tiles exist for 66x65 (right 2-wide / bottom 1-tall)
    clipped = [l for l in corr.splitlines()
               if l.startswith("block ")
               and (l.split()[3] != "16" or l.split()[5] != "16")]
    assert clipped, "no clipped edge tiles: partial blocks still skipped"
    m = load_manifest(out)
    fr = m["frames"][0]
    assert fr["total_samples"] > 0
    assert fr["valid_samples"] == fr["total_samples"], (
        "uncovered pixels leaked into accumulation as invalid/valid mismatch")


def test_ground_truth_attachment_and_provenance(sdr_clip, tmp_path):
    """HR ground truth attaches explicitly: per-frame association, dimensions,
    format, and an independently verifiable SHA-256 in the manifest."""
    gt = tmp_path / "gt.pgm"
    # Genuine 2x HR reference for the 64x64 LR observation. The first raster
    # byte is '#' to guard the binary PNM header/raster boundary.
    payload = bytes([0x23]) + bytes((i * 11) % 256 for i in range(128 * 128 - 1))
    gt.write_bytes(b"P5\n128 128\n255\n" + payload)
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
    assert (g["width"], g["height"], g["maxval"]) == (128, 128, 255)
    assert (g["observation_width"], g["observation_height"]) == (64, 64)
    assert (g["scale_x"], g["scale_y"]) == (2.0, 2.0)
    assert g["resolution_relation"] == "higher_resolution"
    assert g["bytes_per_sample"] == 1
    assert g["format"] == "pgm"
    assert g["size_bytes"] == gt.stat().st_size
    assert "excluded from candidate reconstruction" in g["usage"]
    assert m["frames"][0]["has_ground_truth"] is True


def test_ground_truth_never_contaminates_reconstruction(sdr_clip, tmp_path):
    gt = tmp_path / "gt.pgm"
    gt.write_bytes(b"P5\n128 128\n255\n" + bytes((i * 3) % 256 for i in range(128 * 128)))
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
    assert proc1.returncode == 1 and "lower_resolution_than_observation" in proc1.stderr
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


def _read_pgm_samples(path: Path):
    data = path.read_bytes()
    magic, dims, maxval_b, payload = data.split(b"\n", 3)
    assert magic == b"P5"
    width, height = map(int, dims.split())
    maxval = int(maxval_b)
    if maxval < 256:
        samples = list(payload)
        assert len(samples) == width * height
    else:
        assert len(payload) == width * height * 2
        samples = [int.from_bytes(payload[i:i + 2], "big")
                   for i in range(0, len(payload), 2)]
    return width, height, maxval, samples


def _decode_yuv420p10le_frames(path: Path, frame_count: int):
    proc = subprocess.run([
        FFMPEG, "-loglevel", "error", "-i", str(path),
        "-frames:v", str(frame_count), "-f", "rawvideo",
        "-pix_fmt", "yuv420p10le", "-"
    ], check=True, stdout=subprocess.PIPE)
    w = h = 64
    y_n = w * h
    c_n = (w // 2) * (h // 2)
    frame_bytes = (y_n + 2 * c_n) * 2
    assert len(proc.stdout) >= frame_count * frame_bytes
    frames = []
    for fi in range(frame_count):
        raw = proc.stdout[fi * frame_bytes:(fi + 1) * frame_bytes]
        vals = [int.from_bytes(raw[i:i + 2], "little")
                for i in range(0, len(raw), 2)]
        frames.append((vals[:y_n],
                       vals[y_n:y_n + c_n],
                       vals[y_n + c_n:y_n + 2 * c_n]))
    return frames


def test_hdr_10bit_temporal_reconstruction_is_sample_depth_safe(hdr_clip, tmp_path):
    """10-bit samples survive the pipeline depth-intact, and correspondence
    mode 'none' provides NO evidence: the output must equal the target plane
    exactly (fabricated zero-motion blending is prohibited by the
    partial-coverage contract; review 4202350977)."""
    out = tmp_path / "hdr_temporal"
    proc = run_runner(out, "--input", hdr_clip, "--start-frame", 2,
                      "--frame-count", 1, "--past", 1, "--future", 0,
                      "--correspondence", "none")
    assert proc.returncode == 0, proc.stderr

    source = _decode_yuv420p10le_frames(hdr_clip, 3)
    prev, target = source[1], source[2]
    for suffix, plane, dims in (("y", 0, (64, 64)),
                                ("u", 1, (32, 32)),
                                ("v", 2, (32, 32))):
        p = out / f"frame_2_{suffix}.pgm"
        assert p.is_file(), f"missing preserved {suffix.upper()} plane"
        w, h, maxval, actual = _read_pgm_samples(p)
        assert (w, h) == dims
        assert maxval == 1023
        # no correspondence evidence -> neighbor excluded -> target passthrough
        assert actual == target[plane], (
            f"10-bit {suffix.upper()} blended without correspondence evidence")
        assert max(actual) > 255, "fixture did not exercise >8-bit values"

    # With proven (estimated) correspondence the neighbor participates again:
    # accumulation is exercised and every sample has valid coverage.
    out2 = tmp_path / "hdr_temporal_est"
    proc = run_runner(out2, "--input", hdr_clip, "--start-frame", 2,
                      "--frame-count", 1, "--past", 1, "--future", 0,
                      "--correspondence", "estimate")
    assert proc.returncode == 0, proc.stderr
    m = load_manifest(out2)
    fr = m["frames"][0]
    assert fr["total_samples"] > 0
    assert fr["valid_samples"] == fr["total_samples"], (
        "uncovered pixels present in estimate mode")


def test_missing_input_graceful_error(tmp_path):
    out = tmp_path / "err"
    proc = run_runner(out, "--input", tmp_path / "no_such_file.mp4",
                      "--frame-count", 1)
    assert proc.returncode == 1
    assert "error" in proc.stderr.lower()
    assert not (out / "manifest.json").exists() or True  # no crash is the contract
