#!/usr/bin/env python3
"""Local-only, fail-closed ANVIL paired visual-review server.

Uses stdlib only. Publishes only SHA-verified same-frame captures.
"""
import argparse
import hashlib
import json
import re
import struct
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit

MANIFEST_SCHEMA = "anvil-visual-pair-v1"
FRAME_NAME = re.compile(r"frame_[0-9]{6,20}\Z")
SHA256 = re.compile(r"[0-9a-f]{64}\Z")
ASSETS = {"anvil_baseline.png", "anvil_plus_fsr41.png"}
STATIC = {"index.html", "styles.css", "app.js"}


def png_dimensions(path):
    with path.open("rb") as f:
        data = f.read(24)
    if len(data) != 24 or data[:8] != b"\x89PNG\r\n\x1a\n" or data[12:16] != b"IHDR":
        return None
    return struct.unpack(">II", data[16:24])


def verified_pair(folder):
    """Return metadata only when both images and all capture invariants hold."""
    try:
        data = json.loads((folder / "manifest.json").read_text(encoding="utf-8"))
        if not isinstance(data, dict):
            return None
        if (data.get("schema") != MANIFEST_SCHEMA or
                data.get("anvil_live_profile") != "causal_cpu_previous_frame_v1" or
                data.get("verified_same_frame") is not True):
            return None
        index = data.get("frame_index")
        w, h = data.get("output_width"), data.get("output_height")
        if not (isinstance(index, int) and index >= 0 and
                FRAME_NAME.fullmatch(folder.name) and
                int(folder.name.split("_", 1)[1]) == index and
                isinstance(w, int) and 0 < w <= 16384 and
                isinstance(h, int) and 0 < h <= 16384 and
                isinstance(data.get("pts_us"), int) and data["pts_us"] >= 0 and
                isinstance(data.get("source_width"), int) and data["source_width"] > 0 and
                isinstance(data.get("source_height"), int) and data["source_height"] > 0):
            return None
        for k, expected, status in (
            ("a", "anvil_baseline.png", "baseline"),
            ("b", "anvil_plus_fsr41.png", "tryout"),
        ):
            arm = data.get(k)
            if not isinstance(arm, dict) or arm.get("image") != expected or arm.get("status") != status:
                return None
            claimed = arm.get("sha256")
            if not isinstance(claimed, str) or not SHA256.fullmatch(claimed):
                return None
            image = folder / expected
            if not image.is_file() or image.is_symlink() or image.stat().st_size > 120_000_000:
                return None
            if png_dimensions(image) != (w, h):
                return None
            with image.open("rb") as stream:
                digest = hashlib.file_digest(stream, "sha256").hexdigest()
            if digest != claimed:
                return None
        return data
    except (OSError, ValueError, TypeError, KeyError, json.JSONDecodeError):
        return None


def catalog(capture_root):
    result, rejected = [], []
    if not capture_root.exists():
        return {"pairs": [], "rejected": []}
    for folder in sorted(capture_root.iterdir()):
        if not folder.is_dir() or folder.is_symlink() or not FRAME_NAME.fullmatch(folder.name):
            continue
        item = verified_pair(folder)
        if item is None:
            rejected.append(folder.name)
            continue
        entry = dict(item)
        entry["key"] = folder.name
        for side in ("a", "b"):
            entry[side] = dict(entry[side])
            entry[side]["url"] = f"/api/image/{folder.name}/{entry[side]['image']}"
        result.append(entry)
    return {"pairs": result, "rejected": rejected}


def build_handler(root, viewer):
    class Handler(BaseHTTPRequestHandler):
        def respond(self, code, content, mime):
            self.send_response(code)
            self.send_header("Content-Type", mime)
            self.send_header("Content-Length", str(len(content)))
            self.send_header("Cache-Control", "no-store")
            self.send_header("X-Content-Type-Options", "nosniff")
            self.send_header("Content-Security-Policy",
                             "default-src 'self'; img-src 'self'; style-src 'self'; "
                             "script-src 'self'; connect-src 'self'; object-src 'none'")
            self.end_headers()
            self.wfile.write(content)

        def do_GET(self):
            route = unquote(urlsplit(self.path).path)
            if route in {"/", "/index.html", "/styles.css", "/app.js"}:
                filename = "index.html" if route == "/" else route[1:]
                if filename not in STATIC:
                    return self.respond(404, b"Not found", "text/plain")
                mime = "text/html; charset=utf-8" if filename.endswith(".html") else (
                    "text/css; charset=utf-8" if filename.endswith(".css") else
                    "text/javascript; charset=utf-8")
                return self.respond(200, (viewer / filename).read_bytes(), mime)
            if route == "/api/catalog":
                result = json.dumps(catalog(root), separators=(",", ":")).encode()
                return self.respond(200, result, "application/json; charset=utf-8")
            parts = route.split("/")
            if len(parts) == 5 and parts[:3] == ["", "api", "image"]:
                folder, name = parts[3], parts[4]
                if not FRAME_NAME.fullmatch(folder) or name not in ASSETS:
                    return self.respond(404, b"Not found", "text/plain")
                directory = root / folder
                if directory.is_symlink() or not directory.is_dir():
                    return self.respond(404, b"Not found", "text/plain")
                if verified_pair(directory) is None:
                    return self.respond(409, b"Evidence not verified", "text/plain")
                return self.respond(200, (directory / name).read_bytes(), "image/png")
            return self.respond(404, b"Not found", "text/plain")

        def log_message(self, fmt, *args):
            print("%s - %s" % (self.address_string(), fmt % args))
    return Handler


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--capture-dir", required=True, type=Path,
                   help="Directory containing frame_NNNNNN capture pairs")
    p.add_argument("--port", type=int, default=8765)
    opts = p.parse_args()
    if not 1 <= opts.port <= 65535:
        p.error("port must be 1..65535")
    root = opts.capture_dir.expanduser().resolve()
    root.mkdir(parents=True, exist_ok=True)
    server = ThreadingHTTPServer(("127.0.0.1", opts.port),
                                 build_handler(root, Path(__file__).resolve().parent))
    print(f"ANVIL Visual Review: http://127.0.0.1:{opts.port}")
    print(f"Evidence directory: {root}")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
