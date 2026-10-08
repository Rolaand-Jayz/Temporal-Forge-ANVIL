#!/usr/bin/env python3
"""Rebuild-time provenance regression (review 4209766186).

Guard: a rebuilt binary must never claim a stale Git source identity.

This fails on the fd6fd9b4 design, where ANVIL_GIT_SHA / ANVIL_GIT_DIRTY are
captured only by CMake configure (execute_process) and preferred
unconditionally by detectGitSha()/detectGitDirty(): editing a source file (or
committing a new revision) and re-running ninja recompiles the changed
sources WITHOUT re-running configure, so the rebuilt binary keeps embedding
the configure-time SHA and can even claim git_dirty=false for a mutated tree.

Flow (all runner invocations happen with cwd set to a NON-REPOSITORY
directory — the runtime-git detection tier must not be able to mask a stale
embedded identity):
  1. git worktree add --detach <tmp>/wt HEAD   (clean tree at HEAD)
  2. cmake configure (Ninja, Release); build ONLY target anvil_runner
  3. run once: git_sha == HEAD, git_dirty == "false",
     git_dirty_hash null/absent, provenance_source == "build_generated"
  4. mutate a tracked source; rebuild WITHOUT reconfigure; run again:
     must NOT claim (old HEAD, clean) — require git_dirty == "true" AND a
     non-empty git_dirty_hash
  5. commit the mutation; rebuild; run again: git_sha == NEW HEAD and clean
  6. determinism: two further no-change rebuilds leave the runner binary
     byte-identical (and untouched mtime — no recompilation churn)

Exit codes: 0 pass, 1 fail, 77 deterministic skip (tooling unavailable).
"""

import hashlib
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path

MUTATION_MARKER = "// provenance-rebuild-regression mutation\n"


def fail(msg: str) -> "None":
    print(f"FAIL anvil_provenance_rebuild_regression: {msg}")
    sys.exit(1)


def skip(msg: str) -> "None":
    print(f"SKIP anvil_provenance_rebuild_regression: {msg}")
    sys.exit(77)


def out(msg: str) -> "None":
    print(f"anvil_provenance_rebuild_regression: {msg}")


def check_tools(source_dir: Path) -> None:
    for tool in ("git", "cmake", "ninja", "ffmpeg"):
        if shutil.which(tool) is None:
            skip(f"'{tool}' binary unavailable")
    enc = subprocess.run(
        ["ffmpeg", "-hide_banner", "-encoders"],
        capture_output=True, text=True)
    if enc.returncode != 0 or "libx264" not in enc.stdout:
        skip("ffmpeg libx264 encoder unavailable")
    if not shutil.which("pkg-config"):
        skip("pkg-config unavailable (FFmpeg dev libs required to build)")
    probe = subprocess.run(
        ["pkg-config", "--exists", "libavformat libavcodec libavutil libswscale"])
    if probe.returncode != 0:
        skip("FFmpeg dev libraries (libavformat/libavcodec/libavutil/libswscale) unavailable")
    top = subprocess.run(
        ["git", "rev-parse", "--show-toplevel"], cwd=source_dir,
        capture_output=True, text=True)
    if top.returncode != 0:
        skip(f"source dir is not a git checkout: {source_dir}")


def git(source_dir: Path, *args: str) -> str:
    proc = subprocess.run(["git", "-C", str(source_dir), *args],
                          capture_output=True, text=True)
    if proc.returncode != 0:
        fail(f"git {' '.join(args)} failed: {proc.stderr.strip()}")
    return proc.stdout.strip()


def run_build(build_dir: Path) -> None:
    proc = subprocess.run(
        ["cmake", "--build", str(build_dir), "--target", "anvil_runner",
         "--parallel"],
        capture_output=True, text=True)
    if proc.returncode != 0:
        fail(f"cmake --build failed:\n{proc.stdout}\n{proc.stderr}")


