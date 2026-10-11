// Main.qml — standardized media player UI.
//
// Layout (VLC/MPV-class conventions):
//   - Full-bleed video surface.
//   - Bottom control overlay: draggable seek bar on top, buttons below.
//   - Top-right: optional debug + media info panels (toggleable, auto-hide).
//   - Hover-aware chrome: every control surface hides itself after the mouse
//     has been idle for a few seconds, and only comes back when the mouse
//     enters the region it lives in (or anywhere, in fullscreen). This is
//     the standard player behavior the user asked for.
//
// Keyboard shortcuts are limited to controls implemented by PlaybackEngine.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Dialogs
import TemporalForge

ApplicationWindow {
    id: window
    visible: false
    width: 1280; height: 720
    minimumWidth: 760; minimumHeight: 480
    color: "#0b1015"
    title: "ANVIL Player" + (playback.mediaTitle ? " — " + playback.mediaTitle : "")

    PlayerTheme { id: theme }

    // --- auto-hide state ---
    // True while the chrome (controls + overlays) is visible. Drops to false
    // after the mouse has been still for a few seconds, and rises again on
    // any pointer motion or when the mouse enters one of the overlay regions.
    property bool chromeVisible: true
    // True while the user is actively hovering a control region; that region
    // never auto-hides while hovered.
    property bool controlsHovered: false
    property bool overlaysHovered: false

    // --- open-file dialog ---
    FileDialog {
        id: openDialog
        title: "Open videos"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Video files (*.mp4 *.mkv *.mov *.webm *.avi *.ts *.m2ts)", "All files (*)"]
        onAccepted: playback.openPlaylist(urlsToEntries(selectedFiles))
    }

    FileDialog {
        id: addDialog
        title: "Add videos to playlist"
        fileMode: FileDialog.OpenFiles
        nameFilters: ["Video files (*.mp4 *.mkv *.mov *.webm *.avi *.ts *.m2ts)", "All files (*)"]
        onAccepted: playback.appendPlaylist(urlsToEntries(selectedFiles))
    }

    function urlsToEntries(urls) {
        const entries = []
        for (let i = 0; i < urls.length; ++i)
            entries.push(String(urls[i]))
        return entries
    }

    // A compact playlist drawer. The engine owns ordering and end-of-media
    // advancement; this view only selects an existing item or adds files.
    Popup {
        id: playlistPopup
        width: Math.min(460, Math.max(320, window.width - 24))
        height: Math.min(520, Math.max(260, window.height - 120))
        x: Math.max(12, window.width - width - 12)
        y: 12
        padding: 12
        modal: false
        focus: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: "#151d25"
            radius: 8
            border.color: "#30414b"
            border.width: 1
        }

        contentItem: ColumnLayout {
            spacing: 8

            RowLayout {
                Layout.fillWidth: true
                Label {
                    text: "Playlist (" + playback.playlistCount + ")"
                    color: "#f1f6f5"
                    font.bold: true
                    Layout.fillWidth: true
                }
                Button {
                    text: "Add…"
                    onClicked: addDialog.open()
                }
                Button {
                    text: "Clear"
                    enabled: playback.playlistCount > 0
                    onClicked: {
                        playback.clearPlaylist()
                        playlistPopup.close()
                    }
                }
            }

            Label {
                visible: playback.playlistCount === 0
                text: "No videos in the playlist"
                color: "#81949e"
                Layout.fillWidth: true
                horizontalAlignment: Text.AlignHCenter
            }

            ListView {
                id: playlistView
                visible: playback.playlistCount > 0
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 2
                model: playback.playlist

                delegate: ItemDelegate {
                    width: playlistView.width
                    highlighted: index === playback.playlistIndex
                    text: {
                        const value = String(modelData)
                        const slash = Math.max(value.lastIndexOf("/"), value.lastIndexOf("\\"))
                        return slash >= 0 ? value.substring(slash + 1) : value
                    }
                    onClicked: {
                        playback.selectPlaylist(index)
                        playlistPopup.close()
                    }
                    ToolTip.visible: hovered
                    ToolTip.text: String(modelData)
                    ToolTip.delay: 500
                }
            }
        }
    }

    // --- FSR4 INT8 experimental warning ---
    Dialog {
        id: experimentalWarning
        title: "FSR 4.1 INT8 — Experimental"
        modal: true
        anchors.centerIn: parent
        standardButtons: Dialog.Ok | Dialog.Cancel
        Label {
            text: "FSR 4.1 INT8 is an experimental, RE-derived video upscaler for RDNA3. Validation here covers local GPU execution, finite output, bounds, and visual A/B inspection. AMD's Windows runtime and an official video-FSR reference are outside this project's definition of done."
            wrapMode: Text.Wrap
            width: parent ? parent.width - 24 : 420
        }
    }

    // Track window size for the FSR info chain (spec 02: window size only
    // affects presentation, never the FSR target).
    Connections {
        target: window
        function onWidthChanged() { fsr.setWindowSize(surfaceArea.width, surfaceArea.height) }
        function onHeightChanged() { fsr.setWindowSize(surfaceArea.width, surfaceArea.height) }
    }

    Component.onCompleted: {
        fsr.setWindowSize(surfaceArea.width, surfaceArea.height)
        playback.compareEnabled = compare.active
    }

    // Refresh FSR source dims when media changes.
    Connections {
        target: playback
        function onMediaChanged() { fsr.setWindowSize(surfaceArea.width, surfaceArea.height) }
    }
    Connections {
        target: compare
        function onChanged() { playback.compareEnabled = compare.active }
    }

    // --- video surface ---
    // Fills the entire window. The control overlay floats above it; the
    // overlay used to be a ToolBar footer which reserved its own space and
    // squashed the video — the user reported that the controls did not feel
    // like a normal player. Floating overlay is the standard.
    Item {
        id: surfaceArea
        anchors.fill: parent
        clip: true

        Rectangle { anchors.fill: parent; color: "#000000" }

        // Video surface: imports the completed native Vulkan image written by
        // the real FSR4 dispatch. The refresh Timer only advances frame
        // pacing and schedules a scene-graph update.
        VideoSurface {
            id: videoSurface
            anchors.fill: parent
            visible: playback.hasMedia
            brightness: videoSettings.brightness
            contrast: videoSettings.contrast
            saturation: videoSettings.saturation
            hue: videoSettings.hue
            gamma: videoSettings.gamma
            presentationScaler: videoSettings.presentationScaler
            compareActive: false
            compareRawOnLeft: false
        }

        Item {
            id: compareRawPane
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            width: parent.width * compare.splitPosition
            visible: playback.hasMedia && compare.active
            clip: true
            z: 1

            VideoSurface {
                id: compareRawSurface
                width: compareRawPane.parent.width
                height: compareRawPane.parent.height
                brightness: videoSettings.brightness
                contrast: videoSettings.contrast
                saturation: videoSettings.saturation
                hue: videoSettings.hue
                gamma: videoSettings.gamma
                presentationScaler: videoSettings.presentationScaler
                compareActive: true
                compareRawOnLeft: true
                rawOnly: true
            }
        }

        // Drive the VideoSurface refresh from QML.
        Timer {
            running: playback.hasMedia && playback.playing
            repeat: true
            interval: playback.mediaInfo.fps > 0
                      ? Math.max(8, Math.round(1000 / Math.min(playback.mediaInfo.fps, 60)))
                      : 16
            property int lastFrameCounter: -1
            onTriggered: {
                videoSurface.refresh()
                if (compare.active && videoSurface.frameCounter !== lastFrameCounter) {
                    lastFrameCounter = videoSurface.frameCounter
                    compareRawSurface.requestUpdate()
                }
            }
        }

        // Compare divider drag handle (visible only in compare mode).
        MouseArea {
            anchors.fill: parent
            enabled: compare.active
            z: 10
            cursorShape: compare.active ? Qt.SplitHCursor : Qt.ArrowCursor
            onPressed: function(mouse) {
                // Clicking anywhere moves the divider there.
                compare.splitPosition = mouse.x / parent.width
            }
            onPositionChanged: function(mouse) {
                if (pressed)
                    compare.splitPosition = Math.max(0, Math.min(1, mouse.x / parent.width))
            }
        }

        Rectangle {
            visible: compare.active
            x: parent.width * compare.splitPosition - width * 0.5
            y: 0
            width: 2
            height: parent.height
            color: "#f1f6f5"
            opacity: 0.9
            z: 11
        }

        Rectangle {
            visible: compare.active
            x: parent.width * compare.splitPosition - width * 0.5
            y: parent.height * 0.5 - height * 0.5
            width: 28
            height: 28
            radius: 14
            color: "#151d25"
            border.color: "#f1f6f5"
            border.width: 2
            z: 12
        }

        // Actionable empty state.
        Column {
            anchors.centerIn: parent
            visible: !playback.hasMedia
            z: 8
            spacing: 14
            width: Math.min(420, Math.max(280, surfaceArea.width - 48))
            Item {
                width: parent.width; height: 68
                Rectangle {
                    anchors.centerIn: parent
                    width: 66; height: 66; radius: 18
                    color: theme.surface
                    border.color: theme.border
                    VectorIcon {
                        anchors.centerIn: parent
                        width: 30; height: 30
                        name: "folder-open"; color: theme.accent
                    }
                }
            }
            Text {
                width: parent.width
                text: "ANVIL PLAYER"
                font.pixelSize: 22
                font.weight: Font.DemiBold
                horizontalAlignment: Text.AlignHCenter
                color: theme.foreground
            }
            Text {
                width: parent.width
                text: "Open a video to start playback. The active processing backend is shown in the controls."
                color: theme.secondary
                font.pixelSize: 13
                wrapMode: Text.WordWrap
                horizontalAlignment: Text.AlignHCenter
            }
            Item {
                width: parent.width; height: 46
                IconButton {
                    anchors.centerIn: parent
                    width: 150; height: 42
                    iconName: "folder-open"
                    textLabel: "Open video"
                    tooltip: "Open videos (O)"
                    primary: true
                    onClicked: openDialog.open()
                }
            }
        }

        // Whole-window mouse tracker: drives auto-hide. This MouseArea sits
        // above the video but below the controls; it never blocks clicks
        // (it only watches motion), so playback toggles and divider drags
        // keep working.
        MouseArea {
            id: activityTracker
            anchors.fill: parent
            enabled: true
            hoverEnabled: true
            propagateComposedEvents: true
            acceptedButtons: Qt.NoButton  // observe only; never swallow clicks
            z: 5
            onPositionChanged: function(mouse) {
                chromeVisible = true
                hideTimer.restart()
                mouse.accepted = false
            }
            onExited: hideTimer.restart()
        }

        // Double-click toggles fullscreen (common player UX). Single click
        // toggles play/pause AND pokes the chrome. Right-click opens the
        // standardized context menu (aspect / zoom / screenshot / etc.).
        MouseArea {
            anchors.fill: parent
            acceptedButtons: Qt.LeftButton | Qt.RightButton
            z: 6
            onDoubleClicked: window.visibility === Window.FullScreen
                             ? window.showNormal() : window.showFullScreen()
            onClicked: function(mouse) {
                if (mouse.button === Qt.RightButton) {
                    contextMenu.popup()
                    return
                }
                playback.togglePlay()
                chromeVisible = true
                hideTimer.restart()
            }
        }
    }

    // Right-click context menu. All entries map to real actions; see
    // VideoContextMenu.qml.
    VideoContextMenu {
        id: contextMenu
        videoSurface: videoSurface
        playlistPopup: playlistPopup
    }

    // --- auto-hide timer ---
    // Resets whenever the mouse moves. When it fires, the chrome drops unless
    // a control region is being actively hovered or a menu/popup is open.
    Timer {
        id: hideTimer
        interval: 2500
        repeat: false
        running: false
        onTriggered: {
            if (!controlsHovered && !overlaysHovered &&
                !openDialog.visible && !settingsDialog.visible &&
                !addDialog.visible && !playlistPopup.visible &&
                !experimentalWarning.visible)
                chromeVisible = false
        }
    }

    // --- control overlay (bottom of the window, floating above the video) ---
    // Two-row layout: draggable seek bar on top, buttons below. The whole
    // overlay fades out smoothly when chromeVisible is false.
    Item {
        id: controlOverlay
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        height: 106
        z: 20
        opacity: chromeVisible ? 1.0 : 0.0
        visible: opacity > 0.01
        Behavior on opacity { NumberAnimation { duration: 180; easing.type: Easing.OutCubic } }

        // Dim the video behind the controls so text stays legible.
        Rectangle {
            id: controlBackground
            anchors.fill: parent
            // Subtle gradient from transparent (top) to dark (bottom) — VLC style.
            gradient: Gradient {
                orientation: Gradient.Vertical
                GradientStop { position: 0.0; color: "#00000000" }
                GradientStop { position: 0.55; color: "#b00b1015" }
                GradientStop { position: 1.0; color: "#f70b1015" }
            }
        }

        // Hover trap: while the mouse is anywhere over the overlay, controls
        // stay visible and the auto-hide timer is held off.
        MouseArea {
            id: controlHoverTrap
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton  // observe only; controls receive clicks
            z: 0
            onEntered: {
                controlsHovered = true
                chromeVisible = true
                hideTimer.stop()
            }
            onExited: {
                controlsHovered = false
                hideTimer.restart()
            }
            onPositionChanged: {
                chromeVisible = true
                hideTimer.restart()
            }
        }

        ColumnLayout {
            anchors.fill: parent
            anchors.leftMargin: 18
            anchors.rightMargin: 18
            anchors.topMargin: 8
            anchors.bottomMargin: 12
            spacing: 9
            z: 1

            // --- top row: seek bar ---
            RowLayout {
                Layout.fillWidth: true
                spacing: 8

                Label {
                    text: seekBar.pressed ? formatTime(seekBar.scrubTarget) : formatTime(playback.positionUs)
                    color: "#f1f6f5"
                    font.pixelSize: 12
                    Layout.preferredWidth: 56
                    horizontalAlignment: Qt.AlignHCenter
                    font.family: "monospace"
                }

                // Draggable, hover-aware seek slider. Click-to-seek anywhere
                // on the trough and drag the handle to scrub.
                Slider {
                    id: seekBar
                    property real scrubTarget: 0
                    Layout.fillWidth: true
                    from: 0
                    to: Math.max(1, playback.durationUs)
                    value: 0
                    enabled: playback.hasMedia

                    onPressedChanged: {
                        if (pressed) scrubTarget = value
                        else playback.seekUs(Math.round(scrubTarget))
                    }
                    onMoved: scrubTarget = value
                    // Preview the scrub position; request the seek only on release.
                    Binding {
                        target: seekBar
                        property: "value"
                        value: playback.positionUs
                        when: !seekBar.pressed
                        restoreMode: Binding.RestoreNone
                    }

                    background: Rectangle {
                        x: seekBar.leftPadding
                        y: seekBar.topPadding + seekBar.availableHeight / 2 - height / 2
                        implicitWidth: 200
                        implicitHeight: 4
                        width: seekBar.availableWidth
                        height: implicitHeight
                        radius: 2
                        color: "#293640"

                        Rectangle {
                            width: seekBar.visualPosition * parent.width
                            height: parent.height
                            color: "#41d995"
                            radius: 2
                        }
                    }

                    handle: Rectangle {
                        x: seekBar.leftPadding + seekBar.visualPosition
                           * (seekBar.availableWidth - width)
                        y: seekBar.topPadding + seekBar.availableHeight / 2 - height / 2
                        implicitWidth: 14
                        implicitHeight: 14
                        radius: 7
                        color: seekBar.pressed ? "#ffffff" : "#f1f6f5"
                        border.color: "#41d995"
                        border.width: 2
                    }
                }

                Label {
                    text: formatTime(playback.durationUs)
                    color: "#b5c5cd"
                    font.pixelSize: 12
                    Layout.preferredWidth: 56
                    horizontalAlignment: Qt.AlignHCenter
                    font.family: "monospace"
                }
            }

            // --- bottom row: buttons ---
            RowLayout {
                Layout.fillWidth: true
                spacing: 5

                // Open file.
                IconButton {
                    iconName: "folder-open"
                    tooltip: "Open videos (O)"
                    onClicked: openDialog.open()
                    Layout.preferredWidth: 36
                }

                // Previous playlist item, or restart the current item when
                // playback is more than three seconds in.
                IconButton {
                    iconName: "skip-back"
                    tooltip: "Previous item (or restart)"
                    enabled: playback.hasMedia
                    onClicked: playback.previous()
                    Layout.preferredWidth: 36
                }

                // Skip back 10s.
                IconButton {
                    iconName: "rewind"
                    tooltip: "Back 10s (Left)"
                    enabled: playback.hasMedia
                    onClicked: seekBy(-10000000)
                    Layout.preferredWidth: 36
                }

                // Approximate source-frame step backward (pause first).
                IconButton {
                    iconName: "step-back"
                    tooltip: "Previous frame (approximate, comma)"
                    enabled: playback.hasMedia
                    onClicked: seekFrame(-1)
                    Layout.preferredWidth: 32
                    visible: window.width >= 1040
                }

                // Play/pause (wide, primary).
                IconButton {
                    iconName: playback.playing ? "pause" : "play"
                    tooltip: playback.playing ? "Pause (Space)" : "Play (Space)"
                    enabled: playback.hasMedia
                    primary: true
                    onClicked: playback.togglePlay()
                    Layout.preferredWidth: 52
                }

                // Approximate source-frame step forward (pause first).
                IconButton {
                    iconName: "step-forward"
                    tooltip: "Next frame (approximate, period)"
                    enabled: playback.hasMedia
                    onClicked: seekFrame(1)
                    Layout.preferredWidth: 32
                    visible: window.width >= 1040
                }

                // Skip forward 10s.
                IconButton {
                    iconName: "fast-forward"
                    tooltip: "Forward 10s (Right)"
                    enabled: playback.hasMedia
                    onClicked: seekBy(10000000)
                    Layout.preferredWidth: 36
                }

                // Advance to the next playlist item without waiting for EOF.
                IconButton {
                    iconName: "skip-forward"
                    tooltip: "Next item"
                    enabled: playback.hasNext
                    onClicked: playback.next()
                    Layout.preferredWidth: 36
                }

                // Volume mute toggle.
                IconButton {
                    iconName: playback.muted || playback.volume === 0 ? "volume-x"
                          : playback.volume < 50 ? "volume-low" : "volume-high"
                    tooltip: playback.muted ? "Unmute (M)" : "Mute (M)"
                    onClicked: playback.muted = !playback.muted
                    Layout.preferredWidth: 32
                }
                Slider {
                    id: volumeBar
                    from: 0; to: 100
                    value: playback.volume
                    onMoved: playback.volume = value
                    Layout.preferredWidth: 94
                    visible: window.width >= 920
                    enabled: true
                    background: Rectangle {
                        implicitHeight: 4
                        height: 4
                        radius: 2
                        color: "#293640"
                        Rectangle {
                            width: volumeBar.visualPosition * parent.width
                            height: parent.height
                            color: "#f1f6f5"
                            radius: 2
                        }
                    }
                }

                IconButton {
                    iconName: "playlist"
                    textLabel: String(playback.playlistCount)
                    tooltip: "Playlist (P)"
                    onClicked: playlistPopup.open()
                    Layout.preferredWidth: 70
                }

                Item { Layout.fillWidth: true }

                // Legacy FSR preset selector, not an ANVIL roster selector.
                ComboBox {
                    id: presetCombo
                    model: fsr.presetOptions
                    textRole: "label"
                    valueRole: "key"
                    currentIndex: {
                        const key = fsr.preset
                        for (let i = 0; i < fsr.presetOptions.length; ++i)
                            if (fsr.presetOptions[i].key === key) return i
                        return 0
                    }
                    onActivated: function(index) {
                        fsr.preset = currentValue
                    }
                    Layout.preferredWidth: Math.max(150, Math.min(205, window.width * 0.18))
                    visible: window.width >= 1120
                }

                // Compare toggle.
                IconButton {
                    iconName: "compare"
                    tooltip: "A/B compare (C)"
                    checked: compare.active
                    enabled: playback.hasMedia
                    onClicked: compare.active = !compare.active
                    Layout.preferredWidth: 38
                }

                // Fullscreen toggle.
                IconButton {
                    iconName: window.visibility === Window.FullScreen ? "minimize" : "maximize"
                    tooltip: window.visibility === Window.FullScreen
                             ? "Exit fullscreen (F/Esc)" : "Fullscreen (F)"
                    checked: window.visibility === Window.FullScreen
                    onClicked: window.visibility === Window.FullScreen
                               ? window.showNormal() : window.showFullScreen()
                    Layout.preferredWidth: 36
                }

                // Settings gear button.
                IconButton {
                    iconName: "settings"
                    tooltip: "Settings (Ctrl+,)"
                    onClicked: settingsDialog.show()
                    Layout.preferredWidth: 36
                }
            }
        }
    }

    // --- keyboard shortcuts (spec 05 + new transport keys) ---
    Shortcut { sequence: "Space"; onActivated: playback.togglePlay() }
    Shortcut { sequence: "O";     onActivated: openDialog.open() }
    Shortcut { sequence: "P";     onActivated: playlistPopup.open() }
    Shortcut {
        sequence: "F"
        onActivated: window.visibility === Window.FullScreen
                     ? window.showNormal() : window.showFullScreen()
    }
    Shortcut { sequence: "Escape"; onActivated: if (window.visibility === Window.FullScreen) window.showNormal() }
    Shortcut { sequence: "M"; onActivated: playback.muted = !playback.muted }
    Shortcut { sequence: "Ctrl+F"; onActivated: debug.visible = !debug.visible }
    Shortcut { sequence: "Ctrl+I"; onActivated: infoPanel.visible = !infoPanel.visible }
    Shortcut { sequence: "Ctrl+,"; onActivated: settingsDialog.show() }
    // Screenshot + clipboard capture (real, via ScreenCaptureController).
    Shortcut { sequence: "Ctrl+S"; onActivated: capture.captureToFile(playback, playback.mediaTitle) }
    Shortcut { sequence: "Ctrl+Shift+C"; onActivated: capture.captureToClipboard(playback) }
    Shortcut { sequence: "C"; onActivated: compare.active = !compare.active }
    Shortcut { sequence: "H"; onActivated: liveHud.hudVisible = !liveHud.hudVisible }
    // Seek by 10s.
    Shortcut { sequence: "Left";  onActivated: seekBy(-10000000) }
    Shortcut { sequence: "Right"; onActivated: seekBy(10000000) }
    // Frame step (~1 frame @ 25fps). Always pauses first, like VLC.
    Shortcut { sequence: ","; onActivated: seekFrame(-1) }
    Shortcut { sequence: "."; onActivated: seekFrame(1) }
    // Volume via arrow keys.
    Shortcut { sequence: "Up";   onActivated: playback.volume = Math.min(100, playback.volume + 5) }
    Shortcut { sequence: "Down"; onActivated: playback.volume = Math.max(0, playback.volume - 5) }
    // spec 05 FSR preset shortcuts
    Shortcut { sequence: "Ctrl+1"; onActivated: fsr.preset = 1 } // Off
    Shortcut { sequence: "Ctrl+2"; onActivated: fsr.preset = 3 } // Quality
    Shortcut { sequence: "Ctrl+3"; onActivated: fsr.preset = 4 } // Balanced
    Shortcut { sequence: "Ctrl+4"; onActivated: fsr.preset = 5 } // Performance
    Shortcut { sequence: "Ctrl+5"; onActivated: fsr.preset = 6 } // Ultra Performance

    // --- debug overlay (spec 05) ---
    Rectangle {
        id: debugOverlay
        visible: debug.visible
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 12
        width: debugCol.implicitWidth + 24
        height: debugCol.implicitHeight + 24
        color: "#000000"
        opacity: 0.82
        radius: 6
        z: 100

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            onEntered: { overlaysHovered = true; chromeVisible = true; hideTimer.stop() }
            onExited:   { overlaysHovered = false; hideTimer.restart() }
        }

        Column {
            id: debugCol
            anchors.margins: 12
            anchors.fill: parent
            spacing: 2
            Label { text: "DEBUG (Ctrl+F)"; color: "#41d995"; font.pixelSize: 11; font.bold: true }
            Label { text: "Source: " + debug.stats.sourceResolution; color: "#b5c5cd"; font.pixelSize: 11; font.family: "monospace" }
            Label { text: "FSR target: " + debug.stats.fsrTargetResolution; color: "#b5c5cd"; font.pixelSize: 11; font.family: "monospace" }
            Label { text: "Window: " + debug.stats.windowResolution; color: "#b5c5cd"; font.pixelSize: 11; font.family: "monospace" }
            Label { text: "Backend: " + debug.stats.backend; color: "#b5c5cd"; font.pixelSize: 11; font.family: "monospace" }
            Label { text: "Preset: " + debug.stats.preset; color: "#b5c5cd"; font.pixelSize: 11; font.family: "monospace" }
            Label { text: "Video FPS: " + debug.stats.videoFps + " (preserved)"; color: "#b5c5cd"; font.pixelSize: 11; font.family: "monospace" }
            Label { text: "Frame PTS: " + debug.stats.framePts; color: "#b5c5cd"; font.pixelSize: 11; font.family: "monospace" }
            Label { text: "History resets: " + debug.stats.historyResets; color: "#b5c5cd"; font.pixelSize: 11; font.family: "monospace" }
            Label { text: "Scene cuts: " + debug.stats.sceneCutsDetected; color: "#b5c5cd"; font.pixelSize: 11; font.family: "monospace" }
            Label { text: "Motion confidence: " + debug.stats.motionConfidence.toFixed(3); color: "#b5c5cd"; font.pixelSize: 11; font.family: "monospace" }
            Label { text: "Reactive avg: " + debug.stats.reactiveAverage.toFixed(3); color: "#b5c5cd"; font.pixelSize: 11; font.family: "monospace" }
        }
    }

    // --- media / FSR info panel (spec 05 Media Info + FSR Info panels) ---
    Rectangle {
        id: infoPanel
        visible: false
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.margins: 12
        width: infoCol.implicitWidth + 24
        height: infoCol.implicitHeight + 24
        color: "#000000"
        opacity: 0.85
        radius: 6
        z: 100

        MouseArea {
            anchors.fill: parent
            hoverEnabled: true
            acceptedButtons: Qt.NoButton
            onEntered: { overlaysHovered = true; chromeVisible = true; hideTimer.stop() }
            onExited:   { overlaysHovered = false; hideTimer.restart() }
        }

        Column {
            id: infoCol
            anchors.margins: 12
            anchors.fill: parent
            spacing: 4
            Label { text: "MEDIA INFO (Ctrl+I)"; color: "#41d995"; font.pixelSize: 11; font.bold: true }
            Label { text: "File: " + playback.mediaTitle; color: "#b5c5cd"; font.pixelSize: 11 }
            Label { text: "Container: " + playback.mediaInfo.container; color: "#9a9aa5"; font.pixelSize: 10 }
            Label { text: "Video: " + playback.mediaInfo.videoCodec + " " + playback.mediaInfo.width + "x" + playback.mediaInfo.height; color: "#9a9aa5"; font.pixelSize: 10 }
            Label { text: "FPS: " + playback.mediaInfo.fps + " (preserved)"; color: "#9a9aa5"; font.pixelSize: 10 }
            Label { text: "Audio: " + playback.mediaInfo.audioCodec + " " + playback.mediaInfo.sampleRate + "Hz"; color: "#9a9aa5"; font.pixelSize: 10 }
            Label { text: " "; color: "#9a9aa5"; font.pixelSize: 6 }
            Label { text: "FSR CHAIN"; color: "#41d995"; font.pixelSize: 11; font.bold: true }
            Label { text: fsr.chainLabel; color: "#b5c5cd"; font.pixelSize: 10; wrapMode: Text.Wrap; width: parent.width - 24 }
            Label { text: "Backend: " + fsr.backendLabel; color: "#9a9aa5"; font.pixelSize: 10 }
        }
    }

    // --- Live enhancement HUD (toggle with H) ---
    // Positioning (bottom-right, above timeline) is defined in LiveHud.qml.
    // The HUD respects the global chromeVisible flag so it hides with the
    // rest of the chrome when the mouse is idle, but its toggle (H) still
    // controls whether it reappears.
    LiveHud {
        id: liveHud
        visible: playback.hasMedia && hudVisible && chromeVisible
    }

    // --- Settings dialog (Ctrl+, or gear button) ---
    SettingsDialog {
        id: settingsDialog
        visible: false
        function show() {
            visible = true
            raise()
            requestActivate()
        }
    }

    // --- helpers ---
    // Approximate source-frame stepping; seekUs is not an exact-frame stepping API.
    function seekFrame(direction) {
        if (!playback.hasMedia) return
        if (playback.playing) playback.togglePlay()
        const fps = Number(playback.mediaInfo.fps)
        seekBy(direction * Math.round(1000000 / (fps > 0 ? fps : 25)))
    }

    function seekBy(deltaUs) {
        if (!playback.hasMedia) return
        const target = Math.max(0,
            Math.min(playback.durationUs, playback.positionUs + deltaUs))
        playback.seekUs(target)
    }

    function formatTime(us) {
        if (us <= 0) return "0:00"
        const totalSec = Math.floor(us / 1000000)
        const h = Math.floor(totalSec / 3600)
        const m = Math.floor((totalSec % 3600) / 60)
        const s = totalSec % 60
        if (h > 0) return h + ":" + String(m).padStart(2, '0') + ":" + String(s).padStart(2, '0')
        return m + ":" + String(s).padStart(2, '0')
    }
}
