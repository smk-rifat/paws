import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Layouts
import DrawBoard 1.0

ApplicationWindow {
    id: appWindow
    width: backend.windowWidth > 0 ? backend.windowWidth : 1280
    height: backend.windowHeight > 0 ? backend.windowHeight : 800
    minimumWidth: 640
    minimumHeight: 480
    visible: true
    title: backend.documentTitle + (backend.isModified ? " *" : "") + " — Paws"
    color: theme.canvas

    Component.onCompleted: {
        if (backend.windowMaximized) {
            appWindow.showMaximized()
        } else if (backend.windowX >= 0 && backend.windowY >= 0) {
            appWindow.x = backend.windowX
            appWindow.y = backend.windowY
        }
    }

    function recordWindowGeometry() {
        var isMax = (appWindow.visibility === Window.Maximized)
        var w = isMax ? (backend.windowWidth > 0 ? backend.windowWidth : 1280) : appWindow.width
        var h = isMax ? (backend.windowHeight > 0 ? backend.windowHeight : 800) : appWindow.height
        var wx = isMax ? backend.windowX : appWindow.x
        var wy = isMax ? backend.windowY : appWindow.y
        backend.saveWindowGeometry(wx, wy, w, h, isMax)
    }

    Timer {
        id: geometryDebounce
        interval: 1000
        repeat: false
        onTriggered: appWindow.recordWindowGeometry()
    }

    onWidthChanged: if (visible && visibility !== Window.Maximized) geometryDebounce.restart()
    onHeightChanged: if (visible && visibility !== Window.Maximized) geometryDebounce.restart()
    onXChanged: if (visible && visibility !== Window.Maximized) geometryDebounce.restart()
    onYChanged: if (visible && visibility !== Window.Maximized) geometryDebounce.restart()
    onVisibilityChanged: {
        if (visible) {
            geometryDebounce.restart()
        }
    }

    onClosing: (close) => {
        recordWindowGeometry()
    }

    Component.onDestruction: {
        recordWindowGeometry()
    }

    // Main Canvas (Dominates application)
    CanvasItem {
        id: canvas
        anchors.fill: parent
        focus: true

        Component.onCompleted: {
            backend.registerCanvas(canvas)
        }

        onCreateTextRequested: (worldX, worldY, screenX, screenY, fontSize, color) => {
            textEditorOverlay.openForCreate(worldX, worldY, screenX, screenY, fontSize, color)
        }

        onEditTextRequested: (id, text, screenX, screenY, fontSize, color) => {
            textEditorOverlay.openForEdit(id, text, screenX, screenY, fontSize, color)
        }
    }

    // Top-Left Dot Menu (•)
    property bool zenMode: false
    readonly property bool shouldHideControls: zenMode

    function toggleZenMode() {
        zenMode = !zenMode
        toastNotification.show(zenMode ? "Zen Mode: On (Press Z to restore)" : "Zen Mode: Off")
    }

    DotMenu {
        id: dotMenu
        anchors.left: parent.left
        anchors.top: parent.top
        anchors.margins: 14
        z: 10
        opacity: appWindow.shouldHideControls ? 0.0 : 1.0
        visible: opacity > 0.01

        Behavior on opacity {
            NumberAnimation { duration: 180; easing.type: Easing.InOutQuad }
        }

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
        opacity: appWindow.shouldHideControls ? 0.0 : 1.0
        visible: opacity > 0.01

        Behavior on opacity {
            NumberAnimation { duration: 180; easing.type: Easing.InOutQuad }
        }

        onRequestImageImport: imageImportDialog.open()
    }

    // Inline Canvas Text Editor (tldraw-style: transparent, inline, no separate block/card)
    Item {
        id: textEditorOverlay
        anchors.fill: parent
        visible: false
        z: 20

        property string currentElementId: ""
        property real targetWorldX: 0
        property real targetWorldY: 0
        property real initialScreenX: 0
        property real initialScreenY: 0
        property color textColor: theme.text
        property real baseFontSize: 16

        // Backdrop click commits and closes the editor
        MouseArea {
            anchors.fill: parent
            z: -1
            onPressed: textEditorOverlay.commitAndClose()
        }

        // Inline Text Box (borderless, transparent background, auto-expanding)
        Item {
            id: editorContainer
            x: textEditorOverlay.initialScreenX
            y: textEditorOverlay.initialScreenY
            width: Math.max(60, textInputArea.contentWidth + 10)
            height: Math.max(24, textInputArea.contentHeight + 4)

            // Subtle 1px accent outline only while actively typing
            Rectangle {
                anchors.fill: parent
                anchors.margins: -2
                color: "transparent"
                border.color: theme.accent
                border.width: 1
                opacity: 0.6
                radius: 2
            }

            TextEdit {
                id: textInputArea
                anchors.fill: parent
                wrapMode: TextEdit.NoWrap
                font.pixelSize: Math.max(12, Math.round(textEditorOverlay.baseFontSize * canvas.zoom))
                font.family: "sans-serif"
                color: textEditorOverlay.textColor
                selectByMouse: true
                focus: true

                Keys.onEscapePressed: (event) => {
                    textEditorOverlay.cancelAndClose()
                    event.accepted = true
                }

                Keys.onReturnPressed: (event) => {
                    if (event.modifiers & Qt.ShiftModifier || event.modifiers & Qt.ControlModifier) {
                        textInputArea.insert(textInputArea.cursorPosition, "\n")
                        event.accepted = true
                    } else {
                        textEditorOverlay.commitAndClose()
                        event.accepted = true
                    }
                }
            }
        }

        function openForCreate(worldX, worldY, screenX, screenY, fontSize, colorStr) {
            currentElementId = ""
            targetWorldX = worldX
            targetWorldY = worldY
            initialScreenX = screenX
            initialScreenY = screenY
            baseFontSize = fontSize || 16
            textColor = colorStr ? colorStr : theme.text
            textInputArea.text = ""
            visible = true
            textInputArea.forceActiveFocus()
        }

        function openForEdit(elementId, initialText, screenX, screenY, fontSize, colorStr) {
            currentElementId = elementId
            initialScreenX = screenX
            initialScreenY = screenY
            baseFontSize = fontSize || 16
            textColor = colorStr ? colorStr : theme.text
            textInputArea.text = initialText
            visible = true
            textInputArea.forceActiveFocus()
            textInputArea.selectAll()
        }

        function commitAndClose() {
            if (!visible) return
            var text = textInputArea.text
            if (currentElementId === "") {
                backend.createText(targetWorldX, targetWorldY, text)
            } else {
                backend.updateText(currentElementId, text)
            }
            visible = false
            currentElementId = ""
            canvas.forceActiveFocus()
        }

        function cancelAndClose() {
            var text = textInputArea.text.trim()
            if (text.length > 0) {
                commitAndClose()
            } else {
                visible = false
                currentElementId = ""
                canvas.forceActiveFocus()
            }
        }
    }

    // File Dialogs (Native portal integration)
    FileDialog {
        id: openDialog
        title: "Open Document"
        nameFilters: ["Paws Documents (*.paws *.draw)", "All Files (*)"]
        onAccepted: {
            var path = selectedFile.toString().replace(/^file:\/\//, "")
            backend.openDocument(path)
        }
    }

    FileDialog {
        id: saveDialog
        title: "Save Document"
        fileMode: FileDialog.SaveFile
        nameFilters: ["Paws Documents (*.paws *.draw)", "All Files (*)"]
        onAccepted: {
            var path = selectedFile.toString().replace(/^file:\/\//, "")
            if (!path.endsWith(".draw")) path += ".draw"
            backend.saveDocument(path)
        }
    }

    FileDialog {
        id: imageImportDialog
        title: "Import Photo, Screenshot, or PDF"
        fileMode: FileDialog.OpenFile
        nameFilters: [
            "Images and Documents (*.png *.jpg *.jpeg *.webp *.bmp *.pdf)",
            "Images (*.png *.jpg *.jpeg *.webp *.bmp)",
            "PDF Documents (*.pdf)",
            "All Files (*)"
        ]
        onAccepted: {
            var path = selectedFile.toString().replace(/^file:\/\//, "")
            backend.insertImageFromFile(path)
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

    // Startup Easter Egg Line-Art Cat Overlay
    CatOverlay {
        id: catOverlay
    }

    // Minimalist Floating Toast Notification
    Rectangle {
        id: toastNotification
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        anchors.topMargin: 20
        z: 100
        implicitWidth: toastLabel.implicitWidth + 28
        implicitHeight: 30
        radius: 15
        color: theme.surfaceElevated
        border.color: theme.accent
        border.width: 1
        opacity: 0.0
        visible: opacity > 0.01

        Behavior on opacity {
            NumberAnimation { duration: 180; easing.type: Easing.InOutQuad }
        }

        Text {
            id: toastLabel
            anchors.centerIn: parent
            text: ""
            color: theme.text
            font.family: "sans-serif"
            font.pixelSize: 12
            font.bold: true
        }

        Timer {
            id: toastTimer
            interval: 1800
            onTriggered: toastNotification.opacity = 0.0
        }

        function show(msg) {
            toastLabel.text = msg
            toastNotification.opacity = 1.0
            toastTimer.restart()
        }
    }

    // Global Shortcuts System (Application-wide, keyboard-first)
    readonly property bool shortcutsEnabled: !textEditorOverlay.visible && !exportDialog.visible && !shortcutsOverlay.visible && !settingsDialog.visible

    Shortcut { sequence: "P"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.tool = "pen" }
    Shortcut { sequence: "M"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.tool = "highlighter" }
    Shortcut { sequence: "V"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.tool = "select" }
    Shortcut { sequence: "E"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.tool = "eraser" }
    Shortcut { sequence: "R"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.tool = "rectangle" }
    Shortcut { sequences: ["C", "O"]; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.tool = "ellipse" }
    Shortcut { sequence: "L"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.tool = "line" }
    Shortcut { sequence: "A"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.tool = "arrow" }
    Shortcut { sequence: "T"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.tool = "text" }
    Shortcut { sequence: "N"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.tool = "note" }
    Shortcut { sequence: "X"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.tool = "connector" }
    Shortcut { sequence: "H"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.tool = "hand" }
    Shortcut { sequence: "K"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.tool = "laser" }
    Shortcut { sequence: "Z"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: appWindow.toggleZenMode() }
    Shortcut { sequence: "1"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.penSize = 3.0 }
    Shortcut { sequence: "2"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.penSize = 4.0 }
    Shortcut { sequence: "3"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.penSize = 5.0 }
    Shortcut { sequence: "4"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.penSize = 6.0 }
    Shortcut { sequence: "Tab"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.cycleColor(true) }
    Shortcut { sequences: ["Shift+Tab", "Backtab"]; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.cycleColor(false) }
    Shortcut { sequence: "Ctrl+Shift+C"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.copySelectionToClipboard() }
    Shortcut { sequence: "S"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.snapToGrid = !backend.snapToGrid }
    Shortcut { sequence: "G"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.gridStyle = (backend.gridStyle + 1) % 4 }
    Shortcut { sequence: "Delete"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.deleteSelected() }
    Shortcut { sequence: "Backspace"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.deleteSelected() }
    Shortcut { sequences: [StandardKey.Undo]; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.undo() }
    Shortcut { sequences: [StandardKey.Redo]; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.redo() }
    Shortcut { sequences: [StandardKey.SelectAll]; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.selectAll() }
    Shortcut { sequence: "Ctrl+D"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.duplicateSelected() }
    Shortcut { sequence: "Ctrl+G"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.groupSelected() }
    Shortcut { sequence: "Ctrl+Shift+G"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.ungroupSelected() }
    Shortcut { sequence: "]"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.bringForward() }
    Shortcut { sequence: "["; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.sendBackward() }
    Shortcut { sequence: "Shift+]"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.bringToFront() }
    Shortcut { sequence: "Shift+["; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.sendToBack() }
    Shortcut { sequences: [StandardKey.ZoomIn]; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.zoomIn() }
    Shortcut { sequences: [StandardKey.ZoomOut]; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.zoomOut() }
    Shortcut { sequence: "Ctrl+0"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.resetView() }
    Shortcut { sequence: "Shift+1"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.zoomToFit() }
    Shortcut { sequence: "Ctrl+N"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.newDocument() }
    Shortcut { sequence: "Ctrl+O"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: openDialog.open() }
    Shortcut { sequences: [StandardKey.Paste, "Ctrl+V"]; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: { if (!backend.pasteFromClipboard()) imageImportDialog.open(); } }
    Shortcut { sequence: "Ctrl+S"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: { if (backend.filePath === "") saveDialog.open(); else backend.saveDocument(); } }
    Shortcut { sequence: "Ctrl+E"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: exportDialog.open() }
    Shortcut { sequence: "Ctrl+'"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: backend.gridStyle = (backend.gridStyle + 1) % 4 }
    Shortcut { sequence: "Ctrl+M"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: dotMenu.menuOpen = !dotMenu.menuOpen }
    Shortcut { sequence: "?"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: shortcutsOverlay.open() }
    Shortcut { sequence: "F1"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: shortcutsOverlay.open() }
    Shortcut { sequence: "Escape"; context: Qt.ApplicationShortcut; enabled: appWindow.shortcutsEnabled; onActivated: { backend.clearSelection(); backend.tool = "select"; } }

    Connections {
        target: backend
        function onShowExportDialog() { exportDialog.open() }
        function onShowShortcutsDialog() { shortcutsOverlay.open() }
        function onShowSettingsDialog() { settingsDialog.open() }
        function onShowOpenDialog() { openDialog.open() }
        function onShowImportImageDialog() { imageImportDialog.open() }
        function onShowDotMenu() { dotMenu.menuOpen = !dotMenu.menuOpen }
        function onToastRequested(message) { toastNotification.show(message) }
        function onToggleZenModeRequested() { appWindow.toggleZenMode() }
    }
}
