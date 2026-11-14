#include "backend.h"
#include "exporter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace DrawBoard {

Backend::Backend(Theme *theme, QObject *parent)
    : QObject(parent), m_theme(theme) {
    // History initial state
    m_history.pushState("Initial", m_doc.elements(), m_doc.selectedIds());

    connect(&m_doc, &Document::documentChanged, this, [this] {
        updateStatus();
        emit documentChanged();
    });

    connect(&m_doc, &Document::selectionChanged, this, [this] {
        updateStatus();
        emit selectionChanged();
    });

    connect(&m_doc, &Document::modifiedChanged, this, [this] {
        updateStatus();
        emit modifiedChanged();
    });

    connect(&m_history, &History::stateChanged, this, &Backend::historyChanged);

    if (m_theme) {
        connect(m_theme, &Theme::themeChanged, this, [this] {
            syncActiveColors();
            emit colorPaletteChanged();
            emit activeColorChanged();
            updateStatus();
        });
    }

    loadRecentFiles();

    // Autosave timer every 10s
    connect(&m_autosaveTimer, &QTimer::timeout, this, &Backend::performAutosave);
    m_autosaveTimer.start(10000);

    syncActiveColors();
    updateStatus();
}

Backend::~Backend() {
    saveRecentFiles();
}

void Backend::registerCanvas(CanvasItem *canvas) {
    m_canvas = canvas;
    if (m_canvas) {
        m_canvas->setDocument(&m_doc);
        m_canvas->setHistory(&m_history);
        m_canvas->setTheme(m_theme);
        m_canvas->setActiveTool(m_tool);
        m_canvas->setStrokeWidth(m_penSize);
        m_canvas->setStrokeColor(activeColor());
        m_canvas->setSnapToGrid(m_snapToGrid);
        m_canvas->setGridStyle(m_gridStyle);

        connect(m_canvas, &CanvasItem::zoomChanged, this, [this] {
            int pct = static_cast<int>(std::round(m_canvas->zoom() * 100.0));
            m_zoomText = QString("%1%").arg(pct);
            emit zoomTextChanged();
            updateStatus();
        });

        connect(m_canvas, &CanvasItem::pointerMoved, this, [this](qreal x, qreal y) {
            m_cursorWorldX = x;
            m_cursorWorldY = y;
            updateStatus();
        });

        connect(m_canvas, &CanvasItem::activeToolChanged, this, [this] {
            if (m_tool != m_canvas->activeTool()) {
                m_tool = m_canvas->activeTool();
                emit toolChanged();
                updateStatus();
            }
        });
    }
}

QStringList Backend::colorPalette() const {
    if (!m_theme)
        return {"#ebdbb2", "#fe8019", "#8ec07c"};

    return {
        m_theme->strokePrimary(),
        m_theme->strokeSecondary(),
        m_theme->strokeTertiary()
    };
}

QColor Backend::activeColor() const {
    QStringList palette = colorPalette();
    int idx = std::clamp(m_activeColorIndex, 0, static_cast<int>(palette.size()) - 1);
    return QColor(palette[idx]);
}

void Backend::setActiveColorIndex(int index) {
    if (index >= 0 && index < colorPalette().size() && m_activeColorIndex != index) {
        m_activeColorIndex = index;
        emit activeColorIndexChanged();
        emit activeColorChanged();
        if (m_canvas) {
            m_canvas->setStrokeColor(activeColor());
        }
        updateStatus();
    }
}

void Backend::syncActiveColors() {
    if (m_canvas) {
        m_canvas->setStrokeColor(activeColor());
    }
}

void Backend::setTool(const QString &tool) {
    if (m_tool != tool) {
        m_tool = tool;
        if (m_canvas) {
            m_canvas->setActiveTool(tool);
        }
        emit toolChanged();
        updateStatus();
    }
}

void Backend::setPenSize(qreal size) {
    if (!qFuzzyCompare(m_penSize, size) && size > 0.5) {
        m_penSize = size;
        if (m_canvas) {
            m_canvas->setStrokeWidth(size);
        }
        emit penSizeChanged();
        updateStatus();
    }
}

void Backend::setSnapToGrid(bool snap) {
    if (m_snapToGrid != snap) {
        m_snapToGrid = snap;
        if (m_canvas) m_canvas->setSnapToGrid(snap);
        emit snapToGridChanged();
        updateStatus();
    }
}

void Backend::setGridStyle(int style) {
    if (m_gridStyle != style) {
        m_gridStyle = style;
        if (m_canvas) m_canvas->setGridStyle(style);
        emit gridStyleChanged();
    }
}

