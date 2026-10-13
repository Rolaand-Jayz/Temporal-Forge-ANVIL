# ANVIL causal live subset: paired visual experiment

## Scientific identity

This paired viewer does **not** execute or redefine the canonical frozen
**ANVIL baseline**. Current native playback uses the explicitly identified
`causal_cpu_previous_frame_v1` subset. Do not equate its quality with the full
research baseline or presume that its experimental FSR4.1 attachment is useful.

- **Arm A — ANVIL causal live subset v1 (control).** Post-reconstruction
  YUV→RGBA presentation, with Lanczos resize to match the comparison grid.
- **Arm B — ANVIL causal live subset v1 + FSR 4.1 RE (tryout).** The same
  decoded/reconstructed frame entering experimental FSR and presentation.

The control's Lanczos grid-alignment and the experimental arm's presentation
scaling are different operations. Any visual difference cannot automatically
be attributed to FSR neural reconstruction. These are static image captures;
real-time 30/60 FPS is not demonstrated by this workflow.

## Authenticated capture setup

The old unsigned `anvil-visual-pair-v1` directory is never accepted as
scientific evidence. The `v2` producer writes two PNGs, SHA-256 hashes,
frame and timestamp identifiers, a manifest, and `capture.hmac` calculated
with a randomly generated 32-byte secret kept outside capture storage.

On the trusted capture machine, create a fresh secret:

```sh
umask 077
head -c 32 /dev/urandom > "$HOME/.anvil-review-key"
chmod 600 "$HOME/.anvil-review-key"
```

Never place the secret in the capture directory, repository, artifacts,
review comments, logs, or exports. Possession of the key can forge captures.
HMAC proves possession of the protected producer key and that signed manifest
bytes were not altered. An independently observed AMD GPU execution is still
required to establish genuine frame provenance.

Build on the experimental PR #7 branch, with the PR #6 causal live
reconstruction changes present:

```sh
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build --parallel 2
ctest --test-dir build --output-on-failure
```

Capture a short SDR clip first. The generous CPU budget is only for still
capture and is NOT a valid throughput measurement:

```sh
mkdir -p anvil-review-captures
ANVIL_LIVE_PRE_FSR=1 \
ANVIL_LIVE_BUDGET_MS=300 \
ANVIL_REVIEW_ATTEST_KEY_FILE="$HOME/.anvil-review-key" \
ANVIL_REVIEW_CAPTURE_DIR="$PWD/anvil-review-captures" \
ANVIL_REVIEW_FRAMES=48,96,144 \
./build/temporal_forge_player /absolute/path/to/short-sdr-video.mp4
```

If ANVIL bypasses, exceeds budget or FSR fails, no valid pair is generated.
Capture readbacks stall playback; this is a visual experiment only.

## Review session

```sh
python3 tools/anvil/review/server.py \
  --capture-dir "$PWD/anvil-review-captures" \
  --attest-key-file "$HOME/.anvil-review-key" \
  --port 8765
```

Open http://127.0.0.1:8765. The local server only publishes pairs passing
producer-key authentication, expected schema/profile/roles, SHA-256 hashes,
full PNG CRC/chunk/inflate/scanline decode and declared dimensions.
The HTTP image endpoint performs the same verification.

Wipe, split, fade, flicker, pixel difference, A/B swap and synchronized
pan/zoom support the visual review. The browser reports **producer-key
authenticated — NOT independently GPU-verified**. The canonical frozen
baseline remains unchanged.

## Remaining acceptance gates

- Negative tests for forged/unkeyed evidence, modified manifest and image,
  header-only/truncated/corrupt PNG, wrong frame IDs and HTTP bypass.
- Native C++ capture contract and active Release negative-control CTest.
- New exact-head CI on both repaired branches.
- Genuine GPU capture witnessed on the target Radeon hardware, binary/source
  provenance, browser interaction review and 30/60 FPS throughput separately.
- Independent evaluator re-review and owner approval before any merge or
  candidate promotion. No result is a quality win by default.
