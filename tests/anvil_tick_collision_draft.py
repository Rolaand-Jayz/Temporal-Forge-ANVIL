# tests/anvil_tick_collision_draft.py — DRAFT runner-level regression for
# native-tick frame selection (review 4209783084).
#
# STATUS: DRAFT — intentionally NOT wired into CI (not collected by pytest:
# filename is not test_*.py, and nothing imports it). This is the runner-level
# regression the parent will transplant into tests/test_anvil_contract.py once
# the runner-side integration lands.
#
# DEPENDS ON RUNNER CHANGES NOT PRESENT AT THIS HEAD (worker A6 is forbidden
# from editing src/anvil/Runner.cpp, src/anvil/Manifest.cpp, and
# tools/anvil/main.cpp):
#   1. a `--start-pts-ticks N` selection flag with exact-match semantics
#      (anvil::selectFrameByTicks over decoded Observation::ptsTicks; exit 1
#      on no-match, exit 1 with "ambiguous" on duplicate ticks, exit 2 on
#      malformed/mutually-exclusive configuration);
#   2. Runner.cpp observationFromDecoded() copying d.tbNum/d.tbDen/d.ptsSource
#      into anvil::Observation (fields already appended in src/anvil/Core.hpp);
#   3. Manifest FrameRecord serialization of:
#      pts_ticks, timebase_num, timebase_den,
#      timestamp_source ("pts" | "best_effort" | "none").
# Until those exist, every test below fails by construction — that is the
# point of a draft: it encodes the target contract, not the current behavior.
#
# Fixture: an mp4 with a 10 MHz video track timescale containing frames at
# native ticks 0/1000000/2000000/3000000/4000006/4000010/6000000/7000000.
# Ticks 4000006 and 4000010 differ by 0.4 us yet BOTH rescale to 400001 us,
# so --start-pts-us cannot address either frame: one is unaddressable, the
# other collides. All truth is derived from ffprobe at runtime.

import subprocess
import shutil
from pathlib import Path

import pytest

REPO_ROOT = Path(__file__).resolve().parents[1]
RUNNER_CANDIDATES = [
    REPO_ROOT / "build" / "anvil_runner",
    REPO_ROOT / "build" / "anvil_runner" / "anvil_runner",
]

FFMPEG = shutil.which("ffmpeg")
FFPROBE = shutil.which("ffprobe")
RUNNER = next(
    (str(c.resolve()) for c in RUNNER_CANDIDATES
     if c.is_file() and __import__("os").access(c, __import__("os").X_OK)),
    None,
)

pytestmark = pytest.mark.skipif(
    RUNNER is None or FFMPEG is None or FFPROBE is None,
    reason="draft: anvil_runner binary or ffmpeg/ffprobe unavailable",
)


def run_runner(out_dir, *args):
    out_dir.mkdir(parents=True, exist_ok=True)
    cmd = [RUNNER, "--output-dir", str(out_dir), *map(str, args)]
    return subprocess.run(cmd, capture_output=True, text=True)


def load_manifest(out_dir: Path) -> dict:
    import json
    return json.loads((out_dir / "manifest.json").read_text())


def probe_tick_truth(clip: Path):
    """(tb_num, tb_den, [pts tick per frame]) — read back from the container."""
    tb = subprocess.run(
        [FFPROBE, "-v", "quiet", "-select_streams", "v:0",
         "-show_entries", "stream=time_base", "-of", "csv=p=0", str(clip)],
        check=True, capture_output=True, text=True).stdout.strip()
    num, den = (int(x) for x in tb.split("/"))
    ticks = [
        int(line.split(",")[0])   # csv rows may carry trailing empty fields
        for line in subprocess.run(
            [FFPROBE, "-v", "quiet", "-select_streams", "v:0",
             "-show_entries", "frame=pts", "-of", "csv=p=0", str(clip)],
            check=True, capture_output=True, text=True).stdout.splitlines()
        if line.strip()
    ]
    return num, den, ticks


def rescale_q_us(tick: int, num: int, den: int) -> int:
    """Mirror av_rescale_q(tick, {num, den}, {1, 1000000}): round to nearest,
    ties away from zero (ticks here are non-negative)."""
    return (2 * tick * num * 1_000_000 + den) // (2 * den)


