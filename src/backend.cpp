#include "backend.h"
#include "exporter.h"
#include "geometry.h"

#include <QBuffer>
#include <QClipboard>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>
#include <QImage>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMimeData>
#include <QProcess>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QUrl>
#include <QUuid>

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
            if (!m_loadingSettings) {
                saveSettings();
            }
        });
    }

    loadSettings();
    loadRecentFiles();

    // Autosave timer every 10s
    connect(&m_autosaveTimer, &QTimer::timeout, this, &Backend::performAutosave);
    m_autosaveTimer.start(10000);

    syncActiveColors();
    updateStatus();
}

Backend::~Backend() {
    saveRecentFiles();
    saveSettings();
}

void Backend::registerCanvas(CanvasItem *canvas) {
    m_canvas = canvas;
    if (m_canvas) {
        m_canvas->setBackend(this);
        m_canvas->setDocument(&m_doc);
        m_canvas->setHistory(&m_history);
        m_canvas->setTheme(m_theme);
        m_canvas->setActiveTool(m_tool);
        m_canvas->setStrokeWidth(m_penSize);
        m_canvas->setStrokeColor(activeColor());
        m_canvas->setSnapToGrid(m_snapToGrid);
        m_canvas->setGridStyle(m_gridStyle);
        m_canvas->setGridSize(m_gridSize);

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
                if (!m_loadingSettings) {
                    saveSettings();
                }
            }
        });
    }
}

QStringList Backend::colorPalette() const {
    if (!m_theme)
        return {"#ebdbb2", "#ff8787", "#fe8019", "#8ec07c", "#60a5fa", "#fabd2f", "#bd93f9"};

    return {
        m_theme->strokePrimary(),
        "#ff8787",
        m_theme->strokeSecondary(),
        m_theme->strokeTertiary(),
        "#60a5fa",
        "#fabd2f",
        "#bd93f9"
    };
}

QColor Backend::activeColor() const {
    QStringList palette = colorPalette();
    int idx = std::clamp(m_activeColorIndex, 0, static_cast<int>(palette.size()) - 1);
    return QColor(palette[idx]);
}

void Backend::setActiveColorIndex(int index) {
    if (index >= 0 && index < colorPalette().size()) {
        bool indexChanged = (m_activeColorIndex != index);
        m_activeColorIndex = index;
        if (indexChanged) {
            emit activeColorIndexChanged();
            emit activeColorChanged();
        }

        QColor col = activeColor();
        if (m_canvas) {
            m_canvas->setStrokeColor(col);
        }

        // Apply selected color to currently selected elements
        if (!m_doc.selectedIds().isEmpty()) {
            bool elementChanged = false;
            auto elements = m_doc.elements();
            const auto &selIds = m_doc.selectedIds();
            for (auto &el : elements) {
                if (selIds.contains(el.id) && !el.locked && el.strokeColor != col) {
                    el.strokeColor = col;
                    elementChanged = true;
                }
            }
            if (elementChanged) {
                m_doc.setElements(elements);
                m_history.pushState("Change Color", m_doc.elements(), m_doc.selectedIds());
                emit documentChanged();
                if (m_canvas) m_canvas->update();
            }
        }

        updateStatus();
        if (indexChanged && !m_loadingSettings) {
            saveSettings();
        }
    }
}

void Backend::cycleColor(bool forward) {
    const auto pal = colorPalette();
    if (pal.isEmpty())
        return;
    int total = pal.size();
    int next = forward ? (m_activeColorIndex + 1) % total : (m_activeColorIndex + total - 1) % total;
    setActiveColorIndex(next);
}

void Backend::cyclePenSize(bool forward) {
    static const QVector<qreal> sizes = {3.0, 4.0, 5.0, 6.0};
    int currentIdx = 0;
    for (int i = 0; i < sizes.size(); ++i) {
        if (std::abs(m_penSize - sizes[i]) < 0.1) {
            currentIdx = i;
            break;
        }
    }
    int next = forward ? (currentIdx + 1) % sizes.size() : (currentIdx + sizes.size() - 1) % sizes.size();
    setPenSize(sizes[next]);
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
        if (!m_loadingSettings) {
            saveSettings();
        }
    }
}

