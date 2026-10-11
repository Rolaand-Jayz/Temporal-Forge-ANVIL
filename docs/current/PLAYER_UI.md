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
- Seeking while dragging previews the selected position and issues one seek
  upon release, rather than flooding the decoder with new seek requests.
- Player UI and the ANVIL reconstruction/roster integration are separate
  engineering milestones. The current FSR preset selector is legacy playback
  state, not an ANVIL candidate roster.

## Verification

- CMake Qt/QML build checks component registration and QML validity.
- `python3 -m pytest -q tests/test_player_ui_contract.py` checks for
  accidental emoji reintroduction, inert playback-speed controls, the A/B\n  swap path, and the real status shown by the HUD.
- Manual acceptance (needs a real Qt/Vulkan player session): launch with no
  media; open a file; play/pause; scrub; mute; open playlist and
  settings; check keyboard focus; test 760px and 1280px layouts; enter/exit
  fullscreen; verify compare-side swapping, divider-only dragging, and the context-menu controls. Do not claim
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
- ANVIL reconstruction and roster-driven FSR placement are future development;
  this player UI must not pretend either already runs.
