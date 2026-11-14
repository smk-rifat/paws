import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import DrawBoard 1.0

ApplicationWindow {
    id: appWindow
    width: 1280
    height: 800
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: backend.documentTitle + (backend.isModified ? " *" : "") + " — Drawboard"
    color: theme.canvas

    // Main Canvas (Dominates application)
    CanvasItem {
        id: canvas
        anchors.fill: parent
        focus: true

        Component.onCompleted: {
            backend.registerCanvas(canvas)
        }

        onEditTextRequested: (id, text, screenRect) => {
            textEditorOverlay.openEditor(id, text, screenRect)
        }
    }

    // Top-Left Dot Menu (•)
    DotMenu {
        id: dotMenu
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 14
        z: 10

        onOpenExportDialog: exportDialog.open()
        onOpenShortcutsDialog: shortcutsOverlay.open()
        onOpenSettingsDialog: (tab) => {
            settingsDialog.currentTab = tab
            settingsDialog.open()
        }
    }

    // Bottom-Center Minimal Controls (Active tool, 3 theme colors, pen size)
    CompactToolbar {
        id: compactToolbar
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom
        anchors.bottomMargin: 14
        z: 10
    }

    // Bottom-Right Subtle Neovim Status Line
    StatusLine {
        id: statusLine
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.margins: 14
        z: 10
    }

    // Floating Inline Text Editor
    Item {
        id: textEditorOverlay
        visible: false
        z: 20

        property string currentElementId: ""

        Rectangle {
            id: editorBox
            color: theme.surface
            border.color: theme.accent
            border.width: 1.5
            radius: Math.min(4, theme.radius)

            ScrollView {
                anchors.fill: parent
                anchors.margins: 4

                TextArea {
                    id: textInputArea
                    wrapMode: TextEdit.Wrap
                    font.pixelSize: 15
                    font.family: "sans-serif"
                    color: theme.text
                    selectByMouse: true
                    background: null

                    Keys.onEscapePressed: {
                        textEditorOverlay.closeEditor(false)
                    }

                    Keys.onReturnPressed: (event) => {
                        if (event.modifiers & Qt.ShiftModifier || event.modifiers & Qt.ControlModifier) {
                            textInputArea.insert(textInputArea.cursorPosition, "\n")
                        } else {
                            textEditorOverlay.closeEditor(true)
                        }
                    }
                }
            }
        }

        function openEditor(elementId, initialText, screenRect) {
            currentElementId = elementId
            textInputArea.text = initialText
            editorBox.x = Math.max(10, Math.min(appWindow.width - 240, screenRect.x))
            editorBox.y = Math.max(10, Math.min(appWindow.height - 140, screenRect.y))
            editorBox.width = Math.max(180, screenRect.width)
            editorBox.height = Math.max(70, screenRect.height)
            visible = true
            textInputArea.forceActiveFocus()
            textInputArea.selectAll()
        }

        function closeEditor(commit) {
            if (visible && commit && currentElementId !== "") {
                backend.updateText(currentElementId, textInputArea.text)
            }
            visible = false
            currentElementId = ""
            canvas.forceActiveFocus()
        }

        MouseArea {
            anchors.fill: parent
            z: -1
            onClicked: textEditorOverlay.closeEditor(true)
        }
    }

    // File Dialogs (Native portal integration)
    FileDialog {
        id: openDialog
        title: "Open Document"
        nameFilters: ["Drawboard Documents (*.draw)", "All Files (*)"]
        onAccepted: {
            var path = selectedFile.toString().replace(/^file:\/\//, "")
            backend.openDocument(path)
        }
    }

    FileDialog {
        id: saveDialog
        title: "Save Document"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Drawboard Documents (*.draw)", "All Files (*)"]
        onAccepted: {
            var path = selectedFile.toString().replace(/^file:\/\//, "")
            if (!path.endsWith(".draw")) path += ".draw"
            backend.saveDocument(path)
        }
    }

    Item {
        id: fileDialogLoader
        function openFileDialog() { openDialog.open() }
        function saveFileDialog() { saveDialog.open() }
    }

    // Export Dialog Popup
    ExportDialog {
        id: exportDialog
        z: 30
    }

    // Shortcuts Cheatsheet Overlay
    ShortcutsOverlay {
        id: shortcutsOverlay
        z: 30
    }

    // Full Settings Dialog
    SettingsDialog {
        id: settingsDialog
        z: 30
    }

    // Global Key Handling (Keyboard-first Neovim philosophy)
    Item {
        id: globalKeysHandler
        anchors.fill: parent
        focus: true

        Connections {
            target: backend
            function onShowExportDialog() { exportDialog.open() }
            function onShowShortcutsDialog() { shortcutsOverlay.open() }
            function onShowSettingsDialog() { settingsDialog.open() }
        }

        Keys.onPressed: (event) => {
            // If inline text editor is open, don't intercept keys
            if (textEditorOverlay.visible)
                return

            // If a popup/modal is open, let it handle Esc
            if (exportDialog.visible || shortcutsOverlay.visible || settingsDialog.visible)
                return

            if (event.key === Qt.Key_Escape) {
                backend.clearSelection()
                backend.tool = "select"
                event.accepted = true
                return
            }

            if (event.key === Qt.Key_O && (event.modifiers & Qt.ControlModifier)) {
                openDialog.open()
                event.accepted = true
                return
            }

            if (event.key === Qt.Key_S && (event.modifiers & Qt.ControlModifier)) {
                if (backend.filePath === "") {
                    saveDialog.open()
                } else {
                    backend.saveDocument()
                }
                event.accepted = true
                return
            }

            // Pass to backend shortcut dispatcher
            var handled = backend.handleKeyPress(event.key, event.modifiers)
            if (handled) {
                event.accepted = true
            }
        }
    }
}
