#!/usr/bin/env python3
"""Local-only ANVIL visual review.

Producer-key HMAC authenticates capture-process possession of an external secret.
It does not independently prove GPU origin, frame equivalence or visual quality.
"""
import argparse
import hashlib
import hmac
import zlib
import stat
import json
import re
import struct
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import unquote, urlsplit

MANIFEST_SCHEMA = "anvil-visual-pair-v2"
PROFILE = "causal_cpu_previous_frame_v1"
MAX_PIXELS = 40_000_000
MAX_PNG_BYTES = 120_000_000
FRAME_NAME = re.compile(r"frame_[0-9]{6,20}\Z")
SHA256 = re.compile(r"[0-9a-f]{64}\Z")
ASSETS = {"anvil_causal_live_control.png", "anvil_causal_live_fsr41.png"}
STATIC = {"index.html", "styles.css", "app.js"}


def load_attestation_key(path):
    """A key outside the evidence tree is mandatory; readable only by owner."""
    path = Path(path)
    if path.is_symlink() or not path.is_file():
        raise ValueError("Attestation key must be a regular non-symlink file")
    if path.stat().st_mode & (stat.S_IRWXG | stat.S_IRWXO):
        raise ValueError("Attestation key permissions must exclude group/other")
    key = path.read_bytes()
    if not 32 <= len(key) <= 4096:
        raise ValueError("Attestation key must contain 32–4096 random bytes")
    return key


def decoded_png_dimensions(path):
    """Check PNG CRC/chunk completeness, inflate and reconstruct all RGB(A)8 rows.

    Deliberately reject unsupported PNG encodings rather than claiming success.
    """
    if path.is_symlink() or not path.is_file() or path.stat().st_size > MAX_PNG_BYTES:
        return None
    data = path.read_bytes()
    if not data.startswith(b"\x89PNG\r\n\x1a\n"):
        return None
    pos, dims, channels = 8, None, None
    idats, idat_seen, idat_ended, finished = [], False, False, False
    allowed = {b"IHDR", b"IDAT", b"IEND", b"gAMA", b"sRGB", b"cHRM",
               b"pHYs", b"tEXt", b"zTXt", b"iTXt", b"tIME", b"iCCP"}
    while pos + 12 <= len(data):
        length = struct.unpack_from(">I", data, pos)[0]
        tag = data[pos + 4:pos + 8]
        end = pos + 12 + length
        if end > len(data) or tag not in allowed:
            return None
        chunk = data[pos + 8:pos + 8 + length]
        crc = struct.unpack_from(">I", data, end - 4)[0]
        if zlib.crc32(tag + chunk) & 0xffffffff != crc:
            return None
        if dims is None:
            if tag != b"IHDR" or length != 13:
                return None
            width, height, depth, color, compression, filtering, interlace = struct.unpack(">IIBBBBB", chunk)
            if (not width or not height or width * height > MAX_PIXELS or
                    depth != 8 or color not in (2, 6) or
                    compression or filtering or interlace):
                return None
            dims, channels = (width, height), (3 if color == 2 else 4)
        elif tag == b"IHDR":
            return None
        elif tag == b"IDAT":
            if idat_ended:
                return None
            idat_seen = True
            idats.append(chunk)
        elif tag == b"IEND":
            if length or not idat_seen or end != len(data):
                return None
            finished = True
            break
        elif idat_seen:
            idat_ended = True
        pos = end
    if not finished or dims is None:
        return None
    width, height = dims
    rowbytes = width * channels
    expected = height * (rowbytes + 1)
    try:
        obj = zlib.decompressobj()
        pixels = obj.decompress(b"".join(idats), expected + 1)
        if (len(pixels) != expected or not obj.eof or obj.unused_data or
                obj.unconsumed_tail or obj.flush()):
            return None
    except zlib.error:
        return None
    previous = bytearray(rowbytes)
    for y in range(height):
        start = y * (rowbytes + 1)
        mode = pixels[start]
        if mode > 4:
            return None
        row = bytearray(pixels[start + 1:start + 1 + rowbytes])
        for x in range(rowbytes):
            left = row[x - channels] if x >= channels else 0
            above = previous[x]
            upper_left = previous[x - channels] if x >= channels else 0
            if mode == 1: predictor = left
            elif mode == 2: predictor = above
            elif mode == 3: predictor = (left + above) // 2
            elif mode == 4:
                p = left + above - upper_left
                a, b, c = abs(p-left), abs(p-above), abs(p-upper_left)
                predictor = left if a <= b and a <= c else above if b <= c else upper_left
            else: predictor = 0
            row[x] = (row[x] + predictor) & 255
        previous = row
    return dims


