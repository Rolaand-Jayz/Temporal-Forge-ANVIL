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
  The seek-based frame step is explicitly approximate, not exact stepping.
- Seeking while dragging previews the selected position and issues one seek
  upon release, rather than flooding the decoder with new seek requests.
- Player UI and the ANVIL reconstruction/roster integration are separate
  engineering milestones. The current FSR preset selector is legacy playback
  state, not an ANVIL candidate roster.

## Verification

- CMake Qt/QML build checks component registration and QML validity.
- `python3 -m pytest -q tests/test_player_ui_contract.py` checks for
  accidental emoji reintroduction and inert playback-speed controls.
- Manual acceptance (needs a real Qt/Vulkan player session): launch with no
  media; open a file; play/pause; scrub; step; mute; open playlist and
  settings; check keyboard focus; test 760px and 1280px layouts; enter/exit
  fullscreen; verify the compare and context-menu controls. Do not claim
  this last validation is complete without an actual run.

## Out of scope

GPU/FSR dispatch policy, frame pacing, audio engine, codec selection, roster
implementation, performance claims, and backend placement.
