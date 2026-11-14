#include "canvasitem.h"
#include "exporter.h"

#include <QCursor>
#include <QFont>
#include <QPainter>
#include <QtMath>
#include <algorithm>

namespace DrawBoard {

CanvasItem::CanvasItem(QQuickItem *parent)
    : QQuickPaintedItem(parent) {
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setFlag(ItemAcceptsInputMethod, true);
    setFlag(ItemIsFocusScope, true);
    setAntialiasing(true);
}

void CanvasItem::setDocument(Document *doc) {
    if (m_doc) {
        disconnect(m_doc, nullptr, this, nullptr);
    }
    m_doc = doc;
    if (m_doc) {
        connect(m_doc, &Document::documentChanged, this, [this] { update(); });
        connect(m_doc, &Document::selectionChanged, this, [this] { update(); });
    }
    update();
}

void CanvasItem::setHistory(History *history) {
    m_history = history;
}

void CanvasItem::setTheme(Theme *theme) {
    if (m_theme) {
        disconnect(m_theme, nullptr, this, nullptr);
    }
    m_theme = theme;
    if (m_theme) {
        connect(m_theme, &Theme::themeChanged, this, [this] { update(); });
    }
    update();
}

void CanvasItem::setPanX(qreal x) {
    if (!qFuzzyCompare(m_panX, x)) {
        m_panX = x;
        emit panChanged();
        update();
    }
}

void CanvasItem::setPanY(qreal y) {
    if (!qFuzzyCompare(m_panY, y)) {
        m_panY = y;
        emit panChanged();
        update();
    }
}

void CanvasItem::setZoom(qreal z) {
    z = std::clamp(z, 0.05, 20.0);
    if (!qFuzzyCompare(m_zoom, z)) {
        m_zoom = z;
        emit zoomChanged();
        update();
    }
}

void CanvasItem::setGridStyle(int style) {
    GridStyle gs = static_cast<GridStyle>(style);
    if (m_gridStyle != gs) {
        m_gridStyle = gs;
        emit gridStyleChanged();
        update();
    }
}

void CanvasItem::setGridSize(qreal size) {
    if (!qFuzzyCompare(m_gridSize, size) && size > 4.0) {
        m_gridSize = size;
        emit gridSizeChanged();
        update();
    }
}

void CanvasItem::setSnapToGrid(bool snap) {
    if (m_snapToGrid != snap) {
        m_snapToGrid = snap;
        emit snapToGridChanged();
    }
}

QString CanvasItem::activeTool() const {
    switch (m_activeTool) {
    case Tool::Select: return "select";
    case Tool::Pen: return "pen";
    case Tool::Eraser: return "eraser";
    case Tool::Rectangle: return "rectangle";
    case Tool::Ellipse: return "ellipse";
    case Tool::Line: return "line";
    case Tool::Arrow: return "arrow";
    case Tool::Text: return "text";
    case Tool::StickyNote: return "note";
    case Tool::Connector: return "connector";
    case Tool::Hand: return "hand";
    }
    return "select";
}

void CanvasItem::setActiveTool(const QString &toolStr) {
    Tool t = Tool::Select;
    if (toolStr == "pen") t = Tool::Pen;
    else if (toolStr == "eraser") t = Tool::Eraser;
    else if (toolStr == "rectangle") t = Tool::Rectangle;
    else if (toolStr == "ellipse") t = Tool::Ellipse;
    else if (toolStr == "line") t = Tool::Line;
    else if (toolStr == "arrow") t = Tool::Arrow;
    else if (toolStr == "text") t = Tool::Text;
    else if (toolStr == "note") t = Tool::StickyNote;
    else if (toolStr == "connector") t = Tool::Connector;
    else if (toolStr == "hand") t = Tool::Hand;

    if (m_activeTool != t) {
        m_activeTool = t;
        emit activeToolChanged();
        update();
    }
}

void CanvasItem::setStrokeWidth(qreal width) {
    if (!qFuzzyCompare(m_strokeWidth, width)) {
        m_strokeWidth = width;
        emit strokeWidthChanged();
    }
}

void CanvasItem::setStrokeColor(const QColor &color) {
    if (m_strokeColor != color) {
        m_strokeColor = color;
        emit strokeColorChanged();
    }
}

void CanvasItem::setCurrentFillColor(const QColor &color) {
    if (m_currentFillColor != color) {
        m_currentFillColor = color;
        emit currentFillColorChanged();
    }
}

QPointF CanvasItem::screenToWorld(const QPointF &sp) const {
    qreal wx = (sp.x() - width() * 0.5) / m_zoom - m_panX;
    qreal wy = (sp.y() - height() * 0.5) / m_zoom - m_panY;
    return QPointF(wx, wy);
}

QPointF CanvasItem::worldToScreen(const QPointF &wp) const {
    qreal sx = (wp.x() + m_panX) * m_zoom + width() * 0.5;
    qreal sy = (wp.y() + m_panY) * m_zoom + height() * 0.5;
    return QPointF(sx, sy);
}

QRectF CanvasItem::worldRectToScreen(const QRectF &wr) const {
    QPointF tl = worldToScreen(wr.topLeft());
    return QRectF(tl.x(), tl.y(), wr.width() * m_zoom, wr.height() * m_zoom);
}

QPointF CanvasItem::snap(const QPointF &pt) const {
    if (!m_snapToGrid)
        return pt;
    qreal sx = std::round(pt.x() / m_gridSize) * m_gridSize;
    qreal sy = std::round(pt.y() / m_gridSize) * m_gridSize;
    return QPointF(sx, sy);
}

void CanvasItem::resetView() {
    setPanX(0.0);
    setPanY(0.0);
    setZoom(1.0);
}

void CanvasItem::zoomIn() {
    setZoom(m_zoom * 1.25);
}

void CanvasItem::zoomOut() {
    setZoom(m_zoom / 1.25);
}

void CanvasItem::zoomToFit() {
    if (!m_doc || m_doc->isEmpty()) {
        resetView();
        return;
    }

    QRectF bounds = Exporter::calculateExportBounds(*m_doc, 60.0);
    if (bounds.width() < 10 || bounds.height() < 10) {
        resetView();
        return;
    }

    qreal zx = width() / bounds.width();
    qreal zy = height() / bounds.height();
    qreal newZoom = std::clamp(std::min(zx, zy) * 0.9, 0.1, 5.0);

    QPointF center = bounds.center();
    setPanX(-center.x());
    setPanY(-center.y());
    setZoom(newZoom);
}

void CanvasItem::paint(QPainter *painter) {
    painter->setRenderHint(QPainter::Antialiasing, true);
    painter->setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter->setRenderHint(QPainter::TextAntialiasing, true);

    ThemePalette pal = m_theme ? m_theme->current() : ThemePalette();
    if (pal.canvas.isEmpty()) pal.canvas = "#282828";
    if (pal.canvasGrid.isEmpty()) pal.canvasGrid = "#3c3836";

    // 1. Fill background canvas
    painter->fillRect(boundingRect(), QColor(pal.canvas));

    // Visible world rect
    QPointF wTopLeft = screenToWorld(QPointF(0, 0));
    QPointF wBottomRight = screenToWorld(QPointF(width(), height()));
    QRectF visibleWorld(wTopLeft, wBottomRight);

    // 2. Draw Grid
    drawGrid(painter, visibleWorld);

    // 3. Render World Elements with world transformation
    painter->save();
    painter->translate(width() * 0.5, height() * 0.5);
    painter->scale(m_zoom, m_zoom);
    painter->translate(m_panX, m_panY);

    if (m_doc) {
        Exporter::paintElements(*painter, m_doc->elements(), pal);
    }

    // In-progress item drawing (stroke, line, shape preview)
    drawInProgress(painter);

    painter->restore();

    // 4. Viewport Overlays (Selection bounds, handles, rubberband, connector ports)
    drawSelectionOverlay(painter);
}

void CanvasItem::drawGrid(QPainter *painter, const QRectF &vw) {
    if (m_gridStyle == GridStyle::None)
        return;

    ThemePalette pal = m_theme ? m_theme->current() : ThemePalette();
    QColor gridColor = QColor(pal.canvasGrid);

    // Fade grid when zoomed out too far
    qreal alpha = 1.0;
    if (m_zoom < 0.4) {
        alpha = std::clamp((m_zoom - 0.1) / 0.3, 0.0, 1.0);
    }
    if (alpha <= 0.01)
        return;

    gridColor.setAlphaF(alpha * 0.6);
    painter->setPen(QPen(gridColor, 1.0));

    qreal startX = std::floor(vw.left() / m_gridSize) * m_gridSize;
    qreal endX = std::ceil(vw.right() / m_gridSize) * m_gridSize;
    qreal startY = std::floor(vw.top() / m_gridSize) * m_gridSize;
    qreal endY = std::ceil(vw.bottom() / m_gridSize) * m_gridSize;

    if (m_gridStyle == GridStyle::Dots) {
        painter->setBrush(gridColor);
        qreal dotRadius = std::max(1.0, 1.2 * m_zoom);
        for (qreal x = startX; x <= endX; x += m_gridSize) {
            for (qreal y = startY; y <= endY; y += m_gridSize) {
                QPointF sp = worldToScreen(QPointF(x, y));
                painter->drawEllipse(sp, dotRadius, dotRadius);
            }
        }
    } else if (m_gridStyle == GridStyle::Lines) {
        for (qreal x = startX; x <= endX; x += m_gridSize) {
            QPointF p1 = worldToScreen(QPointF(x, vw.top()));
            QPointF p2 = worldToScreen(QPointF(x, vw.bottom()));
            painter->drawLine(p1, p2);
        }
        for (qreal y = startY; y <= endY; y += m_gridSize) {
            QPointF p1 = worldToScreen(QPointF(vw.left(), y));
            QPointF p2 = worldToScreen(QPointF(vw.right(), y));
            painter->drawLine(p1, p2);
        }
    } else if (m_gridStyle == GridStyle::Crosses) {
        qreal arm = 3.0 * m_zoom;
        for (qreal x = startX; x <= endX; x += m_gridSize) {
            for (qreal y = startY; y <= endY; y += m_gridSize) {
                QPointF sp = worldToScreen(QPointF(x, y));
                painter->drawLine(sp.x() - arm, sp.y(), sp.x() + arm, sp.y());
                painter->drawLine(sp.x(), sp.y() - arm, sp.x(), sp.y() + arm);
            }
        }
    }
}

void CanvasItem::drawInProgress(QPainter *painter) {
    QPen pen(m_strokeColor, m_strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QBrush brush(m_currentFillColor);

    if (m_dragMode == DragMode::DrawStroke && !m_currentStrokePoints.isEmpty()) {
        QPainterPath path = Geometry::buildSmoothPath(m_currentStrokePoints);
        painter->setPen(pen);
        painter->setBrush(Qt::NoBrush);
        painter->drawPath(path);
    } else if (m_dragMode == DragMode::DrawShape) {
        painter->setPen(pen);
        painter->setBrush(brush);
        if (m_activeTool == Tool::Rectangle) {
            painter->drawRoundedRect(m_currentShapeBounds, 4.0, 4.0);
        } else if (m_activeTool == Tool::Ellipse) {
            painter->drawEllipse(m_currentShapeBounds);
        } else if (m_activeTool == Tool::Line) {
            painter->drawLine(m_lineStart, m_lineEnd);
        } else if (m_activeTool == Tool::Arrow) {
            painter->drawLine(m_lineStart, m_lineEnd);
            qreal arrowSize = 10.0 + m_strokeWidth * 2.0;
            QPolygonF head = Geometry::makeArrowHead(m_lineEnd, m_lineStart, arrowSize);
            painter->setBrush(m_strokeColor);
            painter->setPen(Qt::NoPen);
            painter->drawPolygon(head);
        }
    } else if (m_dragMode == DragMode::DrawConnector) {
        painter->setPen(pen);
        painter->drawLine(m_lineStart, m_lineEnd);
        qreal arrowSize = 10.0 + m_strokeWidth * 2.0;
        QPolygonF head = Geometry::makeArrowHead(m_lineEnd, m_lineStart, arrowSize);
        painter->setBrush(m_strokeColor);
        painter->setPen(Qt::NoPen);
        painter->drawPolygon(head);
    }
}

void CanvasItem::drawSelectionOverlay(QPainter *painter) {
    ThemePalette pal = m_theme ? m_theme->current() : ThemePalette();
    QColor selColor = QColor(pal.selection.isEmpty() ? "#83a598" : pal.selection);

    // Rubberband selection
    if (m_dragMode == DragMode::RubberbandSelect && !m_rubberbandRect.isEmpty()) {
        QRectF sRect = worldRectToScreen(m_rubberbandRect);
        QColor fillColor = selColor;
        fillColor.setAlphaF(0.12);
        painter->fillRect(sRect, fillColor);
        painter->setPen(QPen(selColor, 1.0, Qt::DashLine));
        painter->drawRect(sRect);
    }

    // Selected elements bounding box and handles
    if (m_doc && !m_doc->selectedIds().isEmpty()) {
        QRectF selWorld = m_doc->selectionBounds();
        if (!selWorld.isEmpty()) {
            QRectF selScreen = worldRectToScreen(selWorld);
            painter->setPen(QPen(selColor, 1.0));
            painter->setBrush(Qt::NoBrush);
            painter->drawRect(selScreen);

            // Draw 8 handles
            HandleType handles[] = {
                HandleType::NW, HandleType::N, HandleType::NE, HandleType::E,
                HandleType::SE, HandleType::S, HandleType::SW, HandleType::W
            };

            qreal handleSize = 8.0;
            for (HandleType h : handles) {
                QPointF hp = Geometry::getHandlePosition(selScreen, h);
                QRectF hr(hp.x() - handleSize * 0.5, hp.y() - handleSize * 0.5, handleSize, handleSize);
                painter->fillRect(hr, QColor(pal.surface.isEmpty() ? "#32302f" : pal.surface));
                painter->setPen(QPen(selColor, 1.5));
                painter->drawRect(hr);
            }
        }
    }

    // Highlight cardinal connection points if connector tool is active
    if (m_activeTool == Tool::Connector && m_doc) {
        painter->setBrush(selColor);
        painter->setPen(QPen(Qt::white, 1.0));
        for (const auto &el : m_doc->elements()) {
            if (el.type == ElementType::Rectangle || el.type == ElementType::Ellipse ||
                el.type == ElementType::StickyNote || el.type == ElementType::Text) {
                CardinalPoint cards[] = {
                    CardinalPoint::Top, CardinalPoint::Right,
                    CardinalPoint::Bottom, CardinalPoint::Left
                };
                for (CardinalPoint cp : cards) {
                    QPointF wp = Geometry::getCardinalPoint(el, cp);
                    QPointF sp = worldToScreen(wp);
                    painter->drawEllipse(sp, 4.0, 4.0);
                }
            }
        }
    }
}

void CanvasItem::mousePressEvent(QMouseEvent *event) {
    forceActiveFocus();
    m_lastMousePos = event->position();
    QPointF worldPos = screenToWorld(event->position());
    m_dragStartWorld = worldPos;

    // Pan via middle button or Space+Left
    if (event->button() == Qt::MiddleButton || (m_spaceDown && event->button() == Qt::LeftButton) ||
        m_activeTool == Tool::Hand) {
        m_dragMode = DragMode::Pan;
        m_isPanning = true;
        emit isPanningChanged();
        event->accept();
        return;
    }

    if (event->button() != Qt::LeftButton)
        return;

    switch (m_activeTool) {
    case Tool::Pen: {
        m_dragMode = DragMode::DrawStroke;
        m_currentStrokePoints.clear();
        StrokePoint pt;
        pt.x = worldPos.x();
        pt.y = worldPos.y();
        pt.pressure = 0.5;
        pt.time = event->timestamp();
        m_currentStrokePoints.append(pt);
        update();
        break;
    }

    case Tool::Eraser: {
        m_dragMode = DragMode::None;
        if (m_doc && m_history) {
            auto elems = m_doc->elements();
            for (const auto &el : elems) {
                if (Geometry::hitTest(el, worldPos, 8.0)) {
                    m_history->pushState("Erase", m_doc->elements(), m_doc->selectedIds());
                    m_doc->removeElement(el.id);
                    break;
                }
            }
        }
        break;
    }

    case Tool::Rectangle:
    case Tool::Ellipse: {
        m_dragMode = DragMode::DrawShape;
        QPointF snapped = snap(worldPos);
        m_lineStart = snapped;
        m_currentShapeBounds = QRectF(snapped, QSizeF(0, 0));
        update();
        break;
    }

    case Tool::Line:
    case Tool::Arrow: {
        m_dragMode = DragMode::DrawShape;
        QPointF snapped = snap(worldPos);
        m_lineStart = snapped;
        m_lineEnd = snapped;
        update();
        break;
    }

    case Tool::Connector: {
        m_dragMode = DragMode::DrawConnector;
        m_connectorSourceId.clear();
        m_connectorSourceCardinal = CardinalPoint::None;
        m_lineStart = worldPos;
        m_lineEnd = worldPos;

        if (m_doc) {
            for (const auto &el : m_doc->elements()) {
                CardinalPoint cp = Geometry::findNearestCardinal(el, worldPos, 20.0);
                if (cp != CardinalPoint::None) {
                    m_connectorSourceId = el.id;
                    m_connectorSourceCardinal = cp;
                    m_lineStart = Geometry::getCardinalPoint(el, cp);
                    break;
                }
            }
        }
        update();
        break;
    }

    case Tool::Text: {
        if (m_doc && m_history) {
            m_history->pushState("Add Text", m_doc->elements(), m_doc->selectedIds());
            QPointF snapped = snap(worldPos);
            Element el;
            el.type = ElementType::Text;
            el.bounds = QRectF(snapped.x(), snapped.y(), 160, 36);
            el.strokeColor = m_strokeColor;
            el.text = "Type text here...";
            el.fontSize = 16.0;
            QString newId = m_doc->addElement(el);
            m_doc->select(newId);
            emit editTextRequested(newId, el.text, worldRectToScreen(el.bounds));
            setActiveTool("select");
        }
        break;
    }

    case Tool::StickyNote: {
        if (m_doc && m_history) {
            m_history->pushState("Add Note", m_doc->elements(), m_doc->selectedIds());
            QPointF snapped = snap(worldPos);
            Element el;
            el.type = ElementType::StickyNote;
            el.bounds = QRectF(snapped.x(), snapped.y(), 180, 140);
            el.strokeColor = m_strokeColor;
            el.fillColor = m_currentFillColor;
            el.text = "Note...";
            el.fontSize = 15.0;
            QString newId = m_doc->addElement(el);
            m_doc->select(newId);
            emit editTextRequested(newId, el.text, worldRectToScreen(el.bounds));
            setActiveTool("select");
        }
        break;
    }

    case Tool::Select: {
        if (!m_doc) break;

        // 1. Check handles if selection exists
        if (!m_doc->selectedIds().isEmpty()) {
            QRectF selBounds = m_doc->selectionBounds();
            QRectF selScreen = worldRectToScreen(selBounds);
            HandleType h = Geometry::hitTestHandles(selScreen, event->position());
            if (h != HandleType::None) {
                m_dragMode = DragMode::ResizeElements;
                m_activeHandle = h;
                if (m_history) m_history->pushState("Resize", m_doc->elements(), m_doc->selectedIds());
                event->accept();
                return;
            }
        }

        // 2. Check hit test against elements
        QString hitId;
        const auto &elems = m_doc->elements();
        for (int i = elems.size() - 1; i >= 0; --i) {
            if (Geometry::hitTest(elems[i], worldPos, 6.0)) {
                hitId = elems[i].id;
                break;
            }
        }

        bool shiftHeld = event->modifiers() & Qt::ShiftModifier;

        if (!hitId.isEmpty()) {
            if (m_doc->isSelected(hitId)) {
                if (shiftHeld) {
                    // Toggle deselect
                    auto sel = m_doc->selectedIds();
                    sel.remove(hitId);
                    m_doc->setSelection(sel);
                } else {
                    m_dragMode = DragMode::MoveElements;
                    if (m_history) m_history->pushState("Move", m_doc->elements(), m_doc->selectedIds());
                }
            } else {
                m_doc->select(hitId, shiftHeld);
                m_dragMode = DragMode::MoveElements;
                if (m_history) m_history->pushState("Move", m_doc->elements(), m_doc->selectedIds());
            }
        } else {
            // Clicked in empty space -> rubberband selection
            if (!shiftHeld) m_doc->clearSelection();
            m_dragMode = DragMode::RubberbandSelect;
            m_rubberbandRect = QRectF(worldPos, QSizeF(0, 0));
        }
        update();
        break;
    }

    default:
        break;
    }

    event->accept();
}

void CanvasItem::mouseMoveEvent(QMouseEvent *event) {
    QPointF deltaScreen = event->position() - m_lastMousePos;
    m_lastMousePos = event->position();
    QPointF worldPos = screenToWorld(event->position());
    emit pointerMoved(worldPos.x(), worldPos.y());

    if (m_dragMode == DragMode::Pan) {
        setPanX(m_panX + deltaScreen.x() / m_zoom);
        setPanY(m_panY + deltaScreen.y() / m_zoom);
        event->accept();
        return;
    }

    switch (m_dragMode) {
    case DragMode::DrawStroke: {
        StrokePoint pt;
        pt.x = worldPos.x();
        pt.y = worldPos.y();
        pt.pressure = 0.5;
        pt.time = event->timestamp();
        m_currentStrokePoints.append(pt);
        update();
        break;
    }

    case DragMode::DrawShape: {
        QPointF snapped = snap(worldPos);
        m_lineEnd = snapped;

        qreal left = std::min(m_lineStart.x(), snapped.x());
        qreal top = std::min(m_lineStart.y(), snapped.y());
        qreal w = std::abs(snapped.x() - m_lineStart.x());
        qreal h = std::abs(snapped.y() - m_lineStart.y());

        if (event->modifiers() & Qt::ShiftModifier) {
            qreal side = std::max(w, h);
            w = side;
            h = side;
        }

        m_currentShapeBounds = QRectF(left, top, w, h);
        update();
        break;
    }

    case DragMode::DrawConnector: {
        m_lineEnd = worldPos;
        m_connectorTargetId.clear();
        m_connectorTargetCardinal = CardinalPoint::None;

        if (m_doc) {
            for (const auto &el : m_doc->elements()) {
                if (el.id == m_connectorSourceId)
                    continue;
                CardinalPoint cp = Geometry::findNearestCardinal(el, worldPos, 25.0);
                if (cp != CardinalPoint::None) {
                    m_connectorTargetId = el.id;
                    m_connectorTargetCardinal = cp;
                    m_lineEnd = Geometry::getCardinalPoint(el, cp);
                    break;
                }
            }
        }
        update();
        break;
    }

    case DragMode::MoveElements: {
        QPointF worldDelta = deltaScreen / m_zoom;
        if (m_doc) {
            m_doc->moveSelected(worldDelta);
        }
        break;
    }

    case DragMode::ResizeElements: {
        QPointF worldDelta = deltaScreen / m_zoom;
        bool preserveAspect = event->modifiers() & Qt::ShiftModifier;
        if (m_doc) {
            m_doc->resizeSelected(m_activeHandle, worldDelta, preserveAspect);
        }
        break;
    }

    case DragMode::RubberbandSelect: {
        qreal left = std::min(m_dragStartWorld.x(), worldPos.x());
        qreal top = std::min(m_dragStartWorld.y(), worldPos.y());
        qreal w = std::abs(worldPos.x() - m_dragStartWorld.x());
        qreal h = std::abs(worldPos.y() - m_dragStartWorld.y());
        m_rubberbandRect = QRectF(left, top, w, h);

        if (m_doc) {
            QSet<QString> selected;
            for (const auto &el : m_doc->elements()) {
                if (Geometry::rectIntersectsElement(m_rubberbandRect, el))
                    selected.insert(el.id);
            }
            m_doc->setSelection(selected);
        }
        update();
        break;
    }

    default:
        break;
    }

    event->accept();
}

void CanvasItem::mouseReleaseEvent(QMouseEvent *event) {
    Q_UNUSED(event)

    if (m_dragMode == DragMode::Pan) {
        m_dragMode = DragMode::None;
        m_isPanning = false;
        emit isPanningChanged();
        event->accept();
        return;
    }

    switch (m_dragMode) {
    case DragMode::DrawStroke: {
        if (m_currentStrokePoints.size() >= 1 && m_doc && m_history) {
            m_history->pushState("Pen Stroke", m_doc->elements(), m_doc->selectedIds());
            Element elem;
            elem.type = ElementType::Stroke;
            elem.points = Geometry::smoothPoints(m_currentStrokePoints);
            elem.strokeColor = m_strokeColor;
            elem.strokeWidth = m_strokeWidth;
            m_doc->addElement(elem);
        }
        m_currentStrokePoints.clear();
        break;
    }

    case DragMode::DrawShape: {
        if (m_doc && m_history) {
            m_history->pushState("Add Shape", m_doc->elements(), m_doc->selectedIds());
            Element elem;
            elem.strokeColor = m_strokeColor;
            elem.strokeWidth = m_strokeWidth;
            elem.fillColor = m_currentFillColor;

            if (m_activeTool == Tool::Rectangle && m_currentShapeBounds.width() > 3 && m_currentShapeBounds.height() > 3) {
                elem.type = ElementType::Rectangle;
                elem.bounds = m_currentShapeBounds;
                elem.cornerRadius = 4.0;
                m_doc->addElement(elem);
            } else if (m_activeTool == Tool::Ellipse && m_currentShapeBounds.width() > 3 && m_currentShapeBounds.height() > 3) {
                elem.type = ElementType::Ellipse;
                elem.bounds = m_currentShapeBounds;
                m_doc->addElement(elem);
            } else if (m_activeTool == Tool::Line && Geometry::distance(m_lineStart, m_lineEnd) > 3) {
                elem.type = ElementType::Line;
                elem.startPoint = m_lineStart;
                elem.endPoint = m_lineEnd;
                qreal minX = std::min(m_lineStart.x(), m_lineEnd.x());
                qreal maxX = std::max(m_lineStart.x(), m_lineEnd.x());
                qreal minY = std::min(m_lineStart.y(), m_lineEnd.y());
                qreal maxY = std::max(m_lineStart.y(), m_lineEnd.y());
                elem.bounds = QRectF(minX - 4, minY - 4, (maxX - minX) + 8, (maxY - minY) + 8);
                m_doc->addElement(elem);
            } else if (m_activeTool == Tool::Arrow && Geometry::distance(m_lineStart, m_lineEnd) > 3) {
                elem.type = ElementType::Arrow;
                elem.startPoint = m_lineStart;
                elem.endPoint = m_lineEnd;
                elem.arrowEnd = true;
                qreal minX = std::min(m_lineStart.x(), m_lineEnd.x());
                qreal maxX = std::max(m_lineStart.x(), m_lineEnd.x());
                qreal minY = std::min(m_lineStart.y(), m_lineEnd.y());
                qreal maxY = std::max(m_lineStart.y(), m_lineEnd.y());
                elem.bounds = QRectF(minX - 8, minY - 8, (maxX - minX) + 16, (maxY - minY) + 16);
                m_doc->addElement(elem);
            }
        }
        m_currentShapeBounds = QRectF();
        break;
    }

    case DragMode::DrawConnector: {
        if (m_doc && m_history && Geometry::distance(m_lineStart, m_lineEnd) > 5) {
            m_history->pushState("Add Connector", m_doc->elements(), m_doc->selectedIds());
            Element elem;
            elem.type = ElementType::Connector;
            elem.strokeColor = m_strokeColor;
            elem.strokeWidth = m_strokeWidth;
            elem.startPoint = m_lineStart;
            elem.endPoint = m_lineEnd;
            elem.fromId = m_connectorSourceId;
            elem.fromCardinal = m_connectorSourceCardinal;
            elem.toId = m_connectorTargetId;
            elem.toCardinal = m_connectorTargetCardinal;
            elem.arrowEnd = true;
            qreal minX = std::min(m_lineStart.x(), m_lineEnd.x());
            qreal maxX = std::max(m_lineStart.x(), m_lineEnd.x());
            qreal minY = std::min(m_lineStart.y(), m_lineEnd.y());
            qreal maxY = std::max(m_lineStart.y(), m_lineEnd.y());
            elem.bounds = QRectF(minX - 10, minY - 10, (maxX - minX) + 20, (maxY - minY) + 20);
            m_doc->addElement(elem);
        }
        m_connectorSourceId.clear();
        m_connectorTargetId.clear();
        break;
    }

    case DragMode::RubberbandSelect: {
        m_rubberbandRect = QRectF();
        break;
    }

    default:
        break;
    }

    m_dragMode = DragMode::None;
    m_activeHandle = HandleType::None;
    update();
    event->accept();
}

void CanvasItem::mouseDoubleClickEvent(QMouseEvent *event) {
    if (!m_doc) return;
    QPointF worldPos = screenToWorld(event->position());

    const auto &elems = m_doc->elements();
    for (int i = elems.size() - 1; i >= 0; --i) {
        const auto &el = elems[i];
        if ((el.type == ElementType::Text || el.type == ElementType::StickyNote) &&
            Geometry::hitTest(el, worldPos, 6.0)) {
            emit editTextRequested(el.id, el.text, worldRectToScreen(el.bounds));
            event->accept();
            return;
        }
    }
}

void CanvasItem::wheelEvent(QWheelEvent *event) {
    QPointF numDegrees = event->angleDelta() / 8.0;

    if (event->modifiers() & Qt::ControlModifier) {
        // Zoom centered at mouse position
        qreal factor = std::pow(1.0015, numDegrees.y() * 8.0);
        qreal oldZoom = m_zoom;
        qreal newZoom = std::clamp(oldZoom * factor, 0.05, 20.0);

        if (!qFuzzyCompare(oldZoom, newZoom)) {
            QPointF mouseScreen = event->position();
            QPointF mouseWorld = screenToWorld(mouseScreen);

            setZoom(newZoom);

            // Re-anchor pan
            qreal newPanX = (mouseScreen.x() - width() * 0.5) / newZoom - mouseWorld.x();
            qreal newPanY = (mouseScreen.y() - height() * 0.5) / newZoom - mouseWorld.y();
            setPanX(newPanX);
            setPanY(newPanY);
        }
    } else {
        // Smooth pan
        setPanX(m_panX + numDegrees.x() / m_zoom);
        setPanY(m_panY + numDegrees.y() / m_zoom);
    }

    event->accept();
}

void CanvasItem::keyPressEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = true;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }
    QQuickPaintedItem::keyPressEvent(event);
}

void CanvasItem::keyReleaseEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = false;
        unsetCursor();
        event->accept();
        return;
    }
    QQuickPaintedItem::keyReleaseEvent(event);
}

void CanvasItem::hoverMoveEvent(QHoverEvent *event) {
    QPointF worldPos = screenToWorld(event->position());
    emit pointerMoved(worldPos.x(), worldPos.y());

    if (m_activeTool == Tool::Select && m_doc && !m_doc->selectedIds().isEmpty()) {
        QRectF selScreen = worldRectToScreen(m_doc->selectionBounds());
        HandleType h = Geometry::hitTestHandles(selScreen, event->position());
        switch (h) {
        case HandleType::NW:
        case HandleType::SE:
            setCursor(Qt::SizeFDiagCursor);
            return;
        case HandleType::NE:
        case HandleType::SW:
            setCursor(Qt::SizeBDiagCursor);
            return;
        case HandleType::N:
        case HandleType::S:
            setCursor(Qt::SizeVerCursor);
            return;
        case HandleType::E:
        case HandleType::W:
            setCursor(Qt::SizeHorCursor);
            return;
        default:
            unsetCursor();
            break;
        }
    } else {
        unsetCursor();
    }
}

} // namespace DrawBoard
