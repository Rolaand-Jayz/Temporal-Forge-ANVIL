// SettingsDialog.qml — feature-rich settings dialog.
//
// Tabs:
//   • Filters     — post-processing chain (film grain, sharpen, bloom, deband,
//                   letterbox). Real GPU effects via the display shader + the
//                   VideoSurfaceItem geometry path.
//   • Color       — brightness / contrast / saturation / hue / gamma, scaler.
//   • Upscaler    — FSR4 backend + preset, live status.
//   • Audio       — volume, mute.
//   • Interface   — window + UI behavior (most options reflect live values
//                   pulled from the main window).
//   • Hotkeys     — read-only reference table of every keyboard shortcut.
//
// All controls bind to real C++ controllers (videoSettings, fsr, postProcess,
// playback, compare). No fake entries.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import QtQuick.Window
import TemporalForge

ApplicationWindow {
    id: settingsWindow
    title: "Settings"
    width: 900; height: 680
    minimumWidth: 760; minimumHeight: 560
    color: "#0b1015"
    modality: Qt.ApplicationModal

    property int currentTab: 0
    PlayerTheme { id: theme }

    // Reusable styling tokens — keep the whole dialog visually consistent.
    readonly property color cBg: theme.background
    readonly property color cSurface: theme.surface
    readonly property color cSurface2: theme.surfaceRaised
    readonly property color cAccent: theme.accent
    readonly property color cAccentDim: theme.accentTint
    readonly property color cText: theme.foreground
    readonly property color cTextDim: theme.secondary
    readonly property color cTextMuted: theme.muted
    readonly property color cWarn: theme.warning

    // Tab definitions.
    ListModel {
        id: tabModel
        ListElement { name: "Filters";     iconName: "sliders" }
        ListElement { name: "Color";       iconName: "color" }
        ListElement { name: "Upscaler";    iconName: "cpu" }
        ListElement { name: "Audio";       iconName: "headphones" }
        ListElement { name: "Interface";   iconName: "monitor" }
        ListElement { name: "Hotkeys";     iconName: "keyboard" }
    }

    RowLayout {
        anchors.fill: parent
        anchors.margins: 0
        spacing: 0

        // --- Sidebar ---
        Rectangle {
            Layout.fillHeight: true
            Layout.preferredWidth: 180
            color: cBg

            Column {
                anchors.fill: parent
                anchors.margins: 8
                spacing: 2

                // Header.
                Item {
                    width: parent.width
                    height: 56
                    VectorIcon {
                        anchors.left: parent.left; anchors.leftMargin: 12
                        anchors.verticalCenter: parent.verticalCenter
                        width: 22; height: 22
                        name: "settings"; color: cAccent
                    }
                    Text {
                        anchors.left: parent.left; anchors.leftMargin: 44
                        anchors.verticalCenter: parent.verticalCenter
                        text: "Settings"
                        color: cText
                        font.pixelSize: 15
                        font.bold: true
                    }
                }

                Repeater {
                    model: tabModel
                    delegate: Rectangle {
                        width: parent.width
                        height: 40
                        radius: 6
                        color: settingsWindow.currentTab === index ? cSurface2 : "transparent"
                        Behavior on color { ColorAnimation { duration: 100 } }

                        VectorIcon {
                            anchors.left: parent.left
                            anchors.leftMargin: 14
                            anchors.verticalCenter: parent.verticalCenter
                            width: 19; height: 19
                            name: model.iconName
                            color: settingsWindow.currentTab === index ? cAccent : cTextDim
                        }
                        Text {
                            anchors.left: parent.left
                            anchors.leftMargin: 44
                            anchors.verticalCenter: parent.verticalCenter
                            text: model.name
                            color: settingsWindow.currentTab === index ? cText : cTextDim
                            font.pixelSize: 13
                            font.bold: settingsWindow.currentTab === index
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: settingsWindow.currentTab = index
                        }
                    }
                }

                Item { width: 1; height: 12 }

                Button {
                    width: parent.width
                    flat: true
                    text: "Reset tab to defaults"
                    enabled: settingsWindow.currentTab === 0 || settingsWindow.currentTab === 1 || settingsWindow.currentTab === 3
                    onClicked: {
                        switch (settingsWindow.currentTab) {
                            case 0: postProcess.resetAll(); break
                            case 1: videoSettings.resetColor(); break
                            case 3: playback.volume = 100; playback.muted = false; break
                        }
                    }
                    contentItem: Text {
                        text: parent.text
                        color: cTextMuted
                        font.pixelSize: 11
                        horizontalAlignment: Qt.AlignHCenter
                    }
                    background: Rectangle { color: "transparent" }
                }
            }
        }

        // Thin divider.
        Rectangle { Layout.fillHeight: true; Layout.preferredWidth: 1; color: cSurface2 }

        // --- Content area ---
        Rectangle {
            Layout.fillHeight: true
            Layout.fillWidth: true
            color: cBg

            // Header bar showing the current tab name.
            Rectangle {
                id: headerBar
                anchors.left: parent.left; anchors.right: parent.right
                anchors.top: parent.top
                height: 48
                color: "transparent"
                border.color: cSurface2
                border.width: 0

                VectorIcon {
                    anchors.left: parent.left; anchors.leftMargin: 24
                    anchors.verticalCenter: parent.verticalCenter
                    width: 20; height: 20
                    name: tabModel.get(settingsWindow.currentTab).iconName
                    color: cAccent
                }
                Text {
                    anchors.left: parent.left; anchors.leftMargin: 54
                    anchors.verticalCenter: parent.verticalCenter
                    text: tabModel.get(settingsWindow.currentTab).name
                    color: cText
                    font.pixelSize: 18
                    font.bold: true
                }

                // Close button top-right.
                Item {
                    anchors.right: parent.right; anchors.rightMargin: 8
                    anchors.verticalCenter: parent.verticalCenter
                    width: 32; height: 32
                    Rectangle {
                        anchors.fill: parent
                        radius: 6
                        color: closeMa.containsMouse ? cSurface2 : "transparent"
                    }
                    VectorIcon {
                        anchors.centerIn: parent
                        width: 18; height: 18
                        name: "close"
                        color: closeMa.containsMouse ? cText : cTextMuted
                    }
                    MouseArea {
                        id: closeMa
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: settingsWindow.close()
                    }
                }

                Rectangle {
                    anchors.left: parent.left; anchors.right: parent.right
                    anchors.bottom: parent.bottom
                    height: 1
                    color: cSurface2
                }
            }

            // Each SettingsPane owns its scroll area. A second scroll view here
            // prevents sliders and switches from receiving pointer gestures.
            Item {
                anchors.left: parent.left; anchors.right: parent.right
                anchors.top: headerBar.bottom; anchors.bottom: parent.bottom
                StackLayout {
                    id: tabStack
                    anchors.fill: parent
                    currentIndex: settingsWindow.currentTab

                    // ============ Filters ============
                    SettingsPane {
                        title: "Post-Processing Filters"
                        subtitle: "Real-time GPU filters applied to the displayed frame."

                        SettingsSection {
                            title: "Film Grain"
                            description: "Subtle additive noise. Looks great on dark scenes and masks gradient banding."
                            LabeledSlider {
                                label: "Strength"
                                value: postProcess.grainStrength
                                from: 0.0; to: 1.0; stepSize: 0.05
                                decimals: 2
                                onMoved: postProcess.grainStrength = newValue
                            }
                        }

                        SettingsSection {
                            title: "Display Sharpen"
                            description: "Final-pass unsharp mask. Distinct from the FSR upscaler's own sharpness."
                            LabeledSlider {
                                label: "Amount"
                                value: postProcess.displaySharpen
                                from: 0.0; to: 2.0; stepSize: 0.05
                                decimals: 2
                                onMoved: postProcess.displaySharpen = newValue
                            }
                        }

                        SettingsSection {
                            title: "Bloom / Glow"
                            description: "Bright pixels bleed into neighbors. Cinematic look; costs a few ms on big frames."
                            LabeledSlider {
                                label: "Strength"
                                value: postProcess.bloomStrength
                                from: 0.0; to: 1.0; stepSize: 0.05
                                decimals: 2
                                onMoved: postProcess.bloomStrength = newValue
                            }
                            LabeledSlider {
                                label: "Threshold (luma)"
                                value: postProcess.bloomThreshold
                                from: 0.3; to: 1.0; stepSize: 0.01
                                decimals: 2
                                onMoved: postProcess.bloomThreshold = newValue
                            }
                        }

                        SettingsSection {
                            title: "Deband"
                            description: "Ordered Bayer dither. Breaks up visible banding in dark gradients (8-bit sources)."
                            LabeledToggle {
                                text: "Enable deband"
                                checked: postProcess.debandEnabled
                                onToggled: postProcess.debandEnabled = newChecked
                            }
                        }

                        SettingsSection {
                            title: "Letterbox Detection & Crop"
                            description: "Auto-crop hard black bars. Manual lets you specify a top/bottom crop fraction."
                            // PostProcessController.LetterboxOff=0, LetterboxAuto=1,
                            // LetterboxManual=2. Use raw ints because
                            // PostProcessController is exposed as a context
                            // property instance (not a type).
                            LabeledRadio {
                                text: "Off — show full frame"
                                checked: postProcess.letterboxMode === 0
                                onToggled: postProcess.letterboxMode = 0
                            }
                            LabeledRadio {
                                text: "Auto — detect bars per frame"
                                checked: postProcess.letterboxMode === 1
                                onToggled: postProcess.letterboxMode = 1
                            }
                            LabeledRadio {
                                text: "Manual — fixed crop"
                                checked: postProcess.letterboxMode === 2
                                onToggled: postProcess.letterboxMode = 2
                            }
                            LabeledSlider {
                                label: "Manual crop"
                                value: postProcess.letterboxManualCrop
                                from: 0.0; to: 0.25; stepSize: 0.01
                                decimals: 2
                                enabled: postProcess.letterboxMode === 2
                                onMoved: postProcess.letterboxManualCrop = newValue
                            }
                        }
                    }

                    // ============ Color ============
                    SettingsPane {
                        title: "Color Adjustments"
                        subtitle: "Brightness / contrast / saturation / hue / gamma applied at display time."

                        SettingsSection {
                            title: "Tone"
                            LabeledSlider {
                                label: "Brightness"
                                value: videoSettings.brightness
                                from: -1.0; to: 1.0; stepSize: 0.02
                                decimals: 2
                                onMoved: videoSettings.brightness = newValue
                            }
                            LabeledSlider {
                                label: "Contrast"
                                value: videoSettings.contrast
                                from: -1.0; to: 1.0; stepSize: 0.02
                                decimals: 2
                                onMoved: videoSettings.contrast = newValue
                            }
                            LabeledSlider {
                                label: "Gamma"
                                value: videoSettings.gamma
                                from: 0.1; to: 3.0; stepSize: 0.05
                                decimals: 2
                                onMoved: videoSettings.gamma = newValue
                            }
                        }

                        SettingsSection {
                            title: "Color"
                            LabeledSlider {
                                label: "Saturation"
                                value: videoSettings.saturation
                                from: -1.0; to: 1.0; stepSize: 0.02
                                decimals: 2
                                onMoved: videoSettings.saturation = newValue
                            }
                            LabeledSlider {
                                label: "Hue"
                                value: videoSettings.hue
                                from: -1.0; to: 1.0; stepSize: 0.02
                                decimals: 2
                                onMoved: videoSettings.hue = newValue
                            }
                        }

                        SettingsSection {
                            title: "Display Scaler"
                            description: "How the (already upscaled) texture is sampled to the window."
                            ComboBox {
                                model: ["Auto (Bicubic)", "Bilinear", "Bicubic", "Lanczos", "EASU-style"]
                                currentIndex: videoSettings.presentationScaler
                                onActivated: videoSettings.presentationScaler = currentIndex
                                width: 240
                            }
                        }
                    }

                    // ============ Upscaler ============
                    SettingsPane {
                        title: "Playback Backend"
                        subtitle: "Legacy FSR controls for the existing player. Only experimental FSR 4.1 is connected to the live Vulkan dispatch; other selections disable it. The causal ANVIL live stage is connected as an experimental opt-in pre-FSR reconstruction (launch with ANVIL_LIVE_PRE_FSR=1); the full ANVIL roster is not connected."

                        SettingsSection {
                            title: "Backend"
                            // Experimental backend warning.
                            Rectangle {
                                visible: fsr.backend === 3 // Fsr4ReExperimental
                                width: parent.width; height: 56
                                color: "#2a1a0a"; radius: 6
                                border.color: settingsWindow.cWarn; border.width: 1
                                Column {
                                    anchors.centerIn: parent
                                    spacing: 2
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "FSR 4.1 INT8 — Experimental"
                                        color: settingsWindow.cWarn
                                        font.pixelSize: 12; font.bold: true
                                    }
                                    Text {
                                        anchors.horizontalCenter: parent.horizontalCenter
                                        text: "RE-derived. Local validation only — no official video-FSR reference."
                                        color: settingsWindow.cWarn
                                        font.pixelSize: 10
                                    }
                                }
                            }

                            ComboBox {
                                model: fsr.backendOptions
                                textRole: "label"
                                valueRole: "key"
                                currentIndex: {
                                    for (let i = 0; i < fsr.backendOptions.length; ++i)
                                        if (fsr.backendOptions[i].key === fsr.backend) return i
                                    return 0
                                }
                                onActivated: fsr.backend = currentValue
                                width: 460
                            }
                        }

                        SettingsSection {
                            title: "Preset"
                            GridLayout {
                                columns: 3
                                columnSpacing: 8
                                rowSpacing: 8
                                width: parent.width

                                Repeater {
                                    model: fsr.presetOptions
                                    delegate: Rectangle {
                                        Layout.fillWidth: true
                                        height: 64
                                        radius: 6
                                        color: fsr.preset === modelData.key ? settingsWindow.cAccentDim : settingsWindow.cSurface
                                        border.color: fsr.preset === modelData.key ? settingsWindow.cAccent : settingsWindow.cSurface2
                                        border.width: 1

                                        Column {
                                            anchors.centerIn: parent
                                            spacing: 3
                                            Text {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                text: modelData.label
                                                color: fsr.preset === modelData.key ? settingsWindow.cAccent : settingsWindow.cTextDim
                                                font.pixelSize: 11; font.bold: fsr.preset === modelData.key
                                            }
                                            Text {
                                                anchors.horizontalCenter: parent.horizontalCenter
                                                text: modelData.targetW ? (modelData.targetW + "×" + modelData.targetH)
                                                                         : (modelData.ratio.toFixed(2) + "×")
                                                color: settingsWindow.cTextMuted
                                                font.pixelSize: 9
                                            }
                                        }

                                        MouseArea {
                                            anchors.fill: parent
                                            cursorShape: Qt.PointingHandCursor
                                            onClicked: fsr.preset = modelData.key
                                        }
                                    }
                                }
                            }
                        }

                        SettingsSection {
                            title: "Input Stability"
                            description: "These values are applied before the FSR graph. Keep input sharpening low to avoid ringing and screen-door texture on 240p-480p sources."
                            LabeledSlider {
                                label: "FSR input sharpen"
                                value: videoSettings.sharpness
                                from: 0.0; to: 1.0; stepSize: 0.05
                                decimals: 2
                                onMoved: videoSettings.sharpness = newValue
                            }
                            LabeledSlider {
                                label: "Jitter strength"
                                value: videoSettings.jitterStrength
                                from: 0.0; to: 1.5; stepSize: 0.05
                                decimals: 2
                                onMoved: videoSettings.jitterStrength = newValue
                            }
                        }

                        SettingsSection {
                            title: "Live Status"
                            Rectangle {
                                width: parent.width
                                height: 56
                                color: settingsWindow.cSurface
                                radius: 6
                                Text {
                                    anchors.left: parent.left; anchors.leftMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    text: {
                                        if (fsr.backend !== 3) return "FSR4: inactive (decoded playback)"
                                        if (playback.fsr4Active && playback.lastFsr4DispatchMs > 0)
                                            return "FSR4: active • dispatch " + playback.lastFsr4DispatchMs.toFixed(1) + " ms"
                                        if (playback.fsr4Active) return "FSR4: active • waiting for dispatch"
                                        if (playback.fsr4Enabled) return "FSR4: enabled • not yet dispatching"
                                        return "FSR4: selected • enabling on playback"
                                    }
                                    color: fsr.backend === 3 ? "#39d353" : settingsWindow.cTextMuted
                                    font.pixelSize: 12; font.family: "monospace"
                                }
                            }
                            Rectangle {
                                width: parent.width
                                height: 56
                                color: settingsWindow.cSurface
                                radius: 6
                                Text {
                                    anchors.left: parent.left; anchors.leftMargin: 12
                                    anchors.verticalCenter: parent.verticalCenter
                                    // Truthful opt-in live-stage state. The
                                    // stage is off by default; "standby" is
                                    // opted in but not reconstructing yet;
                                    // "bypassed" means a budget miss disabled
                                    // it for this playback (decoded pixels).
                                    text: {
                                        if (playback.anvilLiveStage === "reconstructing")
                                            return "ANVIL live: reconstructing (causal pre-FSR, experimental)"
                                        if (playback.anvilLiveStage === "standby")
                                            return "ANVIL live: opted in • waiting to reconstruct"
                                        if (playback.anvilLiveStage === "bypassed")
                                            return "ANVIL live: bypassed after budget miss (decoded pixels)"
                                        return "ANVIL live: off (opt in with ANVIL_LIVE_PRE_FSR=1)"
                                    }
                                    color: playback.anvilLiveStage === "reconstructing"
                                        ? "#39d353" : settingsWindow.cTextMuted
                                    font.pixelSize: 12; font.family: "monospace"
                                }
                            }
                        }
                    }

                    // ============ Audio ============
                    SettingsPane {
                        title: "Audio"
                        subtitle: "Output volume and mute state."

                        SettingsSection {
                            title: "Output"
                            LabeledSlider {
                                label: "Volume"
                                value: playback.volume
                                from: 0; to: 100; stepSize: 1
                                decimals: 0
                                suffix: "%"
                                onMoved: playback.volume = newValue
                            }
                            LabeledToggle {
                                text: "Muted"
                                checked: playback.muted
                                onToggled: playback.muted = newChecked
                            }
                        }
                        SettingsSection {
                            title: "Track"
                            description: "Audio track switching is not exposed by the current player. This panel controls the active output volume."
                        }
                    }

                    // ============ Interface ============
                    SettingsPane {
                        title: "Interface"
                        subtitle: "Window behavior and on-screen display."

                        SettingsSection {
                            title: "On-Screen Display"
                            LabeledToggle {
                                text: "Show enhancement HUD (H)"
                                checked: liveHud ? liveHud.hudVisible : false
                                onToggled: if (liveHud) liveHud.hudVisible = newChecked
                            }
                        }
                        SettingsSection {
                            title: "Window"
                            LabeledToggle {
                                text: "Fullscreen now"
                                checked: window.visibility === Window.FullScreen
                                onToggled: {
                                    // The flag is persisted by main.cpp on quit; here we
                                    // just toggle the current window as a preview.
                                    if (newChecked) window.showFullScreen()
                                    else          window.showNormal()
                                }
                            }
                            LabeledToggle {
                                text: "Always on top (this session)"
                                checked: (window.flags & Qt.WindowStaysOnTopHint) !== 0
                                onToggled: {
                                    // Real — uses the platform window flag.
                                    if (newChecked) window.flags = window.flags | Qt.WindowStaysOnTopHint
                                    else          window.flags = window.flags & ~Qt.WindowStaysOnTopHint
                                }
                            }
                        }
                    }

                    // ============ Hotkeys ============
                    SettingsPane {
                        title: "Keyboard Shortcuts"
                        subtitle: "Current implemented shortcuts. This panel does not change bindings."

                        // Read-only shortcut reference table, populated in two
                        // columns so it doesn't run too long.
                        GridLayout {
                            columns: 2
                            columnSpacing: 24
                            rowSpacing: 6
                            width: parent.width

                            Repeater {
                                model: [
                                    { key: "Space",      action: "Play / Pause" },
                                    { key: "O",          action: "Open file" },
                                    { key: "F",          action: "Toggle fullscreen" },
                                    { key: "Esc",        action: "Exit fullscreen" },
                                    { key: "M",          action: "Mute" },
                                    { key: "↑ / ↓",      action: "Volume up / down" },
                                    { key: "← / →",      action: "Seek −10s / +10s" },
                                    { key: "C",          action: "Toggle A/B compare" },
                                    { key: "H",          action: "Toggle enhancement HUD" },
                                    { key: "Ctrl+S",     action: "Save screenshot" },
                                    { key: "Ctrl+Shift+C", action: "Copy frame to clipboard" },
                                    { key: "Ctrl+F",     action: "Debug overlay" },
                                    { key: "Ctrl+I",     action: "Media info" },
                                    { key: "Ctrl+,",     action: "Settings" },
                                    { key: "Ctrl+1..5",  action: "FSR preset shortcuts" },
                                    { key: "Right-click",action: "Context menu" }
                                ]
                                delegate: RowLayout {
                                    Layout.fillWidth: true
                                    spacing: 8
                                    Rectangle {
                                        Layout.preferredWidth: 110
                                        Layout.preferredHeight: 24
                                        color: settingsWindow.cSurface2
                                        radius: 3
                                        Text {
                                            anchors.centerIn: parent
                                            text: modelData.key
                                            color: settingsWindow.cAccent
                                            font.pixelSize: 11; font.family: "monospace"; font.bold: true
                                        }
                                    }
                                    Text {
                                        Layout.fillWidth: true
                                        text: modelData.action
                                        color: settingsWindow.cTextDim
                                        font.pixelSize: 12
                                    }
                                }
                            }
                        }
                    }
                }
            }
        }
    }
}
