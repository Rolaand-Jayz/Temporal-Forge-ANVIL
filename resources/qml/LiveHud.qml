// LiveHud.qml — compact enhancement status indicator (bottom-right).
//
// Single-line pill showing backend + resolution chain + status dot.
// Toggle with 'H'. Defaults visible.
import QtQuick
import QtQuick.Controls

Rectangle {
    id: hud
    visible: playback.hasMedia && hudVisible
    property bool hudVisible: true

    anchors.bottom: parent.bottom
    anchors.bottomMargin: 120
    anchors.right: parent.right
    anchors.rightMargin: 12
    width: Math.min(hudText.implicitWidth + 34, Math.max(180, parent.width - 32))
    height: 28
    color: "#101b22"
    opacity: 0.92
    radius: 9
    z: 50

    Row {
        id: hudRow
        anchors.centerIn: parent
        spacing: 8

        // Status dot.
        Rectangle {
            width: 7; height: 7; radius: 4
            anchors.verticalCenter: parent.verticalCenter
            color: {
                if (!playback.hasMedia) return "#81949e"
                if (playback.fsr4Active && playback.fsr4ProofPassed) return "#41d995"
                if (playback.fsr4Active) return "#e8b35b"
                return "#81949e"
            }
        }

        Text {
            id: hudText
            objectName: "publishedFrameHud"
            text: {
                if (!playback.hasMedia) return ""
                const source = fsr.sourceWidth + "x" + fsr.sourceHeight
                // Show the stage of the last published image, never a pending
                // CPU/GPU candidate from an incomplete dispatch.
                const anvil = playback.anvilLiveStage === "reconstructing"
                    ? "  ·  ANVIL RECONSTRUCTING"
                    : playback.anvilLiveStage === "standby"
                        ? "  ·  ANVIL STANDBY"
                        : playback.anvilLiveStage === "bypassed"
                            ? "  ·  ANVIL BYPASSED"
                            : ""
                if (!playback.fsr4Active)
                    return source + anvil + "  ·  FSR INACTIVE"
                let res = source + "  →  " + playback.fsr4OutputWidth + "x" + playback.fsr4OutputHeight
                if (playback.lastFsr4DispatchMs > 0)
                    res += "  ·  " + playback.lastFsr4DispatchMs.toFixed(1) + " ms"
                return "FSR 4.1 RE  ·  " + res + anvil
            }
            color: "#b5c5cd"
            font.pixelSize: 11
            font.family: "monospace"
            width: Math.max(0, hud.width - 34)
            elide: Text.ElideRight
        }
    }
}
