"""PR7 visual-review evidence regression suite (mechanism, not GPU origin proof)."""
import hashlib
import hmac
import importlib.util
import json
import stat
import struct
import tempfile
import threading
import unittest
import zlib
from http.server import ThreadingHTTPServer
from pathlib import Path
from urllib.error import HTTPError
from urllib.request import urlopen

SERVER = Path(__file__).resolve().parents[1] / "tools" / "anvil" / "review" / "server.py"
spec = importlib.util.spec_from_file_location("anvil_review_server", SERVER)
mod = importlib.util.module_from_spec(spec)
spec.loader.exec_module(mod)
KEY = bytes(range(32))


def chunk(tag, payload):
    return (struct.pack(">I", len(payload)) + tag + payload +
            struct.pack(">I", zlib.crc32(tag + payload) & 0xffffffff))


def png(w=2, h=2, rgba=b"\x22\x44\x66\xff"):
    rows = b"".join(b"\x00" + rgba * w for _ in range(h))
    return (b"\x89PNG\r\n\x1a\n" +
            chunk(b"IHDR", struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)) +
            chunk(b"IDAT", zlib.compress(rows)) + chunk(b"IEND", b""))


def sign(folder):
    raw = (folder / "manifest.json").read_bytes()
    (folder / "capture.hmac").write_text(hmac.new(KEY, raw, hashlib.sha256).hexdigest())


def fixture(root, signed=True, frame=48):
    folder = root / f"frame_{frame:06d}"
    folder.mkdir()
    images = ("anvil_causal_live_control.png", "anvil_causal_live_fsr41.png")
    left, right = png(), png()
    (folder / images[0]).write_bytes(left)
    (folder / images[1]).write_bytes(right)
    obj = {
        "schema": "anvil-visual-pair-v2", "verified_same_frame": True,
        "capture_attestation": "producer-hmac-sha256-v1",
        "anvil_live_profile": "causal_cpu_previous_frame_v1",
        "frame_index": frame, "pts_us": frame * 33333,
        "pts_ticks": frame, "tb_num": 1, "tb_den": 30,
        "source_width": 2, "source_height": 2, "output_width": 2, "output_height": 2,
        "a": {"image": images[0], "status": "control",
              "sha256": hashlib.sha256(left).hexdigest()},
        "b": {"image": images[1], "status": "tryout",
              "sha256": hashlib.sha256(right).hexdigest()},
    }
    (folder / "manifest.json").write_text(json.dumps(obj))
    if signed:
        sign(folder)
    return folder, obj


