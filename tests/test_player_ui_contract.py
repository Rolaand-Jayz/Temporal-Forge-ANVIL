"""Source-level UI guardrails; Qt's QML compiler remains the integration check."""
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[1]
QML = ROOT / "resources" / "qml"


def source(name):
    return (QML / name).read_text(encoding="utf-8")


def test_player_interface_has_no_emoji_icons():
    # Avoid platform-dependent emoji rendering in shipped QML. Non-BMP emoji
    # covers the original transport/settings failures; explicit source guards
    # cover the common monochrome emoji presentation characters too.
    for path in QML.glob("*.qml"):
        data = path.read_text(encoding="utf-8")
        assert not [ch for ch in data if ord(ch) > 0xFFFF], path.name
    for path in ("Main.qml", "SettingsDialog.qml", "VideoContextMenu.qml"):
        data = source(path)
        assert not any(token in data for token in ("⚙", "⚠", "▶", "⏮", "⏭", "⇄", "☷")), path


def test_icons_are_vector_and_registered():
    cmake = (ROOT / "CMakeLists.txt").read_text(encoding="utf-8")
    for component in ("VectorIcon.qml", "PlayerTheme.qml", "IconButton.qml"):
        assert "resources/qml/" + component in cmake
    icons = source("VectorIcon.qml")
    assert "Canvas {" in icons and "onPaint:" in icons
    assert "iconName:" in source("Main.qml")
    assert "glyph:" not in source("Main.qml")
    assert "glyph:" not in source("IconButton.qml")


def test_transport_ui_does_not_pretend_to_control_playback_speed():
    ui = source("Main.qml")
    assert "playbackSpeeds" not in ui
    assert "stepPlaybackSpeed(" not in ui
    assert "onMoved: playback.seekUs" not in ui
    # A seek did not display a new frame while paused. Do not label it
    # as frame stepping until the playback engine has true single-step support.
    assert "function seekFrame(direction)" not in ui
    assert "onClicked: seekFrame(" not in ui
    assert 'sequence: ","' not in ui
    assert 'sequence: "."' not in ui


def test_all_static_vector_icon_names_are_drawn():
    # Dynamic names on Main.qml are covered by the playback-state pairs.
    icons = source("VectorIcon.qml")
    names = set(re.findall(r'n === "([^"]+)"', icons))
    candidates = set(re.findall(r'iconName: "([^"]+)"', source("Main.qml")))
    candidates |= set(re.findall(r'(?m)^\s*name: "([^"]+)"', source("SettingsDialog.qml")))
    candidates |= set(re.findall(r'iconName: "([^"]+)"', source("SettingsDialog.qml")))
    candidates |= {"play", "pause", "volume-x", "volume-low", "volume-high",
                   "maximize", "minimize", "folder-open"}
    assert candidates <= names, sorted(candidates - names)


def test_comparison_swap_and_divider_are_wired_to_rendering():
    ui = source("Main.qml")
    assert "x: compare.rawOnLeft ? 0" in ui
    assert "width: compare.rawOnLeft ? surfaceArea.width" in ui
    assert "x: -compareRawPane.x" in ui
    assert "id: compareDividerDrag" in ui
    assert "anchors.fill: parent\n            enabled: compare.active" not in ui
    assert "PLAYER OUTPUT" in ui and "SOURCE" in ui


def test_canvas_redraw_clears_previous_icon():
    icons = source("VectorIcon.qml")
    assert "ctx.reset()" in icons
    assert "ctx.clearRect(0, 0, width, height)" in icons


def test_hud_does_not_reuse_stale_fsr_timing_when_disabled():
    hud = source("LiveHud.qml")
    assert 'if (!playback.fsr4Active) return source + "  ·  FSR INACTIVE"' in hud
    assert "FSR 4.1 RE" in hud
