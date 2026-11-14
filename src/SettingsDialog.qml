import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Popup {
    id: root

    modal: true
    focus: true
    anchors.centerIn: parent
    width: Math.min(680, parent.width - 40)
    height: Math.min(540, parent.height - 40)
    padding: 0
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    property string currentTab: "theme"

    background: Rectangle {
        color: theme.surface
        border.color: theme.border
        border.width: 1
        radius: Math.min(8, theme.radius)
    }

    contentItem: RowLayout {
        spacing: 0

        // Sidebar Navigation
        Rectangle {
            Layout.fillHeight: true
            implicitWidth: 140
            color: theme.background
            border.color: theme.borderMuted
            border.width: 1
            radius: Math.min(8, theme.radius)

            ColumnLayout {
                anchors.fill: parent
                anchors.topMargin: 16
                anchors.leftMargin: 8
                anchors.rightMargin: 8
                spacing: 4

                Text {
                    text: "SETTINGS"
                    font.family: "monospace"
                    font.pixelSize: 11
                    font.bold: true
                    color: theme.accent
                    Layout.bottomMargin: 8
                    Layout.leftMargin: 4
                }

                Repeater {
                    model: [
                        { id: "theme", label: "Theme" },
                        { id: "canvas", label: "Canvas & Grid" },
                        { id: "drawing", label: "Drawing" },
                        { id: "shortcuts", label: "Shortcuts" },
                        { id: "about", label: "About" }
                    ]

                    delegate: Rectangle {
                        required property var modelData
                        Layout.fillWidth: true
                        height: 28
                        radius: Math.min(4, theme.radius)
                        color: root.currentTab === modelData.id ? theme.surfaceElevated : "transparent"

                        Text {
                            anchors.verticalCenter: parent.verticalCenter
                            anchors.left: parent.left
                            anchors.leftMargin: 8
                            text: modelData.label
                            font.pixelSize: 12
                            font.bold: root.currentTab === modelData.id
                            color: root.currentTab === modelData.id ? theme.accent : theme.textMuted
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.currentTab = modelData.id
                        }
                    }
                }

                Item { Layout.fillHeight: true }
            }
        }

        // Tab Content Area
        Rectangle {
            Layout.fillWidth: true
            Layout.fillHeight: true
            color: "transparent"

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 18
                spacing: 12

                // TAB 1: THEMES
                ColumnLayout {
                    visible: root.currentTab === "theme"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 10

                    Text {
                        text: "Theme Family & Variant"
                        font.pixelSize: 14
                        font.bold: true
                        color: theme.text
                    }

                    // Available Families
                    RowLayout {
                        spacing: 6
                        Repeater {
                            model: theme.availableFamilies

                            delegate: Rectangle {
                                required property string modelData
                                implicitWidth: famText.implicitWidth + 14
                                implicitHeight: 26
                                radius: Math.min(4, theme.radius)
                                color: theme.family === modelData ? theme.surfaceElevated : theme.background
                                border.color: theme.family === modelData ? theme.accent : theme.borderMuted
                                border.width: 1

                                Text {
                                    id: famText
                                    anchors.centerIn: parent
                                    text: modelData
                                    font.pixelSize: 11
                                    font.bold: theme.family === modelData
                                    color: theme.family === modelData ? theme.accent : theme.textMuted
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var variants = theme.getVariantsForFamily(modelData)
                                        if (variants.length > 0) {
                                            var newId = theme.getThemeId(modelData, variants[0])
                                            theme.setThemeId(newId)
                                        }
                                    }
                                }
                            }
                        }
                    }

                    // Available Variants for selected family
                    Text {
                        text: "Variants"
                        font.pixelSize: 12
                        color: theme.textMuted
                        Layout.topMargin: 6
                    }

                    RowLayout {
                        spacing: 6
                        Repeater {
                            model: theme.getVariantsForFamily(theme.family)

                            delegate: Rectangle {
                                required property string modelData
                                implicitWidth: varText.implicitWidth + 14
                                implicitHeight: 26
                                radius: Math.min(4, theme.radius)
                                color: theme.variant === modelData ? theme.accent : theme.background
                                border.color: theme.borderMuted
                                border.width: 1

                                Text {
                                    id: varText
                                    anchors.centerIn: parent
                                    text: modelData
                                    font.pixelSize: 11
                                    font.bold: theme.variant === modelData
                                    color: theme.variant === modelData ? theme.accentForeground : theme.text
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: {
                                        var newId = theme.getThemeId(theme.family, modelData)
                                        theme.setThemeId(newId)
                                    }
                                }
                            }
                        }
                    }

                    // Active Palette Tokens Preview
                    Text {
                        text: "Design Token Palette"
                        font.pixelSize: 12
                        color: theme.textMuted
                        Layout.topMargin: 10
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        implicitHeight: 80
                        radius: Math.min(4, theme.radius)
                        color: theme.background
                        border.color: theme.borderMuted
                        border.width: 1

                        RowLayout {
                            anchors.centerIn: parent
                            spacing: 12

                            Repeater {
                                model: [
                                    { name: "Canvas", color: theme.canvas },
                                    { name: "Surface", color: theme.surface },
                                    { name: "Accent", color: theme.accent },
                                    { name: "Stroke 1", color: theme.strokePrimary },
                                    { name: "Stroke 2", color: theme.strokeSecondary },
                                    { name: "Stroke 3", color: theme.strokeTertiary },
                                    { name: "Note", color: theme.note }
                                ]

                                delegate: ColumnLayout {
                                    required property var modelData
                                    spacing: 4

                                    Rectangle {
                                        implicitWidth: 32
                                        implicitHeight: 32
                                        radius: 16
                                        color: modelData.color
                                        border.color: theme.border
                                        border.width: 1
                                    }

                                    Text {
                                        text: modelData.name
                                        font.pixelSize: 9
                                        font.family: "monospace"
                                        color: theme.textMuted
                                        Layout.alignment: Qt.AlignHCenter
                                    }
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }

                // TAB 2: CANVAS & GRID
                ColumnLayout {
                    visible: root.currentTab === "canvas"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    Text {
                        text: "Canvas & Grid Configuration"
                        font.pixelSize: 14
                        font.bold: true
                        color: theme.text
                    }

                    // Grid Style
                    Text {
                        text: "Grid Style"
                        font.pixelSize: 11
                        color: theme.textMuted
                    }

                    RowLayout {
                        spacing: 8
                        Repeater {
                            model: [
                                { id: 0, label: "Dots" },
                                { id: 1, label: "Lines" },
                                { id: 2, label: "Crosses" },
                                { id: 3, label: "None" }
                            ]

                            delegate: Rectangle {
                                required property var modelData
                                implicitWidth: 70
                                implicitHeight: 28
                                radius: Math.min(4, theme.radius)
                                color: backend.gridStyle === modelData.id ? theme.surfaceElevated : theme.background
                                border.color: backend.gridStyle === modelData.id ? theme.accent : theme.borderMuted
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData.label
                                    font.pixelSize: 11
                                    color: backend.gridStyle === modelData.id ? theme.accent : theme.text
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: backend.gridStyle = modelData.id
                                }
                            }
                        }
                    }

                    // Snap to Grid
                    RowLayout {
                        spacing: 8
                        Layout.topMargin: 8

                        Rectangle {
                            implicitWidth: 18
                            implicitHeight: 18
                            radius: Math.min(3, theme.radius)
                            color: backend.snapToGrid ? theme.accent : theme.background
                            border.color: theme.borderMuted
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "✓"
                                visible: backend.snapToGrid
                                font.pixelSize: 11
                                color: theme.accentForeground
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: backend.snapToGrid = !backend.snapToGrid
                            }
                        }

                        Text {
                            text: "Snap to Grid"
                            font.pixelSize: 12
                            color: theme.text
                        }
                    }

                    Item { Layout.fillHeight: true }
                }

                // TAB 3: DRAWING
                ColumnLayout {
                    visible: root.currentTab === "drawing"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 12

                    Text {
                        text: "Drawing Preferences"
                        font.pixelSize: 14
                        font.bold: true
                        color: theme.text
                    }

                    Text {
                        text: "Default Pen Size"
                        font.pixelSize: 11
                        color: theme.textMuted
                    }

                    RowLayout {
                        spacing: 6
                        Repeater {
                            model: [1, 2, 4, 8, 12, 16]

                            delegate: Rectangle {
                                required property int modelData
                                implicitWidth: 32
                                implicitHeight: 28
                                radius: Math.min(4, theme.radius)
                                color: Math.abs(backend.penSize - modelData) < 0.1 ? theme.surfaceElevated : theme.background
                                border.color: Math.abs(backend.penSize - modelData) < 0.1 ? theme.accent : theme.borderMuted
                                border.width: 1

                                Text {
                                    anchors.centerIn: parent
                                    text: modelData + "px"
                                    font.pixelSize: 10
                                    font.family: "monospace"
                                    color: Math.abs(backend.penSize - modelData) < 0.1 ? theme.accent : theme.text
                                }

                                MouseArea {
                                    anchors.fill: parent
                                    cursorShape: Qt.PointingHandCursor
                                    onClicked: backend.penSize = modelData
                                }
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }
                }

                // TAB 4: SHORTCUTS
                ColumnLayout {
                    visible: root.currentTab === "shortcuts"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8

                    RowLayout {
                        Layout.fillWidth: true
                        Text {
                            text: "Keyboard Shortcuts"
                            font.pixelSize: 14
                            font.bold: true
                            color: theme.text
                            Layout.fillWidth: true
                        }

                        Rectangle {
                            implicitWidth: 70
                            implicitHeight: 24
                            radius: Math.min(3, theme.radius)
                            color: theme.background
                            border.color: theme.borderMuted
                            border.width: 1

                            Text {
                                anchors.centerIn: parent
                                text: "Reset All"
                                font.pixelSize: 10
                                color: theme.danger
                            }

                            MouseArea {
                                anchors.fill: parent
                                cursorShape: Qt.PointingHandCursor
                                onClicked: shortcuts.resetAll()
                            }
                        }
                    }

                    ListView {
                        id: scList
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        clip: true
                        spacing: 2
                        model: shortcuts.getShortcuts()

                        delegate: Rectangle {
                            required property var modelData
                            width: scList.width
                            height: 26
                            color: "transparent"

                            RowLayout {
                                anchors.fill: parent
                                Text {
                                    text: modelData.name
                                    font.pixelSize: 11
                                    color: theme.text
                                    Layout.fillWidth: true
                                }
                                Text {
                                    text: modelData.currentKey
                                    font.pixelSize: 10
                                    font.family: "monospace"
                                    font.bold: true
                                    color: modelData.isModified ? theme.warning : theme.accent
                                }
                            }
                        }
                    }
                }

                // TAB 5: ABOUT
                ColumnLayout {
                    visible: root.currentTab === "about"
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8

                    Text {
                        text: "Drawboard"
                        font.pixelSize: 16
                        font.bold: true
                        color: theme.accent
                    }

                    Text {
                        text: "Version 0.1.0 — Neovim for Visual Thinking"
                        font.pixelSize: 12
                        font.family: "monospace"
                        color: theme.textMuted
                    }

                    Rectangle {
                        Layout.fillWidth: true
                        height: 1
                        color: theme.borderMuted
                        Layout.topMargin: 4
                        Layout.bottomMargin: 4
                    }

                    Text {
                        text: "A serious personal visual workspace designed for Arch Linux & Omarchy.\n\n" +
                              "• Infinite vector canvas with high-performance C++ QPainter rendering\n" +
                              "• Natural freehand drawing with Catmull-Rom smoothing\n" +
                              "• Clean semantic design token theme system\n" +
                              "• Dynamic smart connectors for technical diagrams and architectures\n" +
                              "• High-resolution PNG, vector SVG, and vector PDF exports\n" +
                              "• Local-first versioned document storage with crash recovery"
                        font.pixelSize: 11
                        color: theme.text
                        lineHeight: 1.4
                    }

                    Item { Layout.fillHeight: true }
                }

                // Close Button
                RowLayout {
                    Layout.fillWidth: true
                    Item { Layout.fillWidth: true }
                    Rectangle {
                        implicitWidth: 60
                        implicitHeight: 26
                        radius: Math.min(3, theme.radius)
                        color: theme.surfaceElevated
                        border.color: theme.borderMuted
                        border.width: 1

                        Text {
                            anchors.centerIn: parent
                            text: "Done"
                            font.pixelSize: 11
                            color: theme.text
                        }

                        MouseArea {
                            anchors.fill: parent
                            cursorShape: Qt.PointingHandCursor
                            onClicked: root.close()
                        }
                    }
                }
            }
        }
    }
}