def run_runner(runner: Path, clip: Path, out_dir: Path, cwd: Path):
    out_dir.mkdir(parents=True, exist_ok=True)
    proc = subprocess.run(
        [str(runner), "--output-dir", str(out_dir), "--input", str(clip),
         "--frame-count", "1"],
        cwd=str(cwd), capture_output=True, text=True)
    if proc.returncode != 0:
        fail(f"anvil_runner failed from non-repo cwd {cwd}:\n"
             f"{proc.stdout}\n{proc.stderr}")
    manifest_path = out_dir / "manifest.json"
    if not manifest_path.is_file():
        fail(f"manifest.json missing in {out_dir}")
    return json.loads(manifest_path.read_text())


def prov(manifest: dict, key: str):
    return manifest.get("provenance", {}).get(key, "<absent>")


def assert_build_generated(manifest: dict, phase: str) -> None:
    if prov(manifest, "provenance_source") != "build_generated":
        fail(f"{phase}: provenance_source must be 'build_generated', got "
             f"{prov(manifest, 'provenance_source')!r}")


def binary_fingerprint(path: Path):
    data = path.read_bytes()
    return hashlib.sha256(data).hexdigest(), path.stat().st_mtime


def main() -> None:
    if len(sys.argv) != 2:
        fail("usage: anvil_provenance_rebuild_regression.py <source_dir>")
    source_dir = Path(sys.argv[1]).resolve()

    check_tools(source_dir)

    tmp = Path(tempfile.mkdtemp(prefix="anvil_prov_rebuild_"))
    wt = tmp / "wt"
    build_dir = tmp / "build"
    nonrepo = tmp / "nonrepo_cwd"
    nonrepo.mkdir(parents=True)
    clip = tmp / "clip.mp4"
    runner = build_dir / "anvil_runner"
    worktree_added = False
    try:
        # The cwd for every runner invocation must be outside any repository:
        # the runtime-git tier would otherwise mask stale embedded identity.
        inside = subprocess.run(
            ["git", "rev-parse", "--git-dir"], cwd=str(nonrepo),
            capture_output=True, text=True)
        if inside.returncode == 0:
            skip(f"temp dir {nonrepo} is inside a repository; cannot test "
                 "runtime-git masking")

        # --- fixture: tiny deterministic x264 clip ---
        enc = subprocess.run(
            ["ffmpeg", "-y", "-loglevel", "error",
             "-f", "lavfi", "-i", "testsrc2=size=64x64:rate=10:duration=1",
             "-c:v", "libx264", "-pix_fmt", "yuv420p", str(clip)],
            capture_output=True, text=True)
        if enc.returncode != 0:
            fail(f"ffmpeg clip generation failed: {enc.stderr.strip()}")
        if not clip.is_file():
            fail("ffmpeg clip generation produced no file")

        # --- phase 0: clean detached worktree at HEAD ---
        add = subprocess.run(
            ["git", "worktree", "add", "--detach", str(wt), "HEAD"],
            cwd=str(source_dir), capture_output=True, text=True)
        if add.returncode != 0:
            fail(f"git worktree add failed: {add.stderr.strip()}")
        worktree_added = True
        head = git(wt, "rev-parse", "HEAD")
        dirty0 = git(wt, "status", "--porcelain")
        if dirty0:
            fail(f"fresh worktree is not clean: {dirty0!r}")

        # --- phase 1: configure + build target anvil_runner ---
        cfg = subprocess.run(
            ["cmake", "-S", str(wt), "-B", str(build_dir), "-G", "Ninja",
             "-DCMAKE_BUILD_TYPE=Release"],
            capture_output=True, text=True)
        if cfg.returncode != 0:
            fail(f"cmake configure failed:\n{cfg.stdout}\n{cfg.stderr}")
        run_build(build_dir)
        if not runner.is_file():
            fail(f"runner binary missing at {runner}")

        # --- phase 2: clean identity at HEAD, from a non-repo cwd ---
        m = run_runner(runner, clip, tmp / "out_clean", nonrepo)
        assert_build_generated(m, "clean-at-HEAD")
        if prov(m, "git_sha") != head:
            fail(f"clean-at-HEAD: git_sha {prov(m, 'git_sha')!r} != HEAD {head!r}")
        if prov(m, "git_dirty") != "false":
            fail(f"clean-at-HEAD: git_dirty must be \"false\", got "
                 f"{prov(m, 'git_dirty')!r}")
        if prov(m, "git_dirty_hash") not in (None, "<absent>"):
            fail(f"clean-at-HEAD: git_dirty_hash must be null/absent, got "
                 f"{prov(m, 'git_dirty_hash')!r}")
        out(f"clean identity verified: sha={head[:12]} dirty=false")

        # --- phase 3: mutate a tracked source; rebuild WITHOUT reconfigure ---
        mutated = wt / "src" / "util" / "Log.cpp"
        with mutated.open("a", encoding="utf-8") as f:
            f.write(MUTATION_MARKER)
        run_build(build_dir)  # ninja only — no cmake reconfigure
        m = run_runner(runner, clip, tmp / "out_dirty", nonrepo)
        assert_build_generated(m, "dirty-rebuild")
        dirty_sha, dirty_hash = prov(m, "git_sha"), prov(m, "git_dirty_hash")
        if prov(m, "git_dirty") == "false" and dirty_sha == head:
            fail("dirty-rebuild still claims the stale clean identity "
                 f"(git_dirty=false, git_sha==HEAD {head!r}) — configure-time "
                 "capture regressed")
        if prov(m, "git_dirty") != "true":
            fail(f"dirty-rebuild: git_dirty must be \"true\", got "
                 f"{prov(m, 'git_dirty')!r}")
        if not (isinstance(dirty_hash, str) and len(dirty_hash) == 64):
            fail(f"dirty-rebuild: git_dirty_hash must be a non-empty 64-hex "
                 f"string, got {dirty_hash!r}")
        if dirty_sha != head:
            fail(f"dirty-rebuild: uncommitted mutation must keep git_sha at "
                 f"HEAD {head!r}, got {dirty_sha!r}")
        out(f"dirty identity verified: dirty=true dirty_hash={dirty_hash[:12]}…")

        # --- phase 4: commit the mutation; rebuild; sha must advance ---
        git(wt, "add", "-A")
        git(wt, "-c", "user.name=anvil-provenance-regression",
            "-c", "user.email=anvil-provenance-regression@invalid",
            "commit", "-m", "provenance rebuild regression mutation")
        new_head = git(wt, "rev-parse", "HEAD")
        if new_head == head:
            fail("commit produced no new HEAD")
        run_build(build_dir)
        m = run_runner(runner, clip, tmp / "out_committed", nonrepo)
        assert_build_generated(m, "committed-rebuild")
        if prov(m, "git_sha") != new_head:
            fail(f"committed-rebuild: git_sha {prov(m, 'git_sha')!r} != new "
                 f"HEAD {new_head!r}")
        if prov(m, "git_dirty") != "false":
            fail(f"committed-rebuild: git_dirty must be \"false\", got "
                 f"{prov(m, 'git_dirty')!r}")
        if prov(m, "git_dirty_hash") not in (None, "<absent>"):
            fail(f"committed-rebuild: git_dirty_hash must be null/absent, got "
                 f"{prov(m, 'git_dirty_hash')!r}")
        out(f"committed identity verified: sha={new_head[:12]} dirty=false")

        # --- phase 5: determinism — no-change rebuilds must not churn ---
        fp = binary_fingerprint(runner)
        for i in (1, 2):
            run_build(build_dir)
            fp2 = binary_fingerprint(runner)
            if fp2 != fp:
                fail(f"no-change rebuild {i} changed the runner binary "
                     f"(sha256 {fp[0][:12]}… -> {fp2[0][:12]}…, mtime "
                     f"{fp[1]} -> {fp2[1]}): provenance regeneration is not "
                     "content-guarded")
        out("determinism verified: two no-change rebuilds left the binary "
            f"byte-identical (sha256 {fp[0][:12]}…, mtime untouched)")

        print("PASS anvil_provenance_rebuild_regression")
    finally:
        if worktree_added:
            subprocess.run(
                ["git", "worktree", "remove", "--force", str(wt)],
                cwd=str(source_dir), capture_output=True, text=True)
            subprocess.run(["git", "worktree", "prune"],
                           cwd=str(source_dir), capture_output=True, text=True)
        shutil.rmtree(tmp, ignore_errors=True)


if __name__ == "__main__":
    main()