void Backend::setPenSize(qreal size) {
    if (size > 0.5) {
        bool sizeChanged = !qFuzzyCompare(m_penSize, size);
        m_penSize = size;
        if (m_canvas) {
            m_canvas->setStrokeWidth(size);
        }
        if (sizeChanged) {
            emit penSizeChanged();
        }

        // Apply stroke width to currently selected elements
        if (!m_doc.selectedIds().isEmpty()) {
            bool elementChanged = false;
            auto elements = m_doc.elements();
            const auto &selIds = m_doc.selectedIds();
            for (auto &el : elements) {
                if (selIds.contains(el.id) && !el.locked && !qFuzzyCompare(el.strokeWidth, size)) {
                    el.strokeWidth = size;
                    if (el.type == ElementType::Stroke) {
                        el.bounds = Geometry::calculateStrokeBounds(el.points, el.strokeWidth);
                    }
                    elementChanged = true;
                }
            }
            if (elementChanged) {
                m_doc.setElements(elements);
                m_history.pushState("Change Stroke Width", m_doc.elements(), m_doc.selectedIds());
                emit documentChanged();
                if (m_canvas) m_canvas->update();
            }
        }

        updateStatus();
        if (sizeChanged && !m_loadingSettings) {
            saveSettings();
        }
    }
}

void Backend::setSnapToGrid(bool snap) {
    if (m_snapToGrid != snap) {
        m_snapToGrid = snap;
        if (m_canvas) m_canvas->setSnapToGrid(snap);
        emit snapToGridChanged();
        updateStatus();
        if (!m_loadingSettings) {
            saveSettings();
        }
    }
}

void Backend::setGridStyle(int style) {
    if (m_gridStyle != style) {
        m_gridStyle = style;
        if (m_canvas) m_canvas->setGridStyle(style);
        emit gridStyleChanged();
        if (!m_loadingSettings) {
            saveSettings();
        }
    }
}

void Backend::setGridSize(qreal size) {
    if (!qFuzzyCompare(m_gridSize, size) && size >= 5.0) {
        m_gridSize = size;
        if (m_canvas) m_canvas->setGridSize(size);
        emit gridSizeChanged();
        if (!m_loadingSettings) {
            saveSettings();
        }
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
        m_doc.deleteSelected();
        m_history.pushState("Delete", m_doc.elements(), m_doc.selectedIds());
    }
}

void Backend::duplicateSelected() {
    if (!m_doc.selectedIds().isEmpty()) {
        m_doc.duplicateSelected();
        m_history.pushState("Duplicate", m_doc.elements(), m_doc.selectedIds());
    }
}

void Backend::selectAll() {
    m_doc.selectAll();
    setTool("select");
}

void Backend::clearSelection() {
    m_doc.clearSelection();
}

void Backend::groupSelected() {
    if (m_doc.selectedIds().size() >= 2) {
        m_doc.groupSelected();
        m_history.pushState("Group", m_doc.elements(), m_doc.selectedIds());
    }
}

void Backend::ungroupSelected() {
    if (!m_doc.selectedIds().isEmpty()) {
        m_doc.ungroupSelected();
        m_history.pushState("Ungroup", m_doc.elements(), m_doc.selectedIds());
    }
}

void Backend::bringForward() {
    m_doc.bringForward();
    m_history.pushState("Bring Forward", m_doc.elements(), m_doc.selectedIds());
}

void Backend::sendBackward() {
    m_doc.sendBackward();
    m_history.pushState("Send Backward", m_doc.elements(), m_doc.selectedIds());
}

void Backend::bringToFront() {
    m_doc.bringToFront();
    m_history.pushState("Bring to Front", m_doc.elements(), m_doc.selectedIds());
}

