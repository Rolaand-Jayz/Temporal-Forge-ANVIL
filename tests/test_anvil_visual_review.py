"""Evidence catalog and local asset isolation for the ANVIL visual review server."""
import hashlib
import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import zlib

SERVER = Path(__file__).resolve().parents[1] / "tools" / "anvil" / "review" / "server.py"
spec = importlib.util.spec_from_file_location("anvil_review_server", SERVER)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)


def png(w=2, h=2):
    def chunk(name, content):
        return struct.pack(">I", len(content)) + name + content + \
            struct.pack(">I", zlib.crc32(name + content) & 0xffffffff)
    rows = b"".join(b"\x00" + b"\x00\x44\x88\xff" * w for _ in range(h))
    return b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) + \
           chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b"")


def pair(root, frame=48):
    folder = root / f"frame_{frame:06d}"
    folder.mkdir()
    a, b = png(), png()
    (folder / "anvil_baseline.png").write_bytes(a)
    (folder / "anvil_plus_fsr41.png").write_bytes(b)
    manifest = {
        "schema": "anvil-visual-pair-v1",
        "verified_same_frame": True,
        "frame_index": frame, "pts_us": frame * 33333, "pts_ticks": frame,
        "tb_num": 1, "tb_den": 30, "source_width": 1, "source_height": 1,
        "output_width": 2, "output_height": 2,
        "a": {"image": "anvil_baseline.png", "status": "baseline",
              "sha256": hashlib.sha256(a).hexdigest()},
        "b": {"image": "anvil_plus_fsr41.png", "status": "tryout",
              "sha256": hashlib.sha256(b).hexdigest()},
    }
    (folder / "manifest.json").write_text(json.dumps(manifest))
    return folder, manifest


def test_verified_pair_and_modified_png():
    with tempfile.TemporaryDirectory() as tmp:
        folder, record = pair(Path(tmp))
        assert mod.verified_pair(folder)["pts_ticks"] == 48
        assert len(mod.catalog(Path(tmp))["pairs"]) == 1
        (folder / "anvil_plus_fsr41.png").write_bytes(png(3, 2))
        assert mod.verified_pair(folder) is None
        assert mod.catalog(Path(tmp))["rejected"] == ["frame_000048"]


def test_rejects_unsupported_schema_and_missing_assets():
    with tempfile.TemporaryDirectory() as tmp:
        folder, record = pair(Path(tmp))
        record["verified_same_frame"] = False
        (folder / "manifest.json").write_text(json.dumps(record))
        assert mod.verified_pair(folder) is None
        record["verified_same_frame"] = True
        (folder / "manifest.json").write_text(json.dumps(record))
        (folder / "anvil_baseline.png").unlink()
        assert mod.verified_pair(folder) is None


def test_frame_directory_identity_and_path_scope():
    with tempfile.TemporaryDirectory() as tmp:
        root = Path(tmp)
        folder, record = pair(root)
        record["frame_index"] = 49
        (folder / "manifest.json").write_text(json.dumps(record))
        assert mod.verified_pair(folder) is None
        (root / "frame_000049").symlink_to(folder, target_is_directory=True)
        assert len(mod.catalog(root)["pairs"]) == 0


def test_frontend_modes_and_clear_status():
    viewer = SERVER.parent
    index = (viewer / "index.html").read_text()
    app = (viewer / "app.js").read_text()
    for mode in ("wipe", "split", "fade", "flicker", "difference"):
        assert 'data-mode="' + mode + '"' in index
        assert '"' + mode + '"' in app
    assert "verified" in app.lower() and "sha" in app.lower()
    assert "ANVIL baseline" in index
    assert "ANVIL baseline + FSR 4.1" in index