void Backend::undo() {
    if (const HistoryState *state = m_history.undo()) {
        m_doc.setElements(state->elements);
        m_doc.setSelection(state->selectedIds);
    }
}

void Backend::redo() {
    if (const HistoryState *state = m_history.redo()) {
        m_doc.setElements(state->elements);
        m_doc.setSelection(state->selectedIds);
    }
}

void Backend::deleteSelected() {
    if (!m_doc.selectedIds().isEmpty()) {
        m_history.pushState("Delete", m_doc.elements(), m_doc.selectedIds());
        m_doc.deleteSelected();
    }
}

void Backend::duplicateSelected() {
    if (!m_doc.selectedIds().isEmpty()) {
        m_history.pushState("Duplicate", m_doc.elements(), m_doc.selectedIds());
        m_doc.duplicateSelected();
    }
}

void Backend::selectAll() {
    m_doc.selectAll();
}

void Backend::clearSelection() {
    m_doc.clearSelection();
}

void Backend::groupSelected() {
    if (m_doc.selectedIds().size() >= 2) {
        m_history.pushState("Group", m_doc.elements(), m_doc.selectedIds());
        m_doc.groupSelected();
    }
}

void Backend::ungroupSelected() {
    if (!m_doc.selectedIds().isEmpty()) {
        m_history.pushState("Ungroup", m_doc.elements(), m_doc.selectedIds());
        m_doc.ungroupSelected();
    }
}

void Backend::bringForward() {
    m_history.pushState("Bring Forward", m_doc.elements(), m_doc.selectedIds());
    m_doc.bringForward();
}

void Backend::sendBackward() {
    m_history.pushState("Send Backward", m_doc.elements(), m_doc.selectedIds());
    m_doc.sendBackward();
}

void Backend::bringToFront() {
    m_history.pushState("Bring to Front", m_doc.elements(), m_doc.selectedIds());
    m_doc.bringToFront();
}

void Backend::sendToBack() {
    m_history.pushState("Send to Back", m_doc.elements(), m_doc.selectedIds());
    m_doc.sendToBack();
}

void Backend::resetView() {
    if (m_canvas) m_canvas->resetView();
}

void Backend::zoomIn() {
    if (m_canvas) m_canvas->zoomIn();
}

void Backend::zoomOut() {
    if (m_canvas) m_canvas->zoomOut();
}

void Backend::zoomToFit() {
    if (m_canvas) m_canvas->zoomToFit();
}

void Backend::newDocument() {
    m_doc.clear();
    m_history.clear();
    m_history.pushState("New Document", m_doc.elements(), m_doc.selectedIds());
    if (m_canvas) m_canvas->resetView();
    emit documentChanged();
}

bool Backend::openDocument(const QString &path) {
    if (m_doc.loadFromFile(path)) {
        m_history.clear();
        m_history.pushState("Open Document", m_doc.elements(), m_doc.selectedIds());
        addRecentFile(path);
        if (m_canvas) m_canvas->zoomToFit();
        emit documentChanged();
        return true;
    }
    return false;
}

bool Backend::saveDocument(const QString &path) {
    QString target = path.isEmpty() ? m_doc.filePath() : path;
    if (target.isEmpty()) {
        return false;
    }

    if (m_doc.saveToFile(target)) {
        addRecentFile(target);
        emit documentChanged();
        return true;
    }
    return false;
}

bool Backend::exportFile(const QString &path, const QString &format, qreal scale, bool transparent) {
    ThemePalette pal = m_theme ? m_theme->current() : ThemePalette();
    QString fmt = format.toLower();

    if (fmt == "png") {
        return Exporter::exportPng(path, m_doc, pal, scale, transparent);
    } else if (fmt == "svg") {
        return Exporter::exportSvg(path, m_doc, pal, transparent);
    } else if (fmt == "pdf") {
        return Exporter::exportPdf(path, m_doc, pal, transparent);
    }
    return false;
}

void Backend::updateText(const QString &id, const QString &text) {
    Element *el = m_doc.findElement(id);
    if (el && el->text != text) {
        m_history.pushState("Edit Text", m_doc.elements(), m_doc.selectedIds());
        el->text = text;
        m_doc.updateElement(*el);
    }
}