void Backend::sendToBack() {
    m_doc.sendToBack();
    m_history.pushState("Send to Back", m_doc.elements(), m_doc.selectedIds());
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

void Backend::createText(qreal worldX, qreal worldY, const QString &text) {
    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        return;
    }

    Element el;
    el.type = ElementType::Text;
    el.text = text;
    el.fontSize = 16.0;
    el.fontFamily = "sans-serif";
    el.strokeColor = m_canvas ? m_canvas->strokeColor() : QColor("#ebdbb2");
    el.bounds = Geometry::calculateTextBounds(el.text, QPointF(worldX, worldY), el.fontSize, el.fontFamily);

    QString id = m_doc.addElement(el);
    m_doc.select(id);
    m_history.pushState("Add Text", m_doc.elements(), m_doc.selectedIds());
    emit documentChanged();
}

void Backend::updateText(const QString &id, const QString &text) {
    Element *el = m_doc.findElement(id);
    if (!el) return;

    QString trimmed = text.trimmed();
    if (trimmed.isEmpty()) {
        m_doc.removeElement(id);
        m_history.pushState("Delete Text", m_doc.elements(), m_doc.selectedIds());
        emit documentChanged();
        return;
    }

    if (el->text != text) {
        el->text = text;
        if (el->type == ElementType::Text) {
            el->bounds = Geometry::calculateTextBounds(el->text, el->bounds.topLeft(), el->fontSize, el->fontFamily);
        }
        m_doc.updateElement(*el);
        m_history.pushState("Edit Text", m_doc.elements(), m_doc.selectedIds());
        emit documentChanged();
    }
}

bool Backend::insertImage(const QImage &image, const QString &sourceName, const QPointF &preferredWorldPos) {
    if (image.isNull())
        return false;

    QByteArray bytes;
    QBuffer buffer(&bytes);
    buffer.open(QIODevice::WriteOnly);
    if (!image.save(&buffer, "PNG")) {
        return false;
    }

    QPointF centerWorld = preferredWorldPos;
    if (centerWorld.isNull()) {
        if (m_canvas && m_canvas->width() > 0 && m_canvas->height() > 0) {
            centerWorld = m_canvas->screenToWorld(QPointF(m_canvas->width() * 0.5, m_canvas->height() * 0.5));
        } else {
            centerWorld = QPointF(0, 0);
        }
    }

    qreal origW = image.width();
    qreal origH = image.height();
    qreal maxW = 800.0;
    qreal maxH = 600.0;
    qreal scale = 1.0;
    if (origW > maxW || origH > maxH) {
        scale = std::min(maxW / origW, maxH / origH);
    }
    qreal w = std::max(20.0, origW * scale);
    qreal h = std::max(20.0, origH * scale);
    QRectF bounds(centerWorld.x() - w * 0.5, centerWorld.y() - h * 0.5, w, h);

    Element elem;
    elem.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    elem.type = ElementType::Image;
    elem.bounds = bounds;
    elem.image = image;
    elem.assetData = bytes;
    elem.mimeType = "image/png";
    elem.zIndex = m_doc.elements().size();

    m_doc.addElement(elem);
    m_doc.clearSelection();
    m_doc.select(elem.id);
    setTool("select");

    m_history.pushState(QString("Insert %1").arg(sourceName), m_doc.elements(), m_doc.selectedIds());
    emit documentChanged();
    emit selectionChanged();
    updateStatus();
    return true;
}

bool Backend::insertPdf(const QString &pdfPath, const QPointF &preferredWorldPos) {
    if (!QFile::exists(pdfPath))
        return false;

    QTemporaryDir tempDir;
    if (!tempDir.isValid())
        return false;

    QString outPrefix = tempDir.filePath("page");
    QProcess proc;
    QStringList args{"-png", "-r", "150", "-f", "1", "-l", "1", pdfPath, outPrefix};
    proc.start("pdftoppm", args);
    if (!proc.waitForFinished(5000) || proc.exitCode() != 0) {
        return false;
    }

    QDir dir(tempDir.path());
    QStringList pngFiles = dir.entryList(QStringList() << "*.png", QDir::Files, QDir::Name);
    if (pngFiles.isEmpty())
        return false;

    QImage img;
    if (!img.load(dir.filePath(pngFiles.first())))
        return false;

    return insertImage(img, QFileInfo(pdfPath).fileName(), preferredWorldPos);
}

