# ANVIL matched visual review — experimental test workflow

Purpose: compare ANVIL baseline against ANVIL baseline + FSR 4.1 using two
real images captured from the SAME decoded video frame. This is the image-first
viewer discussed for the Home Field visual-review workflow, without any of
the abandoned PR #4 implementation.

## The immutable pipeline names

- A / ANVIL baseline: post-ANVIL causal reconstruction, GPU YUV-to-RGBA
  presentation, then a Lanczos spatial resize to the FSR presentation grid.
  Lanczos is for matched viewing dimensions; it is not counted as a
  temporal-reconstruction feature.
- B / ANVIL baseline + FSR 4.1 — tryout: the same ANVIL-reconstructed
  decoded frame, followed by a successful FSR 4.1 INT8 GPU dispatch and
  the actual FSR presentation output.

The current player baseline profile is explicitly labeled
'causal_cpu_previous_frame_v1'. It is the committed causal **live subset**
of ANVIL, not a claim that the complete offline runner's research stages
are now executing on the GPU or live playback path. The evidence manifests
preserve that identity rather than silently redefining the offline baseline.

The baseline never silently incorporates add-ons, and 'tryout' means initial
evaluation, not proven quality or performance. The current CPU ANVIL stage is
one-frame causal, not the future GPU/live engine. FSR 4.1 is still a temporal
upscaler with its own requirements, not literally a generic image filter.

## Build and capture on Linux

Use a Vulkan-capable AMD host with the local RE-derived FSR assets already
installed. Do not assume software-only CI proves a GPU path works.

    cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
    cmake --build build --parallel 2
    ctest --test-dir build --output-on-failure

Start a new capture directory for each source/experiment. This example uses
a deliberately generous CPU stage budget for acquiring still images; it is
NOT a real-time throughput benchmark:

    mkdir -p ./anvil-review-captures
    ANVIL_LIVE_PRE_FSR=1 \
    ANVIL_LIVE_BUDGET_MS=300 \
    ANVIL_REVIEW_CAPTURE_DIR="$PWD/anvil-review-captures" \
    ANVIL_REVIEW_FRAMES=48,96,144 \
    ./build/temporal_forge_player /absolute/path/to/video.mp4

During a paired capture session the player forces experimental FSR4.1 Quality,
with software decoding enabled for ANVIL. That temporary override is NOT
saved as the user's persistent player preference. Requested frames are
decoder frame indices (zero-based). For ordinary playback or throughput tests
omit the ANVIL_REVIEW_* variables; keep a realistic time budget (e.g. 6 ms).

Successful capture creates, for example:

    anvil-review-captures/frame_000048/
      anvil_baseline.png
      anvil_plus_fsr41.png
      manifest.json

PNG readbacks come from one processed frame under the existing GPU dispatch
lock. If ANVIL bypassed, the decoder frame identity changed, a valid FSR
dispatch did not occur, or GPU readback failed, no pair is claimed. The JSON
manifest is atomically committed last and includes source filename, frame
index, source PTS in microseconds and native ticks/timebase, resolution,
build SHA, stage timings, pipeline names, and image SHA256 hashes. Completed
pairs cannot be silently overwritten.

## Visual review

    python3 tools/anvil/review/server.py \
      --capture-dir "$PWD/anvil-review-captures" --port 8765

Open http://127.0.0.1:8765 locally.

The viewer provides selectable named frame thumbnails, matched display size,
wipe/split/fade/flicker/pixel-difference modes, A/B swap, synchronized pan and
zoom, frame navigation and a visible metadata inspector. Shortcuts 1–5
select modes and left/right arrows select frames. All five comparison modes
use only captured pixels. No fake placeholder can be mistaken for evidence.

The stdlib-only server validates SHA256 and PNG dimensions, source frame
identity, capture schema and allowed file names before exposing any images.
Malformed or missing pairs are excluded from the viewer. It serves only the
explicitly selected evidence directory on 127.0.0.1.

## Honest limitations

- The CPU ANVIL estimator may miss even the 300-ms stage budget on larger
  inputs. Its budget limiter disables it after a miss; skipped captures are
  valid failures, not permission to substitute an unprocessed image.
- Capture performs synchronous GPU readback and CPU Lanczos scaling,
  impacting playback latency. Capture timings do NOT prove 30/60 FPS.
- A uses Lanczos for matched presentation size; B includes the FSR4
  presentation scaler. Therefore differences cannot all be attributed
  solely to FSR's neural reconstruction until this is controlled separately.
- HDR color handling, dynamic scene boundaries, independent quality metrics,
  motion stability and performance qualification still require GPU-host tests.
- FSR remains a replaceable tryout, not a hard-coded ANVIL foundation.
- The historical canonical review harness and abandoned PR #4 are untouched.

This is an experimental, reproducible STILL-FRAME review workflow. Results
cannot be promoted to rookie/starter without actual visual evaluation,
matched performance measurements, verified provenance and user approval.
