# ANVIL player interface — UI contract

The player is the real-time product surface. This UI refresh is limited to QML
presentation and control wiring: it does not merge the offline ANVIL runner into
the playback engine or change FSR GPU execution.

## Design rules

- No emoji or icon fonts. Shared `VectorIcon.qml` paints crisp, scalable
  transport and settings symbols using QtQuick Canvas; no SVG image plugin,
  downloaded fonts, or external assets are required.
- Dark, high-contrast chrome with a restrained green active color, defined by
  `PlayerTheme.qml`. Make hover, checked, focus and disabled states distinct.
- The video occupies the whole window. Controls overlay the lower edge,
  remain available while hovered, and show action-specific tooltips.
- Small windows hide secondary controls, not play/pause or primary transport.
  All hidden functions remain available in settings or via shortcuts.
- Labels must describe the functionality that *actually exists*. The old
  playback-speed selector was a display-only control and has been removed.
  Frame stepping has been removed from the UI because the paused render\n  queue cannot actually present a step; an honest step control needs an\n  engine-level implementation.
- Compare-pane labels must identify the actual pixel-generation stage. The
  raw pane is labeled SOURCE only while `playback.anvilLiveStage` is not
  `reconstructing`; under an engaged opt-in causal ANVIL stage those pixels
  are post-ANVIL and the pane says so.
- Seeking while dragging previews the selected position and issues one seek
  upon release, rather than flooding the decoder with new seek requests.
- The causal ANVIL live reconstruction stage is connected to real-time
  playback as an experimental opt-in (`ANVIL_LIVE_PRE_FSR=1`); the settings
  status text and HUD report its actual state (`off`, `standby`,
  `reconstructing`, `bypassed`). The full ANVIL roster remains a separate
  engineering milestone: the current FSR preset selector is legacy playback
  state, not an ANVIL candidate roster.

## Verification

- CMake Qt/QML build checks component registration and QML validity.
- `python3 -m pytest -q tests/test_player_ui_contract.py` checks for
  accidental emoji reintroduction, inert playback-speed controls, the A/B\n  swap path, stage-aware compare labels, truthful ANVIL live-stage status\n  text, and the real status shown by the HUD.
- Manual acceptance (needs a real Qt/Vulkan player session): launch with no
  media; open a file; play/pause; scrub; mute; open playlist and
  settings; check keyboard focus; test 760px and 1280px layouts; enter/exit
  fullscreen; verify compare-side swapping, divider-only dragging, and the context-menu controls. Repeat the compare and settings checks once with
  `ANVIL_LIVE_PRE_FSR=1` and confirm the raw pane and status text identify
  the ANVIL stage instead of claiming SOURCE/not-connected. Do not claim
  this last validation is complete without an actual run.

## Out of scope

GPU/FSR dispatch policy, frame pacing, audio engine, codec selection, roster
implementation, performance claims, and backend placement.

## Known missing player capabilities

- True source-frame stepping while paused is not implemented in `PlaybackEngine`.
  The former buttons sought by an arbitrary fraction of a second; they are
  intentionally hidden until a real engine API and tests exist.
- Playback speed is not adjustable in the current engine. A decorative selector
  is not acceptable.
- FSR 3.1.5 is not linked in the clean build; non-FSR4 backend choices turn
  off experimental GPU dispatch. The settings panel says this explicitly.
- The causal CPU ANVIL live stage is connected as an experimental opt-in;
  GPU/zero-copy reconstruction and roster-driven backend selection remain
  future work and must not be described as completed.


### Published-frame provenance (PR8-F0001)

The raw comparison pane's label reads `playback.anvilLiveStage`, which is
committed after successful GPU image publication, not speculative decode.
Pending or failed Vulkan work retains the previous visible image's stage.
`POST-ANVIL` denotes genuinely reconstructed pixels; `SOURCE` denotes
unmodified decoded pixels. The settings Published Frame Stage and HUD use
this same identity. Manually test default-off, opted-in reconstructed,
unsupported/discontinuous, budget miss, failed upload/dispatch, both compare
orientations and both window widths on Qt/Vulkan before reviewer acceptance.

### EASU-only comparison (PR8-F0002)

With neural FSR disabled, the spatial output and native-resolution control
come from the same successfully published uploader. Neural harness readiness
is not required for this control. Upload/reallocation withdraws the spatial
pair until conversion, scaling and GPU submission succeed. A raw-only or
split comparison with unavailable input renders no video; it never substitutes
processed output or a cached input from a previous publication.

`native_compare_gpu_tests` runs the real decoder, spatial uploader and Qt
Vulkan scene graph, checking distinct native/control and spatial image handles,
both split orientations, missing publication, mode switching and recovery.
It returns 77 when no display/device is available; such a skip is not runtime
proof. Run it with the generated `build/tests/compare.mp4` fixture both normally
and with `ANVIL_LIVE_PRE_FSR=1`. The small fixture permits real CPU reconstruction;
CPU history uses decoded-frame continuity rather than FSR dispatch completion.
This behavioral test supplements the manual acceptance checklist above.