bool Backend::insertImageFromFile(const QString &filePath, const QPointF &preferredWorldPos) {
    if (filePath.isEmpty() || !QFile::exists(filePath))
        return false;

    QFileInfo fi(filePath);
    QString ext = fi.suffix().toLower();
    if (ext == "pdf") {
        return insertPdf(filePath, preferredWorldPos);
    }

    QImage img;
    if (img.load(filePath)) {
        return insertImage(img, fi.fileName(), preferredWorldPos);
    }
    return false;
}

bool Backend::copySelectionToClipboard() {
    QVector<Element> targets;
    if (!m_doc.selectedIds().isEmpty()) {
        for (const auto &el : m_doc.elements()) {
            if (m_doc.selectedIds().contains(el.id) && !el.hidden) {
                targets.append(el);
            }
        }
    } else {
        for (const auto &el : m_doc.elements()) {
            if (!el.hidden) {
                targets.append(el);
            }
        }
    }

    if (targets.isEmpty()) {
        emit toastRequested("Nothing to copy to clipboard");
        return false;
    }

    ThemePalette pal = m_theme ? m_theme->current() : ThemePalette();
    QImage img = Exporter::renderImage(targets, pal, 2.0, true, 20.0);
    if (img.isNull())
        return false;

    QClipboard *cb = QGuiApplication::clipboard();
    if (cb) {
        cb->setImage(img);
        emit toastRequested(m_doc.selectedIds().isEmpty() ? "Snapshot copied to clipboard" : "Selection copied to clipboard");
        return true;
    }
    return false;
}

bool Backend::pasteFromClipboard() {
    QClipboard *cb = QGuiApplication::clipboard();
    if (!cb)
        return false;

    // 1. Direct image from clipboard (screenshot)
    QImage img = cb->image();
    if (!img.isNull()) {
        return insertImage(img, "Screenshot");
    }

    // 2. MIME data with URLs (file dragged or copied from file manager)
    const QMimeData *mime = cb->mimeData();
    if (mime && mime->hasUrls()) {
        for (const QUrl &url : mime->urls()) {
            QString localPath = url.toLocalFile();
            if (!localPath.isEmpty() && QFile::exists(localPath)) {
                if (insertImageFromFile(localPath)) {
                    return true;
                }
            }
        }
    }

    // 3. MIME data with text containing a file path
    if (mime && mime->hasText()) {
        QString text = mime->text().trimmed();
        if (text.startsWith("file://")) {
            text = QUrl(text).toLocalFile();
        }
        if (!text.isEmpty() && QFile::exists(text)) {
            if (insertImageFromFile(text)) {
                return true;
            }
        }
    }

    return false;
}

