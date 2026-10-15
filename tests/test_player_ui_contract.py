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


def test_compare_pane_labels_identify_the_actual_pixel_stage():
    # PR8-F0001: the raw compare pane is decoder-owned SOURCE only while the
    # opt-in causal ANVIL live stage is not reconstructing. A bare
    # unconditional SOURCE label would relabel post-ANVIL pixels.
    ui = source("Main.qml")
    # Both pane labels must route through the stage-aware helper.
    assert 'text: compare.rawOnLeft ? rawPaneStageLabel() : "PLAYER OUTPUT"' in ui
    assert 'text: compare.rawOnLeft ? "PLAYER OUTPUT" : rawPaneStageLabel()' in ui
    assert "function rawPaneStageLabel()" in ui
    # The helper must distinguish the reconstructing stage from SOURCE and
    # bind to the engine property, so the label follows the actual pixels.
    assert 'playback.anvilLiveStage === "reconstructing"' in ui
    assert '"POST-ANVIL" : "SOURCE"' in ui
    # The pre-repair unconditional labels are gone.
    assert '? "SOURCE" : "PLAYER OUTPUT"' not in ui
    assert '? "PLAYER OUTPUT" : "SOURCE"' not in ui
    # The stale claim that ANVIL processing is not connected must not return.
    assert "not currently connected" not in ui


def test_settings_status_text_is_truthful_about_the_live_stage():
    # PR8-F0001: settings must state when the causal ANVIL live stage is
    # available/active while distinguishing the incomplete roster, in both
    # the default-off and opt-in states.
    settings = source("SettingsDialog.qml")
    assert "not yet connected" not in settings
    assert "ANVIL_LIVE_PRE_FSR=1" in settings
    assert "opt-in" in settings or "opt in" in settings
    # The full roster must stay described as not connected (it is).
    assert "full ANVIL roster is not connected" in settings
    # Live status must bind to the engine's stage property with distinct
    # user-visible wording per state, including the default-off state.
    assert 'playback.anvilLiveStage === "reconstructing"' in settings
    assert 'playback.anvilLiveStage === "standby"' in settings
    assert 'playback.anvilLiveStage === "bypassed"' in settings
    assert "ANVIL live: off (opt in with ANVIL_LIVE_PRE_FSR=1)" in settings
    hud = source("LiveHud.qml")
    # The HUD keeps the stage visible while it is opted in; FSR-only wording
    # would imply a pure decode -> FSR chain under an engaged stage.
    assert 'playback.anvilLiveStage === "reconstructing"' in hud
    assert "ANVIL RECONSTRUCTING" in hud
    assert "ANVIL BYPASSED" in hud


def test_anvil_status_is_published_only_after_completed_gpu_frame():
    engine = (ROOT / "src/core/PlaybackEngine.cpp").read_text(encoding="utf-8")
    header = (ROOT / "src/anvil/LiveFrameProcessor.hpp").read_text(encoding="utf-8")
    decode = engine[engine.index("const auto liveResult = anvilLive.process(df, decodedReset);"):
                    engine.index("promoteStableFsrViewport();")]
    assert "publishAnvilLiveStage" not in decode
    assert "class LivePublicationTracker" in header
    assert "anvilPublication.submit(" in engine
    assert "anvilPublication.retire(" in engine
    assert "anvilPublication.publishDirect(" in engine
    assert engine.count("publishAnvilLiveStage(anvilPublication.publishDirect(") == 3
    assert engine.count("anvilPublication.retire(") == 3
    assert "firstUploader == fsr4Uploader_.get() ? 0u : 1u, true" in engine
    assert "firstUploader == fsr4Uploader_.get() ? 0u : 1u, false" in engine


def test_canvas_redraw_clears_previous_icon():
    icons = source("VectorIcon.qml")
    assert "ctx.reset()" in icons
    assert "ctx.clearRect(0, 0, width, height)" in icons


def test_hud_does_not_reuse_stale_fsr_timing_when_disabled():
    hud = source("LiveHud.qml")
    # The anvil segment is composed before the inactive branch, so the guard
    # still returns without touching lastFsr4DispatchMs.
    assert 'if (!playback.fsr4Active)\n                    return source + anvil + "  ·  FSR INACTIVE"' in hud
    assert "FSR 4.1 RE" in hud
