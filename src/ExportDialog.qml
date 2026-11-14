import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root

    modal: true
    focus: true
    anchors.centerIn: parent
    width: Math.min(460, parent.width - 40)
    padding: 16
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property string format: "png"
    property real exportScale: 2.0
    property bool transparent: false
    property string statusMessage: ""

    background: Rectangle {
        color: theme.surface
        border.color: theme.border
        border.width: 1
        radius: Math.min(8, theme.radius)
    }

    contentItem: ColumnLayout {
        spacing: 14

        // Title
        Text {
            text: "EXPORT DRAWING"
            font.pixelSize: 13
            font.bold: true
            font.family: "monospace"
            color: theme.accent
        }

        // Format selector (PNG, SVG, PDF)
        ColumnLayout {
            spacing: 4
            Text {
                text: "Format"
                font.pixelSize: 11
                color: theme.textMuted
            }

            RowLayout {
                spacing: 8

                Repeater {
                    model: ["PNG", "SVG", "PDF"]

                    delegate: Rectangle {
                        required property string modelData
                        implicitWidth: 80
                        implicitHeight: 28
                        radius: Math.min(4, theme.radius)
                        color: root.format.toUpperCase() === modelData ? theme.surfaceElevated : theme.background
                        border.color: root.format.toUpperCase() === modelData ? theme.accent : theme.borderMuted
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: modelData
                            font.family: "monospace"
                            font.pixelSize: 11
                            font.bold: root.format.toUpperCase() === modelData
                            color: root.format.toUpperCase() === modelData ? theme.accent : theme.text
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: {
                                root.format = modelData.toLowerCase()
                                updateDefaultPath()
                            }
                        }
                    }
                }
            }
        }

        // Output File Path
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Text {
                text: "Target File"
                font.pixelSize: 11
                color: theme.textMuted
            }

            Rectangle {
                Layout.fillWidth: true
                height: 32
                color: theme.background
                border.color: pathInput.activeFocus ? theme.accent : theme.borderMuted
                border.width: 1
                radius: Math.min(4, theme.radius)

                TextInput {
                    id: pathInput
                    anchors.fill: parent
                    anchors.leftMargin: 8
                    anchors.rightMargin: 8
                    verticalAlignment: TextInput.AlignVCenter
                    font.pixelSize: 11
                    font.family: "monospace"
                    color: theme.text
                    selectByMouse: true
                }
            }
        }

        // PNG Options (Scale & Transparent)
        RowLayout {
            visible: root.format === "png"
            spacing: 16

            // Scale
            RowLayout {
                spacing: 6
                Text {
                    text: "Scale:"
                    font.pixelSize: 11
                    color: theme.textMuted
                }

                Repeater {
                    model: [
                        { val: 1.0, label: "1x" },
                        { val: 2.0, label: "2x" },
                        { val: 3.0, label: "3x" }
                    ]

                    delegate: Rectangle {
                        required property var modelData
                        implicitWidth: 32
                        implicitHeight: 24
                        radius: Math.min(3, theme.radius)
                        color: Math.abs(root.exportScale - modelData.val) < 0.1 ? theme.surfaceElevated : theme.background
                        border.color: Math.abs(root.exportScale - modelData.val) < 0.1 ? theme.accent : theme.borderMuted
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: modelData.label
                            font.pixelSize: 10
                            font.family: "monospace"
                            color: Math.abs(root.exportScale - modelData.val) < 0.1 ? theme.accent : theme.textMuted
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.exportScale = modelData.val
                        }
                    }
                }
            }

            // Transparent background checkbox
            RowLayout {
                spacing: 6
                Rectangle {
                    implicitWidth: 16
                    implicitHeight: 16
                    radius: Math.min(3, theme.radius)
                    color: root.transparent ? theme.accent : theme.background
                    border.color: theme.borderMuted
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: "✓"
                        visible: root.transparent
                        font.pixelSize: 10
                        color: theme.accentForeground
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        onClicked: root.transparent = !root.transparent
                    }
                }

                Text {
                    text: "Transparent Background"
                    font.pixelSize: 11
                    color: theme.text
                }
            }
        }

        // Status Feedback
        Text {
            text: root.statusMessage
            font.pixelSize: 11
            color: root.statusMessage.indexOf("Error") >= 0 ? theme.danger : theme.success
            visible: root.statusMessage !== ""
        }

        // Action Buttons
        RowLayout {
            Layout.fillWidth: true
            Layout.topMargin: 4
            spacing: 8

            Item { Layout.fillWidth: true }

            Rectangle {
                implicitWidth: 70
                implicitHeight: 28
                radius: Math.min(4, theme.radius)
                color: cancelMouse.containsMouse ? theme.surfaceElevated : "transparent"
                border.color: theme.borderMuted
                border.width: 1

                Text {
                    anchors.centerIn: parent
                    text: "Cancel"
                    font.pixelSize: 12
                    color: theme.textMuted
                }

                MouseArea {
                    id: cancelMouse
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    hoverEnabled: true
                    onClicked: root.close()
                }
            }

            Rectangle {
                implicitWidth: 80
                implicitHeight: 28
                radius: Math.min(4, theme.radius)
                color: theme.accent

                Text {
                    anchors.centerIn: parent
                    text: "Export"
                    font.pixelSize: 12
                    font.bold: true
                    color: theme.accentForeground
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        var target = pathInput.text.trim()
                        if (!target) {
                            root.statusMessage = "Error: Specify a valid path"
                            return
                        }
                        var ok = backend.exportFile(target, root.format, root.exportScale, root.transparent)
                        if (ok) {
                            root.statusMessage = "Exported successfully!"
                            QTimer.singleShot(800, function() { root.close(); root.statusMessage = ""; })
                        } else {
                            root.statusMessage = "Error: Export failed"
                        }
                    }
                }
            }
        }
    }

    function updateDefaultPath() {
        var base = backend.documentTitle.replace(".draw", "")
        if (base === "Untitled") base = "drawing"
        pathInput.text = "/home/smk/" + base + "." + root.format
    }

    onOpened: {
        root.statusMessage = ""
        updateDefaultPath()
    }
}