@pytest.fixture(scope="module")
def tick_clip(tmp_path_factory):
    """Fine-timebase clip: 10 MHz track, ticks .../4000006/4000010/... which
    share one microsecond after av_rescale_q (0.4 us apart)."""
    out = tmp_path_factory.mktemp("fx") / "anvil_tick_collision.mp4"
    subprocess.run([
        FFMPEG, "-y", "-loglevel", "error",
        "-f", "lavfi", "-i", "testsrc2=size=32x32:rate=10:duration=0.8",
        "-vf", "settb=tb=1/10000000,"
               "setpts='if(eq(N,4),4000006,if(eq(N,5),4000010,PTS))'",
        "-c:v", "libx264", "-pix_fmt", "yuv420p",
        "-x264-params", "keyint=8:bframes=0",
        "-enc_time_base", "1/10000000",
        "-video_track_timescale", "10000000",
        str(out),
    ], check=True)
    return out


def test_manifest_frames_carry_native_timestamp_identity(tick_clip, tmp_path):
    tb_num, tb_den, ticks = probe_tick_truth(tick_clip)
    assert (tb_den > 1_000_000), "fixture timebase is not finer than microseconds"
    out = tmp_path / "identity"
    proc = run_runner(out, "--input", tick_clip, "--frame-count", len(ticks))
    assert proc.returncode == 0, proc.stderr
    frames = load_manifest(out)["frames"]
    assert len(frames) == len(ticks)
    for f, tick in zip(frames, ticks):
        assert f["pts_ticks"] == tick                # native ticks preserved
        assert f["timebase_num"] == tb_num           # container timebase named
        assert f["timebase_den"] == tb_den
        assert f["timestamp_source"] == "pts"        # displayed ts provenance


def test_start_pts_ticks_selects_exactly_one_colliding_frame(
        tick_clip, tmp_path):
    tb_num, tb_den, ticks = probe_tick_truth(tick_clip)
    # exact pair: distinct ticks sharing one microsecond (av_rescale_q semantics)
    colliding = []
    for i, t in enumerate(ticks):
        for u in ticks[i + 1:]:
            if t != u and (rescale_q_us(t, tb_num, tb_den) ==
                           rescale_q_us(u, tb_num, tb_den)):
                colliding = [t, u]
    assert colliding, "fixture lost its sub-us collision"

    # FUTURE FLAG (--start-pts-ticks does not exist at the A6 head): each
    # native tick must address exactly its own frame — impossible via us.
    for tick, expect_index in zip(colliding, (ticks.index(colliding[0]),
                                             ticks.index(colliding[1]))):
        out = tmp_path / f"ticks_{tick}"
        proc = run_runner(out, "--input", tick_clip,
                          "--start-pts-ticks", tick, "--frame-count", 1)
        assert proc.returncode == 0, proc.stderr
        m = load_manifest(out)
        assert m["config"]["start_pts_ticks"] == tick
        assert m["frames"][0]["frame_index"] == expect_index
        assert m["frames"][0]["pts_ticks"] == tick

    # the shared microsecond is ambiguous at the runner boundary: exact-match
    # us selection must refuse rather than silently pick one frame
    shared_us = rescale_q_us(colliding[0], tb_num, tb_den)
    out2 = tmp_path / "us_ambiguous"
    proc = run_runner(out2, "--input", tick_clip,
                      "--start-pts-us", shared_us, "--frame-count", 1)
    assert proc.returncode == 1
    assert "ambiguous" in proc.stderr
    assert not (out2 / "manifest.json").exists()


def test_start_pts_ticks_no_match_and_malformed(tick_clip, tmp_path):
    tb_num, tb_den, ticks = probe_tick_truth(tick_clip)
    gap = (ticks[3] + ticks[4]) // 2
    assert gap not in ticks
    proc = run_runner(tmp_path / "nomatch", "--input", tick_clip,
                      "--start-pts-ticks", gap, "--frame-count", 1)
    assert proc.returncode == 1
    assert f"no frame with exact pts_ticks={gap}" in proc.stderr
    assert not (tmp_path / "nomatch" / "manifest.json").exists()

    # negative ticks are valid PTS, never malformed configuration
    proc = run_runner(tmp_path / "negative", "--input", tick_clip,
                      "--start-pts-ticks", "-1000000", "--frame-count", 1)
    assert proc.returncode == 1
    assert "configuration error" not in proc.stderr.lower()
    assert "exact pts_ticks=-1000000" in proc.stderr

    # mutually exclusive with --start-frame like --start-pts-us
    proc = run_runner(tmp_path / "both", "--input", tick_clip,
                      "--start-frame", 1, "--start-pts-ticks", ticks[0],
                      "--frame-count", 1)
    assert proc.returncode == 2
    assert "mutually exclusive" in proc.stderr
