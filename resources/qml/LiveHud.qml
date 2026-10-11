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
    width: hudText.implicitWidth + 34
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
                if (!playback.hasMedia) return "#555560"
                if (playback.fsr4Active && playback.fsr4ProofPassed) return "#41d995"
                if (playback.fsr4Active) return "#e8b35b"
                return "#6a6a75"
            }
        }

        Text {
            id: hudText
            text: {
                if (!playback.hasMedia) return ""
                var res = fsr.sourceWidth + "x" + fsr.sourceHeight
                if (playback.fsr4Active && playback.fsr4OutputWidth > 0)
                    res += " → " + playback.fsr4OutputWidth + "x" + playback.fsr4OutputHeight
                if (playback.lastFsr4DispatchMs > 0)
                    res += "  " + playback.lastFsr4DispatchMs.toFixed(1) + "ms"
                return res
            }
            color: "#b5c5cd"
            font.pixelSize: 11
            font.family: "monospace"
        }
    }
}
