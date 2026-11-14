import QtQuick
import QtQuick.Layouts

Rectangle {
    id: root

    implicitHeight: 22
    implicitWidth: statusText.implicitWidth + 16
    color: theme.surface
    border.color: theme.borderMuted
    border.width: 1
    radius: Math.min(4, theme.radius)

    Text {
        id: statusText
        anchors.centerIn: parent
        text: backend.statusText
        font.family: "monospace"
        font.pixelSize: 10
        color: theme.textMuted
    }
}
