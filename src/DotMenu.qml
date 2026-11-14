import QtQuick
import QtQuick.Controls
import QtQuick.Layouts

Item {
    id: root

    property bool menuOpen: false
    signal openExportDialog()
    signal openShortcutsDialog()
    signal openSettingsDialog(string initialTab)

    width: 28
    height: 28

    // Minimal • button
    Rectangle {
        id: dotButton
        anchors.fill: parent
        radius: Math.min(4, theme.radius)
        color: dotMouse.containsMouse || root.menuOpen ? theme.surfaceElevated : "transparent"
        border.color: dotMouse.containsMouse || root.menuOpen ? theme.border : "transparent"
        border.width: 1

        Text {
            anchors.centerIn: parent
            text: "•"
            font.pixelSize: 22
            font.bold: true
            color: dotMouse.containsMouse || root.menuOpen ? theme.accent : theme.textMuted
        }

        MouseArea {
            id: dotMouse
            anchors.fill: parent
            hoverEnabled: true
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                root.menuOpen = !root.menuOpen
            }
        }
    }

    // Hyprland-inspired floating popup
    Popup {
        id: menuPopup
        x: 0
        y: 34
        width: 240
        padding: 6
        visible: root.menuOpen
        onClosed: root.menuOpen = false
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

        background: Rectangle {
            color: theme.surface
            border.color: theme.border
            border.width: 1
            radius: Math.min(6, theme.radius)
        }

        contentItem: ColumnLayout {
            spacing: 2

            // Menu Section: Document
            Repeater {
                model: [
                    { label: "New Document", shortcut: "Ctrl+N", action: "new" },
                    { label: "Open...", shortcut: "Ctrl+O", action: "open" },
                    { label: "Save", shortcut: "Ctrl+S", action: "save" },
                    { label: "Export (PNG / SVG / PDF)...", shortcut: "Ctrl+E", action: "export" }
                ]

                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    height: 28
                    radius: Math.min(4, theme.radius)
                    color: itemMouse.containsMouse ? theme.surfaceElevated : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8

                        Text {
                            text: modelData.label
                            color: theme.text
                            font.pixelSize: 12
                            Layout.fillWidth: true
                        }
                        Text {
                            text: modelData.shortcut
                            color: theme.textMuted
                            font.pixelSize: 10
                            font.family: "monospace"
                        }
                    }

                    MouseArea {
                        id: itemMouse
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.menuOpen = false
                            if (modelData.action === "new") backend.newDocument()
                            else if (modelData.action === "open") fileDialogLoader.openFileDialog()
                            else if (modelData.action === "save") {
                                if (backend.filePath === "") fileDialogLoader.saveFileDialog()
                                else backend.saveDocument()
                            }
                            else if (modelData.action === "export") root.openExportDialog()
                        }
                    }
                }
            }

            // Divider
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: theme.borderMuted
                Layout.topMargin: 4
                Layout.bottomMargin: 4
            }

            // Menu Section: View & Tools
            Repeater {
                model: [
                    { label: "Themes & Palette...", shortcut: "", tab: "theme" },
                    { label: "Canvas & Grid...", shortcut: "Ctrl+'", tab: "canvas" },
                    { label: "Keyboard Shortcuts...", shortcut: "?", tab: "shortcuts" },
                    { label: "Settings...", shortcut: "", tab: "general" }
                ]

                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    height: 28
                    radius: Math.min(4, theme.radius)
                    color: itemMouse2.containsMouse ? theme.surfaceElevated : "transparent"

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 8
                        anchors.rightMargin: 8

                        Text {
                            text: modelData.label
                            color: theme.text
                            font.pixelSize: 12
                            Layout.fillWidth: true
                        }
                        Text {
                            text: modelData.shortcut
                            color: theme.textMuted
                            font.pixelSize: 10
                            font.family: "monospace"
                        }
                    }

                    MouseArea {
                        id: itemMouse2
                        anchors.fill: parent
                        hoverEnabled: true
                        cursorShape: Qt.PointingHandCursor
                        onClicked: {
                            root.menuOpen = false
                            if (modelData.tab === "shortcuts") root.openShortcutsDialog()
                            else root.openSettingsDialog(modelData.tab)
                        }
                    }
                }
            }

            // Divider
            Rectangle {
                Layout.fillWidth: true
                height: 1
                color: theme.borderMuted
                Layout.topMargin: 4
                Layout.bottomMargin: 4
            }

            // Document info
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 8
                Layout.rightMargin: 8
                Layout.topMargin: 2
                Layout.bottomMargin: 2

                Text {
                    text: backend.documentTitle + (backend.isModified ? " *" : "")
                    color: theme.textMuted
                    font.pixelSize: 11
                    elide: Text.ElideRight
                    Layout.fillWidth: true
                }
                Text {
                    text: theme.displayName
                    color: theme.accent
                    font.pixelSize: 10
                    font.family: "monospace"
                }
            }
        }
    }
}
