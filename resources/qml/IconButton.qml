// One semantic icon-button control: vector icon, accessible name, hover/focus/pressed states.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root
    property string iconName: ""
    property string textLabel: ""
    property string tooltip: ""
    property bool primary: false
    property bool checked: false
    signal clicked()

    PlayerTheme { id: theme }
    implicitWidth: textLabel.length ? Math.max(52, contents.implicitWidth + 24) : (primary ? 48 : 38)
    implicitHeight: 40
    Layout.preferredHeight: implicitHeight
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: tooltip.length ? tooltip : (textLabel || iconName)
    Accessible.onPressAction: if (root.enabled) root.clicked()

    Rectangle {
        anchors.fill: parent
        radius: 9
        color: !root.enabled ? "transparent"
             : pointer.pressed ? theme.accentPressed
             : root.primary ? theme.accent
             : root.checked ? theme.accentTint
             : pointer.containsMouse || root.activeFocus ? theme.surfaceRaised
             : "transparent"
        border.width: root.checked || root.activeFocus ? 1 : 0
        border.color: root.activeFocus ? theme.accent : theme.border
        Behavior on color { ColorAnimation { duration: 110 } }
    }

    Row {
        id: contents
        anchors.centerIn: parent
        spacing: root.textLabel.length && root.iconName.length ? 6 : 0

        VectorIcon {
            visible: root.iconName.length > 0
            anchors.verticalCenter: parent.verticalCenter
            name: root.iconName
            width: root.primary ? 23 : 20
            height: width
            color: !root.enabled ? theme.disabled
                 : root.primary && !pointer.pressed ? theme.background
                 : root.checked || pointer.containsMouse || root.activeFocus ? theme.foreground
                 : theme.secondary
        }
        Text {
            visible: root.textLabel.length > 0
            anchors.verticalCenter: parent.verticalCenter
            text: root.textLabel
            color: !root.enabled ? theme.disabled : root.primary && !pointer.pressed ? theme.background : theme.foreground
            font.pixelSize: 12
            font.weight: Font.DemiBold
        }
    }

    MouseArea {
        id: pointer
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: root.enabled ? Qt.PointingHandCursor : Qt.ArrowCursor
        enabled: root.enabled
        onClicked: root.clicked()
    }
    Keys.onReturnPressed: if (root.enabled) root.clicked()
    Keys.onSpacePressed: if (root.enabled) root.clicked()
    ToolTip.visible: pointer.containsMouse && root.tooltip.length > 0
    ToolTip.delay: 400
    ToolTip.text: root.tooltip
}