bool Backend::handleKeyPress(int key, int modifiers) {
    Qt::KeyboardModifiers mods = static_cast<Qt::KeyboardModifiers>(modifiers);
    QString action = m_shortcuts.findActionForKey(key, mods);
    if (action.isEmpty())
        return false;

    if (action == "tool_pen") setTool("pen");
    else if (action == "tool_eraser") setTool("eraser");
    else if (action == "tool_select") setTool("select");
    else if (action == "tool_rect") setTool("rectangle");
    else if (action == "tool_ellipse") setTool("ellipse");
    else if (action == "tool_line") setTool("line");
    else if (action == "tool_arrow") setTool("arrow");
    else if (action == "tool_text") setTool("text");
    else if (action == "tool_note") setTool("note");
    else if (action == "tool_connector") setTool("connector");
    else if (action == "tool_hand") setTool("hand");
    else if (action == "zoom_in") zoomIn();
    else if (action == "zoom_out") zoomOut();
    else if (action == "zoom_reset") resetView();
    else if (action == "zoom_fit") zoomToFit();
    else if (action == "edit_undo") undo();
    else if (action == "edit_redo") redo();
    else if (action == "edit_delete") deleteSelected();
    else if (action == "edit_duplicate") duplicateSelected();
    else if (action == "edit_select_all") selectAll();
    else if (action == "edit_group") groupSelected();
    else if (action == "edit_ungroup") ungroupSelected();
    else if (action == "edit_bring_forward") bringForward();
    else if (action == "edit_send_backward") sendBackward();
    else if (action == "edit_bring_to_front") bringToFront();
    else if (action == "edit_send_to_back") sendToBack();
    else if (action == "file_new") newDocument();
    else if (action == "file_save") saveDocument();
    else if (action == "file_export") emit showExportDialog();
    else if (action == "show_shortcuts") emit showShortcutsDialog();
    else if (action == "toggle_grid") {
        setGridStyle((m_gridStyle + 1) % 4);
    }
    else return false;

    return true;
}

void Backend::updateStatus() {
    QString toolUpper = m_tool.toUpper();
    QString modTag = m_doc.isModified() ? "[+]" : "";
    QString countStr = QString("%1 items").arg(m_doc.elementCount());
    if (!m_doc.selectedIds().isEmpty()) {
        countStr = QString("%1/%2 selected").arg(m_doc.selectedIds().size()).arg(m_doc.elementCount());
    }
    QString coordStr = QString("(%1, %2)").arg(static_cast<int>(m_cursorWorldX)).arg(static_cast<int>(m_cursorWorldY));
    QString snapStr = m_snapToGrid ? "SNAP" : "";

    m_statusText = QString("[%1] %2px | %3 | %4 | %5 %6 %7")
                       .arg(toolUpper)
                       .arg(m_penSize)
                       .arg(m_zoomText)
                       .arg(countStr)
                       .arg(coordStr)
                       .arg(snapStr)
                       .arg(modTag)
                       .trimmed();

    emit statusTextChanged();
}

void Backend::loadRecentFiles() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty()) configDir = QDir::homePath() + "/.config/drawboard";
    QFile file(configDir + "/recent.json");
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (doc.isArray()) {
        m_recentFiles.clear();
        for (const auto &v : doc.array()) {
            if (v.isString()) m_recentFiles.append(v.toString());
        }
        emit recentFilesChanged();
    }
}

void Backend::saveRecentFiles() {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty()) configDir = QDir::homePath() + "/.config/drawboard";
    QDir().mkpath(configDir);

    QFile file(configDir + "/recent.json");
    if (file.open(QIODevice::WriteOnly)) {
        QJsonArray arr;
        for (const auto &p : m_recentFiles) arr.append(p);
        file.write(QJsonDocument(arr).toJson(QJsonDocument::Indented));
    }
}

void Backend::addRecentFile(const QString &path) {
    if (path.isEmpty()) return;
    m_recentFiles.removeAll(path);
    m_recentFiles.prepend(path);
    while (m_recentFiles.size() > 10) m_recentFiles.removeLast();
    saveRecentFiles();
    emit recentFilesChanged();
}

void Backend::performAutosave() {
    if (!m_doc.isModified() || m_doc.isEmpty())
        return;

    QString stateDir = QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    if (stateDir.isEmpty()) stateDir = QDir::homePath() + "/.local/state/drawboard";
    QDir().mkpath(stateDir);

    m_doc.saveToFile(stateDir + "/autosave.draw");
}

void Backend::checkAutosaveRecovery() {
    QString stateDir = QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    if (stateDir.isEmpty()) stateDir = QDir::homePath() + "/.local/state/drawboard";
    QString recoveryPath = stateDir + "/autosave.draw";

    if (QFile::exists(recoveryPath) && m_doc.isEmpty()) {
        // If an autosave file exists from a previous session, restore it
        openDocument(recoveryPath);
    }
}

} // namespace DrawBoard
