# ANVIL live decode → reconstruction → FSR integration (first slice)

**What runs:** on the existing video decoder thread, if opted in,
`DecodedVideoFrame` first visits `anvil::LiveFrameProcessor`, then continues
through the unchanged historical side-buffer/FSR4 upload and presentation path.
This is an **actual** opt-in integration, not the offline `anvil_runner` output
file writer. The default player path is unchanged.

**Try it:** set `ANVIL_LIVE_PRE_FSR=1` when launching the native player.
Optional `ANVIL_LIVE_BUDGET_MS=6` is the stage-only CPU work budget
(0.1–1000 ms; invalid values select 6 ms). Default budget 6 ms. The console
logs the first reconstructed frame, every 300 reconstructed frames, and the
first budget failure. This opt-in is intended for careful placement testing,
not a release-quality preset.

**Algorithm:** first frame primes a causal one-frame window. The next decoded
software YUV 4:2:0 frame uses existing ANVIL block correspondence, coverage,
confidence and temporal accumulation; result replaces only decoded pixel
planes prior to any FSR dispatch. Raw observations, exact PTS/ticks, color
metadata, frame indices and codec side information are preserved. This is
not a future-frame configuration, a completed roster, or a general shader.

**Player UI status:** the engine exposes the folded live-stage state as the
`playback.anvilLiveStage` property (`off`, `standby`, `reconstructing`,
`bypassed`). The state tracks the **last successfully published image**, not the latest
frame decoded/submitted to the GPU. `standby` means published decoder-owned
pixels (priming, discontinuity, unsupported or no correspondence),
`reconstructing` means published post-ANVIL pixels, and `bypassed` means
published decoder-owned pixels after the budget deadline was exceeded.
A budget miss may disable future reconstruction before a bypassed image is
published; the UI must retain its old displayed stage in that interval.
The native compare pane labels the raw side `SOURCE` only while it genuinely
displays decoder-owned pixels; under an engaged stage it labels them
post-ANVIL, and the settings Live Status line and the HUD report the same
state. The browser reviewer is a separate surface with its own provenance
labeling.

**Fail open:** missing/unsupported format, GPU-backed frame, malformed plane,
scene discontinuity, and absent proven correspondence leave decoded pixels
unchanged. A stage budget miss discards the computed result and permanently
disables the opt-in stage for that playback instance, retaining ordinary
playback. Seek and decoder flush discard old raw-frame history. This avoids
continuing a CPU path that has already exceeded its latency allowance;
**one first budget miss can still cause a single-frame stall**, and the stage
does not guarantee frame rate.

**Needed for full product:** genuine GPU/zero-copy processing, separate bounded
decode/reconstruction/presentation scheduling, lookahead, measured sustained
30/60 FPS with A/V sync, and a replaceable roster-selected finishing stage.
The existing FSR4 path still has its own temporal inputs; merely placing
ANVIL before it does **not** make FSR4 an ordinary postprocessing filter.

**Verify:** build `anvil_live_frame_tests` and run it under CTest, plus
the existing player and ANVIL regression suites. Visually test the opt-in on
short clips with actual hardware and inspect frame pacing. No runtime quality
or FPS performance claim is made from passing unit tests alone.


### Native published-frame stage ownership (PR8-F0001)

`playback.anvilLiveStage` is committed only with a successfully published
Vulkan image, not when decoding completes. The synchronous FSR/native/EASU
paths publish image and provenance together; asynchronous dual-uploader
slots retain per-slot stage until completion, including the final presentation
scaler. Upload, dispatch, fence and scaler failures leave the previously
published provenance unchanged. The compare pane must not call post-ANVIL
pixels SOURCE. Native Qt/Vulkan verification on AMD hardware remains a separate
mandatory review gate.
