import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    implicitHeight: 22
    implicitWidth: statusText.implicitWidth + 16
    color: statusMouse.containsMouse ? theme.surfaceElevated : theme.surface
    border.color: statusMouse.containsMouse ? theme.accent : theme.borderMuted
    border.width: 1
    radius: Math.min(4, theme.radius)

    Text {
        id: statusText
        anchors.centerIn: parent
        text: backend.statusText
        font.family: "monospace"
        font.pixelSize: 10
        color: statusMouse.containsMouse ? theme.text : theme.textMuted
    }

    ToolTip.visible: statusMouse.containsMouse
    ToolTip.text: "Click to reset zoom to 100% (Ctrl+0)"
    ToolTip.delay: 400

    MouseArea {
        id: statusMouse
        anchors.fill: parent
        hoverEnabled: true
        cursorShape: Qt.PointingHandCursor
        onClicked: {
            backend.resetView()
        }
    }
}

