import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root

    modal: true
    focus: true
    anchors.centerIn: parent
    width: Math.min(680, parent.width - 40)
    height: Math.min(520, parent.height - 40)
    padding: 16
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    background: Rectangle {
        color: theme.surface
        border.color: theme.border
        border.width: 1
        radius: Math.min(8, theme.radius)
    }

    contentItem: ColumnLayout {
        spacing: 12

        // Header
        RowLayout {
            Layout.fillWidth: true

            Text {
                text: "KEYBOARD SHORTCUTS"
                font.pixelSize: 13
                font.bold: true
                font.family: "monospace"
                color: theme.accent
                Layout.fillWidth: true
            }

            Text {
                text: "ESC to close"
                font.pixelSize: 11
                font.family: "monospace"
                color: theme.textMuted
            }
        }

        // Search Filter
        Rectangle {
            Layout.fillWidth: true
            height: 32
            color: theme.background
            border.color: searchInput.activeFocus ? theme.accent : theme.borderMuted
            border.width: 1
            radius: Math.min(4, theme.radius)

            TextInput {
                id: searchInput
                anchors.fill: parent
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                verticalAlignment: TextInput.AlignVCenter
                font.pixelSize: 12
                color: theme.text
                clip: true

                Text {
                    text: "Search shortcuts..."
                    color: theme.textMuted
                    font.pixelSize: 12
                    visible: !searchInput.text && !searchInput.activeFocus
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        // Shortcuts List
        ListView {
            id: listView
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            spacing: 4
            model: {
                var all = shortcuts.getShortcuts()
                var q = searchInput.text.toLowerCase().trim()
                if (!q) return all
                var filtered = []
                for (var i = 0; i < all.length; i++) {
                    var it = all[i]
                    if (it.name.toLowerCase().indexOf(q) >= 0 ||
                        it.category.toLowerCase().indexOf(q) >= 0 ||
                        it.currentKey.toLowerCase().indexOf(q) >= 0) {
                        filtered.push(it)
                    }
                }
                return filtered
            }

            delegate: Rectangle {
                required property var modelData
                width: listView.width
                height: 28
                radius: Math.min(3, theme.radius)
                color: rowMouse.containsMouse ? theme.surfaceElevated : "transparent"

                RowLayout {
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8

                    Text {
                        text: "[" + modelData.category + "]"
                        font.family: "monospace"
                        font.pixelSize: 11
                        color: theme.textMuted
                        Layout.preferredWidth: 90
                    }

                    Text {
                        text: modelData.name
                        font.pixelSize: 12
                        color: theme.text
                        Layout.fillWidth: true
                    }

                    Rectangle {
                        implicitWidth: keyText.implicitWidth + 12
                        implicitHeight: 20
                        radius: Math.min(3, theme.radius)
                        color: theme.background
                        border.color: theme.borderMuted
                        border.width: 1

                        Text {
                            id: keyText
                            anchors.centerIn: parent
                            text: modelData.currentKey
                            font.family: "monospace"
                            font.pixelSize: 11
                            font.bold: true
                            color: theme.accent
                        }
                    }
                }

                MouseArea {
                    id: rowMouse
                    anchors.fill: parent
                    hoverEnabled: true
                }
            }
        }
    }
}