def verified_pair(folder, key):
    """Producer-authenticated manifest AND independently decoded intact PNGs."""
    if not key or len(key) < 32:
        return None
    try:
        manifest_path = folder / "manifest.json"
        seal_path = folder / "capture.hmac"
        if manifest_path.is_symlink() or seal_path.is_symlink():
            return None
        raw_manifest = manifest_path.read_bytes()
        supplied = seal_path.read_text(encoding="ascii").strip()
        if not SHA256.fullmatch(supplied) or not hmac.compare_digest(
                hmac.new(key, raw_manifest, hashlib.sha256).hexdigest(), supplied):
            return None
        data = json.loads(raw_manifest)
        if not isinstance(data, dict):
            return None
        if (data.get("schema") != MANIFEST_SCHEMA or
                data.get("anvil_live_profile") != PROFILE or
                data.get("capture_attestation") != "producer-hmac-sha256-v1" or
                data.get("verified_same_frame") is not True):
            return None
        index = data.get("frame_index")
        w, h = data.get("output_width"), data.get("output_height")
        if not (isinstance(index, int) and index >= 0 and
                FRAME_NAME.fullmatch(folder.name) and
                int(folder.name.split("_", 1)[1]) == index and
                isinstance(w, int) and isinstance(h, int) and
                w > 0 and h > 0 and w * h <= MAX_PIXELS and
                isinstance(data.get("pts_us"), int) and data["pts_us"] >= 0 and
                isinstance(data.get("source_width"), int) and data["source_width"] > 0 and
                isinstance(data.get("source_height"), int) and data["source_height"] > 0):
            return None
        for k, expected, status in (
            ("a", "anvil_causal_live_control.png", "control"),
            ("b", "anvil_causal_live_fsr41.png", "tryout"),
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
            if decoded_png_dimensions(image) != (w, h):
                return None
            with image.open("rb") as stream:
                digest = hashlib.file_digest(stream, "sha256").hexdigest()
            if not hmac.compare_digest(digest, claimed):
                return None
        return data
    except (OSError, ValueError, TypeError, KeyError, UnicodeDecodeError, json.JSONDecodeError):
        return None


def catalog(capture_root, key):
    result, rejected = [], []
    if not capture_root.exists():
        return {"pairs": [], "rejected": []}
    for folder in sorted(capture_root.iterdir()):
        if not folder.is_dir() or folder.is_symlink() or not FRAME_NAME.fullmatch(folder.name):
            continue
        item = verified_pair(folder, key)
        if item is None:
            rejected.append(folder.name)
            continue
        entry = dict(item)
        entry["key"] = folder.name
        entry["evidence_status"] = "producer-key-authenticated-not-independent-gpu-proof"
        for side in ("a", "b"):
            entry[side] = dict(entry[side])
            entry[side]["url"] = f"/api/image/{folder.name}/{entry[side]['image']}"
        result.append(entry)
    return {"pairs": result, "rejected": rejected}


def build_handler(root, viewer, key):
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
                result = json.dumps(catalog(root, key), separators=(",", ":")).encode()
                return self.respond(200, result, "application/json; charset=utf-8")
            parts = route.split("/")
            if len(parts) == 5 and parts[:3] == ["", "api", "image"]:
                folder, name = parts[3], parts[4]
                if not FRAME_NAME.fullmatch(folder) or name not in ASSETS:
                    return self.respond(404, b"Not found", "text/plain")
                directory = root / folder
                if directory.is_symlink() or not directory.is_dir():
                    return self.respond(404, b"Not found", "text/plain")
                if verified_pair(directory, key) is None:
                    return self.respond(409, b"Capture not authenticated or image invalid", "text/plain")
                return self.respond(200, (directory / name).read_bytes(), "image/png")
            return self.respond(404, b"Not found", "text/plain")

        def log_message(self, fmt, *args):
            print("%s - %s" % (self.address_string(), fmt % args))
    return Handler


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--capture-dir", required=True, type=Path,
                   help="Directory containing frame_NNNNNN capture pairs")
    p.add_argument("--attest-key-file", required=True, type=Path,
                   help="Protected producer key outside capture directory")
    p.add_argument("--port", type=int, default=8765)
    opts = p.parse_args()
    if not 1 <= opts.port <= 65535:
        p.error("port must be 1..65535")
    try:
        key = load_attestation_key(opts.attest_key_file.expanduser())
    except (ValueError, OSError) as error:
        p.error(str(error))
    root = opts.capture_dir.expanduser().resolve()
    key_path = opts.attest_key_file.expanduser().resolve()
    if key_path == root or root in key_path.parents:
        p.error("Keep attestation key outside the capture directory")
    root.mkdir(parents=True, exist_ok=True)
    server = ThreadingHTTPServer(("127.0.0.1", opts.port),
                                 build_handler(root, Path(__file__).resolve().parent, key))
    print(f"ANVIL Visual Review: http://127.0.0.1:{opts.port}")
    print(f"Evidence directory: {root}")
    print("Evidence: producer-key authenticated; GPU origin not independently verified")
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == "__main__":
    main()
