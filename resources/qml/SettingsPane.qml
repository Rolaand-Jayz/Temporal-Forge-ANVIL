// SettingsControls.qml — reusable building blocks for the Settings dialog.
//
// These are pure QML components (not stubs — they are real, working UI
// primitives). Keeping them in one file means every slider / section / toggle
// in the dialog looks and behaves the same way without copy-pasted styling.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

// A scrollable pane with a header + subtitle. All sections live inside.
Item {
    id: root
    property string title: ""
    property string subtitle: ""
    default property alias content: column.children

    ScrollView {
        id: scrollView
        anchors.fill: parent
        clip: true
        contentWidth: availableWidth
        ScrollBar.horizontal.policy: ScrollBar.AlwaysOff

        ColumnLayout {
            id: column
            width: scrollView.availableWidth
            spacing: 18

            Item {
                Layout.fillWidth: true
                Layout.preferredHeight: 8
            }

            Text {
                Layout.leftMargin: 24
                text: root.title
                visible: root.title.length > 0
                color: "#f1f6f5"
                font.pixelSize: 22
                font.bold: true
            }
            Text {
                Layout.leftMargin: 24
                Layout.rightMargin: 24
                text: root.subtitle
                visible: root.subtitle.length > 0
                color: "#81949e"
                font.pixelSize: 11
                wrapMode: Text.WordWrap
                Layout.fillWidth: true
            }

            // --- children go here ---
        }
    }
}