class CaptureEvidenceTests(unittest.TestCase):
    def setUp(self):
        self.temp = tempfile.TemporaryDirectory()
        self.root = Path(self.temp.name)

    def tearDown(self):
        self.temp.cleanup()

    def test_complete_png_and_producer_hmac_accept(self):
        folder, _ = fixture(self.root)
        self.assertEqual(mod.decoded_png_dimensions(folder / "anvil_causal_live_control.png"), (2, 2))
        self.assertEqual(mod.verified_pair(folder, KEY)["frame_index"], 48)
        self.assertEqual(mod.catalog(self.root, KEY)["pairs"][0]["evidence_status"],
                         "producer-key-authenticated-not-independent-gpu-proof")
        self.assertIsNone(mod.verified_pair(folder, b""))
        self.assertIsNone(mod.verified_pair(folder, b"X" * 32))

    def test_self_attestation_does_not_upgrade_pair(self):
        folder, record = fixture(self.root, signed=False)
        self.assertIsNone(mod.verified_pair(folder, KEY))
        (folder / "capture.hmac").write_text("0" * 64)
        self.assertIsNone(mod.verified_pair(folder, KEY))
        self.assertEqual(mod.catalog(self.root, KEY)["pairs"], [])

    def test_changed_png_and_resigned_image_hash_without_producer_key(self):
        folder, record = fixture(self.root)
        new_image = png(2, 2, b"\x10\x20\x30\xff")
        target = folder / "anvil_causal_live_fsr41.png"
        target.write_bytes(new_image)
        record["b"]["sha256"] = hashlib.sha256(new_image).hexdigest()
        (folder / "manifest.json").write_text(json.dumps(record))
        self.assertIsNone(mod.verified_pair(folder, KEY))

    def test_header_only_png_rejected_even_when_producer_signed(self):
        folder, record = fixture(self.root)
        short = (folder / "anvil_causal_live_control.png").read_bytes()[:24]
        (folder / "anvil_causal_live_control.png").write_bytes(short)
        record["a"]["sha256"] = hashlib.sha256(short).hexdigest()
        (folder / "manifest.json").write_text(json.dumps(record))
        sign(folder)
        self.assertIsNone(mod.verified_pair(folder, KEY))
        self.assertEqual(mod.catalog(self.root, KEY)["rejected"], ["frame_000048"])

    def test_crc_and_truncated_png_fail(self):
        folder, record = fixture(self.root)
        valid = (folder / "anvil_causal_live_fsr41.png").read_bytes()
        for corrupted in (valid[:24], valid[:-1], valid[:-12], valid[:52] + b"z" + valid[53:]):
            (folder / "anvil_causal_live_fsr41.png").write_bytes(corrupted)
            record["b"]["sha256"] = hashlib.sha256(corrupted).hexdigest()
            (folder / "manifest.json").write_text(json.dumps(record))
            sign(folder)
            self.assertIsNone(mod.verified_pair(folder, KEY))

    def test_wrong_frame_identity_and_symlink_rejected(self):
        folder, record = fixture(self.root)
        record["frame_index"] = 49
        (folder / "manifest.json").write_text(json.dumps(record))
        sign(folder)
        self.assertIsNone(mod.verified_pair(folder, KEY))
        (self.root / "frame_000049").symlink_to(folder, target_is_directory=True)
        self.assertEqual(mod.catalog(self.root, KEY)["pairs"], [])

    def with_server(self, task):
        server = ThreadingHTTPServer(("127.0.0.1", 0),
                                     mod.build_handler(self.root, SERVER.parent, KEY))
        worker = threading.Thread(target=server.serve_forever, daemon=True)
        worker.start()
        try:
            return task("http://127.0.0.1:" + str(server.server_port))
        finally:
            server.shutdown()
            server.server_close()
            worker.join(timeout=3)

    def test_http_rejects_bogus_png_with_matching_hmac(self):
        folder, record = fixture(self.root)
        short = (folder / "anvil_causal_live_control.png").read_bytes()[:24]
        (folder / "anvil_causal_live_control.png").write_bytes(short)
        record["a"]["sha256"] = hashlib.sha256(short).hexdigest()
        (folder / "manifest.json").write_text(json.dumps(record))
        sign(folder)
        def verify(base):
            with urlopen(base + "/api/catalog") as response:
                self.assertEqual(json.load(response)["pairs"], [])
            with self.assertRaises(HTTPError) as error:
                urlopen(base + "/api/image/frame_000048/anvil_causal_live_control.png")
            self.assertEqual(error.exception.code, 409)
        self.with_server(verify)

    def test_http_accepts_complete_image(self):
        folder, record = fixture(self.root)
        def verify(base):
            with urlopen(base + "/api/image/frame_000048/anvil_causal_live_fsr41.png") as response:
                self.assertEqual(response.status, 200)
                self.assertEqual(response.read(), (folder / "anvil_causal_live_fsr41.png").read_bytes())
        self.with_server(verify)

    def test_secret_key_permission_and_length_gate(self):
        key = self.root / "producer.key"
        key.write_bytes(KEY)
        key.chmod(0o600)
        self.assertEqual(mod.load_attestation_key(key), KEY)
        key.chmod(0o644)
        with self.assertRaises(ValueError):
            mod.load_attestation_key(key)
        key.chmod(0o600)
        key.write_bytes(b"weak")
        with self.assertRaises(ValueError):
            mod.load_attestation_key(key)

    def test_ui_is_honest_about_profile_and_provenance(self):
        html = (SERVER.parent / "index.html").read_text()
        js = (SERVER.parent / "app.js").read_text()
        self.assertIn("causal live subset", html)
        self.assertNotIn("ANVIL baseline", html)
        self.assertIn("PRODUCER-KEY AUTHENTICATED", js)
        self.assertIn("independent GPU proof pending", js)
        for mode in ("wipe", "split", "fade", "flicker", "difference"):
            self.assertIn('data-mode="' + mode + '"', html)


if __name__ == "__main__":
    unittest.main()
