import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Rectangle {
    id: root

    implicitHeight: 32
    implicitWidth: contentRow.implicitWidth + 16
    radius: Math.min(6, theme.radius)
    color: theme.surface
    border.color: theme.border
    border.width: 1

    signal requestImageImport()

    RowLayout {
        id: contentRow
        anchors.centerIn: parent
        spacing: 8

        // Tool indicator badge
        Rectangle {
            implicitWidth: 22
            implicitHeight: 20
            radius: Math.min(3, theme.radius)
            color: theme.surfaceElevated
            border.color: theme.borderMuted
            border.width: 1

            Text {
                anchors.centerIn: parent
                text: {
                    switch (backend.tool) {
                    case "pen": return "P";
                    case "highlighter": return "M";
                    case "select": return "V";
                    case "eraser": return "E";
                    case "rectangle": return "R";
                    case "ellipse": return "C";
                    case "line": return "L";
                    case "arrow": return "A";
                    case "text": return "T";
                    case "note": return "N";
                    case "connector": return "X";
                    case "hand": return "H";
                    case "laser": return "K";
                    default: return "P";
                    }
                }
                font.family: "monospace"
                font.pixelSize: 11
                font.bold: true
                color: theme.accent
            }

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                onClicked: {
                    // Quick toggle between Pen and Select
                    if (backend.tool === "pen") backend.tool = "select";
                    else backend.tool = "pen";
                }
            }
        }

        // Divider
        Rectangle {
            implicitWidth: 1
            implicitHeight: 16
            color: theme.borderMuted
        }

        // The 3 Theme-Aware Drawing Colors
        RowLayout {
            spacing: 6

            Repeater {
                model: backend.colorPalette

                delegate: Rectangle {
                    required property string modelData
                    required property int index

                    implicitWidth: 16
                    implicitHeight: 16
                    radius: 8
                    color: modelData
                    border.color: backend.activeColorIndex === index ? theme.text : "transparent"
                    border.width: 2

                    // Inner selection dot for high clarity
                    Rectangle {
                        anchors.centerIn: parent
                        visible: backend.activeColorIndex === index
                        width: 4
                        height: 4
                        radius: 2
                        color: theme.background
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: {
                            backend.activeColorIndex = index
                        }
                    }
                }
            }
        }

        // Divider
        Rectangle {
            implicitWidth: 1
            implicitHeight: 16
            color: theme.borderMuted
        }

        // Pen Size Selector (1: 3px, 2: 4px, 3: 5px, 4: 6px)
        RowLayout {
            spacing: 4

            Repeater {
                model: [
                    { size: 3.0, label: "1" },
                    { size: 4.0, label: "2" },
                    { size: 5.0, label: "3" },
                    { size: 6.0, label: "4" }
                ]

                delegate: Rectangle {
                    required property var modelData

                    implicitWidth: 20
                    implicitHeight: 20
                    radius: Math.min(3, theme.radius)
                    color: Math.abs(backend.penSize - modelData.size) < 0.1 ? theme.surfaceElevated : "transparent"
                    border.color: Math.abs(backend.penSize - modelData.size) < 0.1 ? theme.accent : "transparent"
                    border.width: 1

                    Text {
                        anchors.centerIn: parent
                        text: modelData.label
                        font.family: "monospace"
                        font.pixelSize: 10
                        font.bold: Math.abs(backend.penSize - modelData.size) < 0.1
                        color: Math.abs(backend.penSize - modelData.size) < 0.1 ? theme.accent : theme.textMuted
                    }

                    MouseArea {
                        anchors.fill: parent
                        cursorShape: Qt.PointingHandCursor
                        hoverEnabled: true
                        onClicked: {
                            backend.penSize = modelData.size
                        }
                    }
                }
            }
        }

        // Divider
        Rectangle {
            implicitWidth: 1
            implicitHeight: 16
            color: theme.borderMuted
        }

        // Minimal Paste Screenshot / Insert Photo Button
        Rectangle {
            id: pasteScreenshotBtn
            implicitWidth: 22
            implicitHeight: 20
            radius: Math.min(3, theme.radius)
            color: pasteArea.containsMouse ? theme.surfaceElevated : "transparent"
            border.color: pasteArea.containsMouse ? theme.accent : theme.borderMuted
            border.width: 1

            // Minimal clean vector screenshot / photo frame icon
            Item {
                anchors.centerIn: parent
                width: 12
                height: 10

                // Outer photo / screen frame
                Rectangle {
                    anchors.fill: parent
                    radius: 1
                    color: "transparent"
                    border.color: pasteArea.containsMouse ? theme.accent : theme.textMuted
                    border.width: 1
                }

                // Inner lens / landscape dot
                Rectangle {
                    x: 2
                    y: 2
                    width: 3
                    height: 3
                    radius: 1.5
                    color: pasteArea.containsMouse ? theme.accent : theme.textMuted
                }

                // Small hill / diagonal in bottom right
                Rectangle {
                    x: 6
                    y: 4
                    width: 4
                    height: 4
                    rotation: 45
                    color: pasteArea.containsMouse ? theme.accent : theme.textMuted
                }
            }

            ToolTip.visible: pasteArea.containsMouse
            ToolTip.text: "Paste Screenshot (Ctrl+V) / Click to paste or choose photo/PDF"
            ToolTip.delay: 400

            MouseArea {
                id: pasteArea
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true
                acceptedButtons: Qt.LeftButton | Qt.RightButton
                onClicked: (mouse) => {
                    if (mouse.button === Qt.RightButton || (mouse.modifiers & Qt.ShiftModifier)) {
                        root.requestImageImport()
                    } else {
                        if (!backend.pasteFromClipboard()) {
                            root.requestImageImport()
                        }
                    }
                }
            }
        }
    }
}