bool Backend::handleKeyPress(int key, int modifiers) {
    Qt::KeyboardModifiers mods = static_cast<Qt::KeyboardModifiers>(modifiers);
    bool ctrl = (mods & Qt::ControlModifier);
    bool shift = (mods & Qt::ShiftModifier);
    bool alt = (mods & Qt::AltModifier);

    // 1. Direct standard keys (fastest and most reliable)
    if (!ctrl && !alt) {
        // Arrow keys nudge selected elements (tldraw behavior: 1px by default, 10px with Shift)
        if (!m_doc.selectedIds().isEmpty()) {
            qreal nudgeStep = shift ? 10.0 : 1.0;
            QPointF delta(0, 0);
            if (key == Qt::Key_Left)  delta = QPointF(-nudgeStep, 0);
            else if (key == Qt::Key_Right) delta = QPointF(nudgeStep, 0);
            else if (key == Qt::Key_Up)    delta = QPointF(0, -nudgeStep);
            else if (key == Qt::Key_Down)  delta = QPointF(0, nudgeStep);

            if (!delta.isNull()) {
                m_doc.moveSelected(delta);
                m_history.pushState("Nudge", m_doc.elements(), m_doc.selectedIds());
                return true;
            }
        }

        if (!shift) {
            switch (key) {
            case Qt::Key_P: setTool("pen"); return true;
            case Qt::Key_M: setTool("highlighter"); return true;
            case Qt::Key_V: setTool("select"); return true;
            case Qt::Key_E: setTool("eraser"); return true;
            case Qt::Key_R: setTool("rectangle"); return true;
            case Qt::Key_O:
            case Qt::Key_C: setTool("ellipse"); return true;
            case Qt::Key_L: setTool("line"); return true;
            case Qt::Key_A: setTool("arrow"); return true;
            case Qt::Key_T: setTool("text"); return true;
            case Qt::Key_N: setTool("note"); return true;
            case Qt::Key_X: setTool("connector"); return true;
            case Qt::Key_H: setTool("hand"); return true;
            case Qt::Key_K: setTool("laser"); return true;
            case Qt::Key_Z: emit toggleZenModeRequested(); return true;
            case Qt::Key_S: setSnapToGrid(!m_snapToGrid); return true;
            case Qt::Key_G: setGridStyle((m_gridStyle + 1) % 4); return true;
            case Qt::Key_1: setPenSize(3.0); return true;
            case Qt::Key_2: setPenSize(4.0); return true;
            case Qt::Key_3: setPenSize(5.0); return true;
            case Qt::Key_4: setPenSize(6.0); return true;
            case Qt::Key_Tab: cycleColor(true); return true;
            case Qt::Key_Delete:
            case Qt::Key_Backspace:
                deleteSelected(); return true;
            case Qt::Key_BracketRight: bringForward(); return true;
            case Qt::Key_BracketLeft: sendBackward(); return true;
            case Qt::Key_Escape:
                clearSelection();
                setTool("select");
                return true;
            case Qt::Key_Equal:
            case Qt::Key_Plus:
                zoomIn(); return true;
            case Qt::Key_Minus:
                zoomOut(); return true;
            default: break;
            }
        } else {
            // Shift is held
            switch (key) {
            case Qt::Key_Backtab:
            case Qt::Key_Tab:
                cycleColor(false); return true;
            case Qt::Key_Question:
            case Qt::Key_Slash:
                emit showShortcutsDialog(); return true;
            case Qt::Key_Exclam:
            case Qt::Key_1:
                zoomToFit(); return true;
            case Qt::Key_BraceRight:
            case Qt::Key_BracketRight:
                bringToFront(); return true;
            case Qt::Key_BraceLeft:
            case Qt::Key_BracketLeft:
                sendToBack(); return true;
            case Qt::Key_Plus:
                zoomIn(); return true;
            default: break;
            }
        }
    } else if (ctrl && !alt) {
        if (!shift) {
            switch (key) {
            case Qt::Key_Z: undo(); return true;
            case Qt::Key_Y: redo(); return true;
            case Qt::Key_V: pasteFromClipboard(); return true;
            case Qt::Key_D: duplicateSelected(); return true;
            case Qt::Key_A: selectAll(); return true;
            case Qt::Key_G: groupSelected(); return true;
            case Qt::Key_N: newDocument(); return true;
            case Qt::Key_O: emit showOpenDialog(); return true;
            case Qt::Key_S: saveDocument(); return true;
            case Qt::Key_E: emit showExportDialog(); return true;
            case Qt::Key_M: emit showDotMenu(); return true;
            case Qt::Key_Equal:
            case Qt::Key_Plus:
                zoomIn(); return true;
            case Qt::Key_Minus:
                zoomOut(); return true;
            case Qt::Key_0:
                resetView(); return true;
            case Qt::Key_Apostrophe:
                setGridStyle((m_gridStyle + 1) % 4); return true;
            default: break;
            }
        } else {
            // Ctrl + Shift
            switch (key) {
            case Qt::Key_C: copySelectionToClipboard(); return true;
            case Qt::Key_Z: redo(); return true;
            case Qt::Key_G: ungroupSelected(); return true;
            case Qt::Key_BracketRight: bringToFront(); return true;
            case Qt::Key_BracketLeft: sendToBack(); return true;
            case Qt::Key_Plus:
            case Qt::Key_Equal:
                zoomIn(); return true;
            default: break;
            }
        }
    }

    if (key == Qt::Key_F1) {
        emit showShortcutsDialog();
        return true;
    }

    // 2. Custom rebinding query from ShortcutManager
    QString action = m_shortcuts.findActionForKey(key, mods);
    if (!action.isEmpty()) {
        if (action == "tool_pen") { setTool("pen"); return true; }
        if (action == "tool_highlighter") { setTool("highlighter"); return true; }
        if (action == "tool_eraser") { setTool("eraser"); return true; }
        if (action == "tool_select") { setTool("select"); return true; }
        if (action == "tool_rect") { setTool("rectangle"); return true; }
        if (action == "tool_ellipse") { setTool("ellipse"); return true; }
        if (action == "tool_line") { setTool("line"); return true; }
        if (action == "tool_arrow") { setTool("arrow"); return true; }
        if (action == "tool_text") { setTool("text"); return true; }
        if (action == "tool_note") { setTool("note"); return true; }
        if (action == "tool_connector") { setTool("connector"); return true; }
        if (action == "tool_hand") { setTool("hand"); return true; }
        if (action == "tool_laser") { setTool("laser"); return true; }
        if (action == "view_zen") { emit toggleZenModeRequested(); return true; }
        if (action == "palette_cycle") { cycleColor(true); return true; }
        if (action == "pen_size_1") { setPenSize(3.0); return true; }
        if (action == "pen_size_2") { setPenSize(4.0); return true; }
        if (action == "pen_size_3") { setPenSize(5.0); return true; }
        if (action == "pen_size_4") { setPenSize(6.0); return true; }
        if (action == "zoom_in") { zoomIn(); return true; }
        if (action == "zoom_out") { zoomOut(); return true; }
        if (action == "zoom_reset") { resetView(); return true; }
        if (action == "zoom_fit") { zoomToFit(); return true; }
        if (action == "edit_undo") { undo(); return true; }
        if (action == "edit_redo") { redo(); return true; }
        if (action == "edit_copy_image") { copySelectionToClipboard(); return true; }
        if (action == "edit_paste") { pasteFromClipboard(); return true; }
        if (action == "edit_delete") { deleteSelected(); return true; }
        if (action == "edit_duplicate") { duplicateSelected(); return true; }
        if (action == "edit_select_all") { selectAll(); return true; }
        if (action == "edit_group") { groupSelected(); return true; }
        if (action == "edit_ungroup") { ungroupSelected(); return true; }
        if (action == "edit_bring_forward") { bringForward(); return true; }
        if (action == "edit_send_backward") { sendBackward(); return true; }
        if (action == "edit_bring_to_front") { bringToFront(); return true; }
        if (action == "edit_send_to_back") { sendToBack(); return true; }
        if (action == "file_new") { newDocument(); return true; }
        if (action == "file_open") { emit showOpenDialog(); return true; }
        if (action == "file_save") { saveDocument(); return true; }
        if (action == "file_export") { emit showExportDialog(); return true; }
        if (action == "show_shortcuts") { emit showShortcutsDialog(); return true; }
        if (action == "toggle_grid") { setGridStyle((m_gridStyle + 1) % 4); return true; }
        if (action == "toggle_snap") { setSnapToGrid(!m_snapToGrid); return true; }
        if (action == "toggle_menu") { emit showDotMenu(); return true; }
    }

    return false;
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
    if (configDir.isEmpty()) configDir = QDir::homePath() + "/.config/paws";
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
    if (configDir.isEmpty()) configDir = QDir::homePath() + "/.config/paws";
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
    if (stateDir.isEmpty()) stateDir = QDir::homePath() + "/.local/state/paws";
    QDir().mkpath(stateDir);

    m_doc.saveToFile(stateDir + "/autosave.draw");
}

void Backend::checkAutosaveRecovery() {
    QString stateDir = QStandardPaths::writableLocation(QStandardPaths::StateLocation);
    if (stateDir.isEmpty()) stateDir = QDir::homePath() + "/.local/state/paws";
    QString recoveryPath = stateDir + "/autosave.draw";

    if (QFile::exists(recoveryPath) && m_doc.isEmpty()) {
        // If an autosave file exists from a previous session, restore it
        openDocument(recoveryPath);
    }
}

void Backend::saveWindowGeometry(int x, int y, int width, int height, bool maximized) {
    m_windowX = x;
    m_windowY = y;
    if (width > 100 && height > 100) {
        m_windowWidth = width;
        m_windowHeight = height;
    }
    m_windowMaximized = maximized;
    emit windowGeometryChanged();
    if (!m_loadingSettings) {
        saveSettings();
    }
}

QString Backend::settingsFilePath() const {
    if (!m_customSettingsPath.isEmpty())
        return m_customSettingsPath;
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty())
        configDir = QDir::homePath() + "/.config/paws";
    return configDir + "/settings.json";
}

void Backend::loadSettings(const QString &customPath) {
    QString filePath = customPath.isEmpty() ? settingsFilePath() : customPath;
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return;

    m_loadingSettings = true;
    QJsonObject obj = doc.object();

    if (obj.contains("theme") && m_theme) {
        QString themeId = obj["theme"].toString();
        if (!themeId.isEmpty()) {
            m_theme->setThemeId(themeId);
        }
    }

    if (obj.contains("gridStyle")) {
        m_gridStyle = obj["gridStyle"].toInt(0);
        emit gridStyleChanged();
    }
    if (obj.contains("gridSize")) {
        m_gridSize = obj["gridSize"].toDouble(20.0);
        emit gridSizeChanged();
    }
    if (obj.contains("snapToGrid")) {
        m_snapToGrid = obj["snapToGrid"].toBool(false);
        emit snapToGridChanged();
    }
    if (obj.contains("penSize")) {
        m_penSize = obj["penSize"].toDouble(3.0);
        emit penSizeChanged();
    }
    if (obj.contains("activeColorIndex")) {
        m_activeColorIndex = obj["activeColorIndex"].toInt(0);
        emit activeColorIndexChanged();
        emit activeColorChanged();
    }
    if (obj.contains("tool")) {
        m_tool = obj["tool"].toString("pen");
        emit toolChanged();
    }
    if (obj.contains("windowWidth")) {
        m_windowWidth = obj["windowWidth"].toInt(1280);
    }
    if (obj.contains("windowHeight")) {
        m_windowHeight = obj["windowHeight"].toInt(800);
    }
    if (obj.contains("windowX")) {
        m_windowX = obj["windowX"].toInt(-1);
    }
    if (obj.contains("windowY")) {
        m_windowY = obj["windowY"].toInt(-1);
    }
    if (obj.contains("windowMaximized")) {
        m_windowMaximized = obj["windowMaximized"].toBool(false);
    }
    emit windowGeometryChanged();

    if (m_canvas) {
        m_canvas->setActiveTool(m_tool);
        m_canvas->setStrokeWidth(m_penSize);
        m_canvas->setStrokeColor(activeColor());
        m_canvas->setSnapToGrid(m_snapToGrid);
        m_canvas->setGridStyle(m_gridStyle);
        m_canvas->setGridSize(m_gridSize);
    }

    m_loadingSettings = false;
    syncActiveColors();
    updateStatus();
}

void Backend::saveSettings(const QString &customPath) {
    if (m_loadingSettings)
        return;

    QString filePath = customPath.isEmpty() ? settingsFilePath() : customPath;
    QFileInfo fi(filePath);
    QDir().mkpath(fi.absolutePath());

    QJsonObject obj;
    if (m_theme) {
        obj["theme"] = m_theme->id();
    }
    obj["gridStyle"] = m_gridStyle;
    obj["gridSize"] = m_gridSize;
    obj["snapToGrid"] = m_snapToGrid;
    obj["penSize"] = m_penSize;
    obj["activeColorIndex"] = m_activeColorIndex;
    obj["tool"] = m_tool;
    obj["windowWidth"] = m_windowWidth;
    obj["windowHeight"] = m_windowHeight;
    obj["windowX"] = m_windowX;
    obj["windowY"] = m_windowY;
    obj["windowMaximized"] = m_windowMaximized;

    QFile file(filePath);
    if (file.open(QIODevice::WriteOnly)) {
        file.write(QJsonDocument(obj).toJson(QJsonDocument::Indented));
    }
}

} // namespace DrawBoard
