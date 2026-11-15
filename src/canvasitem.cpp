#include "canvasitem.h"
#include "backend.h"
#include "exporter.h"

#include <QCursor>
#include <QFont>
#include <QMimeData>
#include <QPainter>
#include <QtMath>
#include <algorithm>

namespace DrawBoard {

static inline qreal eventPressure(const QMouseEvent *event) {
    if (event && !event->points().isEmpty()) {
        qreal p = event->points().first().pressure();
        if (p > 0.0 && !std::isnan(p)) {
            return p;
        }
    }
    return 0.5;
}

CanvasItem::CanvasItem(QQuickItem *parent)
    : QQuickPaintedItem(parent) {
    setAcceptedMouseButtons(Qt::AllButtons);
    setAcceptHoverEvents(true);
    setFlag(ItemAcceptsDrops, true);
    setFlag(ItemAcceptsInputMethod, true);
    setFlag(ItemIsFocusScope, true);
    setAntialiasing(true);

    m_holdToStraightenTimer.setSingleShot(true);
    connect(&m_holdToStraightenTimer, &QTimer::timeout, this, &CanvasItem::checkHoldToStraighten);

    connect(&m_laserTimer, &QTimer::timeout, this, [this] {
        qint64 now = QDateTime::currentMSecsSinceEpoch();
        bool anyRemaining = false;
        for (int i = m_laserStrokes.size() - 1; i >= 0; --i) {
            if (m_laserStrokes[i].finishedTime > 0) {
                qint64 age = now - m_laserStrokes[i].finishedTime;
                if (age > 2000) {
                    m_laserStrokes.removeAt(i);
                } else {
                    anyRemaining = true;
                }
            } else {
                anyRemaining = true;
            }
        }
        if (!anyRemaining && m_laserStrokes.isEmpty()) {
            m_laserTimer.stop();
        }
        update();
    });
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
    case Tool::Highlighter: return "highlighter";
    case Tool::Eraser: return "eraser";
    case Tool::Rectangle: return "rectangle";
    case Tool::Ellipse: return "ellipse";
    case Tool::Line: return "line";
    case Tool::Arrow: return "arrow";
    case Tool::Text: return "text";
    case Tool::StickyNote: return "note";
    case Tool::Connector: return "connector";
    case Tool::Hand: return "hand";
    case Tool::Laser: return "laser";
    }
    return "select";
}

void CanvasItem::setActiveTool(const QString &toolStr) {
    Tool t = Tool::Select;
    if (toolStr == "pen") t = Tool::Pen;
    else if (toolStr == "highlighter") t = Tool::Highlighter;
    else if (toolStr == "eraser") t = Tool::Eraser;
    else if (toolStr == "rectangle") t = Tool::Rectangle;
    else if (toolStr == "ellipse") t = Tool::Ellipse;
    else if (toolStr == "line") t = Tool::Line;
    else if (toolStr == "arrow") t = Tool::Arrow;
    else if (toolStr == "text") t = Tool::Text;
    else if (toolStr == "note") t = Tool::StickyNote;
    else if (toolStr == "connector") t = Tool::Connector;
    else if (toolStr == "hand") t = Tool::Hand;
    else if (toolStr == "laser") t = Tool::Laser;

    if (m_activeTool != t) {
        if (m_dragMode == DragMode::Erase) {
            m_dragMode = DragMode::None;
            m_eraserTrail.clear();
            m_markedForDeletion.clear();
        }
        if (m_dragMode == DragMode::Laser) {
            m_dragMode = DragMode::None;
        }
        if (t != Tool::Laser) {
            m_laserStrokes.clear();
            m_laserTimer.stop();
        }
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
        // Viewport culling bounds (visible world area + 60px padding) to maximize render FPS
        qreal minX = std::min(wTopLeft.x(), wBottomRight.x());
        qreal maxX = std::max(wTopLeft.x(), wBottomRight.x());
        qreal minY = std::min(wTopLeft.y(), wBottomRight.y());
        qreal maxY = std::max(wTopLeft.y(), wBottomRight.y());
        QRectF visibleWorldBounds(minX - 60, minY - 60, (maxX - minX) + 120, (maxY - minY) + 120);

        if (m_markedForDeletion.isEmpty()) {
            for (const auto &el : m_doc->elements()) {
                if (!el.bounds.isEmpty() && !visibleWorldBounds.intersects(el.bounds))
                    continue;
                Exporter::paintElement(*painter, el, pal);
            }
        } else {
            for (const auto &el : m_doc->elements()) {
                if (!el.bounds.isEmpty() && !visibleWorldBounds.intersects(el.bounds))
                    continue;
                if (m_markedForDeletion.contains(el.id)) {
                    painter->save();
                    painter->setOpacity(0.25);
                    Exporter::paintElement(*painter, el, pal);
                    painter->restore();
                } else {
                    Exporter::paintElement(*painter, el, pal);
                }
            }
        }
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

    qreal startX = std::floor(vw.left() / m_gridSize) * m_gridSize;
    qreal endX = std::ceil(vw.right() / m_gridSize) * m_gridSize;
    qreal startY = std::floor(vw.top() / m_gridSize) * m_gridSize;
    qreal endY = std::ceil(vw.bottom() / m_gridSize) * m_gridSize;

    if (m_gridStyle == GridStyle::Dots) {
        qreal dotDiameter = std::max(2.0, 2.4 * m_zoom);
        painter->setPen(QPen(gridColor, dotDiameter, Qt::SolidLine, Qt::RoundCap));
        int countX = std::max(1, static_cast<int>((endX - startX) / m_gridSize) + 1);
        int countY = std::max(1, static_cast<int>((endY - startY) / m_gridSize) + 1);
        QVector<QPointF> dotPoints;
        dotPoints.reserve(countX * countY);

        for (qreal x = startX; x <= endX; x += m_gridSize) {
            for (qreal y = startY; y <= endY; y += m_gridSize) {
                dotPoints.append(worldToScreen(QPointF(x, y)));
            }
        }
        if (!dotPoints.isEmpty()) {
            painter->drawPoints(dotPoints.data(), dotPoints.size());
        }
    } else if (m_gridStyle == GridStyle::Lines) {
        painter->setPen(QPen(gridColor, 1.0));
        int countX = std::max(1, static_cast<int>((endX - startX) / m_gridSize) + 1);
        int countY = std::max(1, static_cast<int>((endY - startY) / m_gridSize) + 1);
        QVector<QLineF> lines;
        lines.reserve(countX + countY);

        for (qreal x = startX; x <= endX; x += m_gridSize) {
            QPointF p1 = worldToScreen(QPointF(x, vw.top()));
            QPointF p2 = worldToScreen(QPointF(x, vw.bottom()));
            lines.append(QLineF(p1, p2));
        }
        for (qreal y = startY; y <= endY; y += m_gridSize) {
            QPointF p1 = worldToScreen(QPointF(vw.left(), y));
            QPointF p2 = worldToScreen(QPointF(vw.right(), y));
            lines.append(QLineF(p1, p2));
        }
        if (!lines.isEmpty()) {
            painter->drawLines(lines.data(), lines.size());
        }
    } else if (m_gridStyle == GridStyle::Crosses) {
        painter->setPen(QPen(gridColor, 1.0));
        qreal arm = 3.0 * m_zoom;
        int countX = std::max(1, static_cast<int>((endX - startX) / m_gridSize) + 1);
        int countY = std::max(1, static_cast<int>((endY - startY) / m_gridSize) + 1);
        QVector<QLineF> lines;
        lines.reserve(countX * countY * 2);

        for (qreal x = startX; x <= endX; x += m_gridSize) {
            for (qreal y = startY; y <= endY; y += m_gridSize) {
                QPointF sp = worldToScreen(QPointF(x, y));
                lines.append(QLineF(sp.x() - arm, sp.y(), sp.x() + arm, sp.y()));
                lines.append(QLineF(sp.x(), sp.y() - arm, sp.x(), sp.y() + arm));
            }
        }
        if (!lines.isEmpty()) {
            painter->drawLines(lines.data(), lines.size());
        }
    }
}

void CanvasItem::checkHoldToStraighten() {
    if (m_dragMode != DragMode::DrawStroke || m_currentStrokePoints.size() < 3)
        return;
    if (m_activeTool != Tool::Pen && m_activeTool != Tool::Highlighter)
        return;

    QPointF pStart = m_currentStrokePoints.first().toPointF();
    QPointF pEnd = m_currentStrokePoints.last().toPointF();
    qreal len = Geometry::distance(pStart, pEnd);
    if (len < 20.0)
        return;

    // Check maximum deviation from segment pStart -> pEnd
    qreal maxDev = 0.0;
    for (int i = 1; i < m_currentStrokePoints.size() - 1; ++i) {
        qreal dev = Geometry::distanceToSegment(m_currentStrokePoints[i].toPointF(), pStart, pEnd);
        if (dev > maxDev)
            maxDev = dev;
    }

    qreal maxAllowedDeviation = std::max(20.0, len * 0.28);
    if (maxDev <= maxAllowedDeviation) {
        m_isStraightened = true;
        StrokePoint spStart = m_currentStrokePoints.first();
        StrokePoint spEnd = m_currentStrokePoints.last();
        m_currentStrokePoints.clear();
        m_currentStrokePoints.append(spStart);
        m_currentStrokePoints.append(spEnd);
        update();
    }
}

void CanvasItem::drawInProgress(QPainter *painter) {
    QPen pen(m_strokeColor, m_strokeWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
    QBrush brush(m_currentFillColor);

    if (m_dragMode == DragMode::DrawStroke && !m_currentStrokePoints.isEmpty()) {
        auto livePoints = (m_currentStrokePoints.size() > 3 && !m_isStraightened)
                              ? Geometry::smoothPoints(m_currentStrokePoints)
                              : m_currentStrokePoints;
        QPainterPath path = Geometry::buildSmoothPath(livePoints);
        if (m_activeTool == Tool::Highlighter) {
            QColor hlColor = m_strokeColor.isValid() ? m_strokeColor : QColor("#fabd2f");
            hlColor.setAlphaF(0.35);
            qreal hlWidth = std::max(16.0, m_strokeWidth * 5.0);
            QPen hlPen(hlColor, hlWidth, Qt::SolidLine, Qt::FlatCap, Qt::RoundJoin);
            painter->setPen(hlPen);
        } else {
            painter->setPen(pen);
        }
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

    // Minimal laser pointer (stays on screen while drawing, fades after release)
    QColor baseLaserColor = m_strokeColor.isValid() ? m_strokeColor : QColor("#ff8787");
    qreal laserWidth = std::max(2.5, m_strokeWidth);

    qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const auto &stroke : m_laserStrokes) {
        if (stroke.points.isEmpty())
            continue;

        qreal alpha = 1.0;
        if (stroke.finishedTime > 0) {
            qint64 age = now - stroke.finishedTime;
            if (age < 1200) {
                alpha = 1.0; // Stays fully on screen
            } else if (age < 2000) {
                alpha = std::clamp(1.0 - (age - 1200.0) / 800.0, 0.0, 1.0); // smooth fade
            } else {
                continue;
            }
        }

        QColor strokeColor = baseLaserColor;
        strokeColor.setAlphaF(alpha);

        if (stroke.points.size() == 1) {
            painter->setPen(Qt::NoPen);
            painter->setBrush(strokeColor);
            qreal r = laserWidth * 0.8;
            painter->drawEllipse(stroke.points[0], r, r);
        } else {
            QPen pen(strokeColor, laserWidth, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin);
            painter->setPen(pen);
            painter->setBrush(Qt::NoBrush);

            QPainterPath laserPath;
            laserPath.moveTo(stroke.points[0]);
            if (stroke.points.size() == 2) {
                laserPath.lineTo(stroke.points[1]);
            } else {
                for (int i = 1; i < stroke.points.size() - 1; ++i) {
                    QPointF curr = stroke.points[i];
                    QPointF next = stroke.points[i + 1];
                    QPointF mid = (curr + next) * 0.5;
                    laserPath.quadTo(curr, mid);
                }
                laserPath.lineTo(stroke.points.last());
            }
            painter->drawPath(laserPath);

            // If actively drawing, draw active tip dot
            if (stroke.finishedTime == 0) {
                painter->setPen(Qt::NoPen);
                painter->setBrush(strokeColor);
                qreal r = laserWidth * 0.8;
                painter->drawEllipse(stroke.points.last(), r, r);
            }
        }
    }

    if (m_activeTool == Tool::Laser && m_dragMode != DragMode::Laser && !m_lastHoverWorld.isNull()) {
        // Simple minimal pointer dot at hover cursor
        painter->setPen(Qt::NoPen);
        painter->setBrush(baseLaserColor);
        qreal tipR = laserWidth * 0.8;
        painter->drawEllipse(m_lastHoverWorld, tipR, tipR);
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
    if (m_doc && !m_doc->selectedIds().isEmpty() && m_dragMode != DragMode::Erase) {
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
                el.type == ElementType::StickyNote || el.type == ElementType::Text ||
                el.type == ElementType::Image) {
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
    case Tool::Pen:
    case Tool::Highlighter: {
        m_dragMode = DragMode::DrawStroke;
        m_isStraightened = false;
        m_currentStrokePoints.clear();
        StrokePoint pt;
        pt.x = worldPos.x();
        pt.y = worldPos.y();
        pt.pressure = eventPressure(event);
        pt.time = event->timestamp();
        m_currentStrokePoints.append(pt);
        m_holdToStraightenTimer.start(350);
        update();
        break;
    }

    case Tool::Eraser: {
        m_dragMode = DragMode::Erase;
        m_eraserTrail.clear();
        m_eraserTrail.append(worldPos);
        m_markedForDeletion.clear();

        qreal hitTol = 12.0 / m_zoom;
        if (m_doc) {
            const auto &elems = m_doc->elements();
            for (int i = elems.size() - 1; i >= 0; --i) {
                if (Geometry::hitTest(elems[i], worldPos, hitTol, false)) {
                    m_markedForDeletion.insert(elems[i].id);
                    break;
                }
            }
        }
        update();
        break;
    }

    case Tool::Laser: {
        m_dragMode = DragMode::Laser;
        LaserStroke stroke;
        stroke.points.append(worldPos);
        stroke.finishedTime = 0;
        m_laserStrokes.append(stroke);
        if (!m_laserTimer.isActive()) {
            m_laserTimer.start(16);
        }
        update();
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
        QPointF snapped = snap(worldPos);
        QPointF sPos = worldToScreen(snapped);
        emit createTextRequested(snapped.x(), snapped.y(), sPos.x(), sPos.y(), 16.0, m_strokeColor.name());
        setActiveTool("select");
        break;
    }

    case Tool::StickyNote: {
        if (m_doc) {
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
            if (m_history) {
                m_history->pushState("Add Note", m_doc->elements(), m_doc->selectedIds());
            }
            QPointF sPos = worldToScreen(el.bounds.topLeft() + QPointF(12.0, 12.0));
            emit editTextRequested(newId, el.text, sPos.x(), sPos.y(), el.fontSize,
                                  m_theme ? m_theme->current().noteText : "#ebdbb2");
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
                m_dragInitialElements = m_doc->elements();
                m_dragInitialSelectionBounds = selBounds;
                event->accept();
                return;
            }
        }

        // 2. Check hit test against elements with screen-scaled tolerance
        qreal hitTol = std::max(6.0, 10.0 / m_zoom);
        QString hitId;
        const auto &elems = m_doc->elements();
        for (int i = elems.size() - 1; i >= 0; --i) {
            if (Geometry::hitTest(elems[i], worldPos, hitTol)) {
                hitId = elems[i].id;
                break;
            }
        }

        bool shiftHeld = event->modifiers() & Qt::ShiftModifier;
        bool altHeld = event->modifiers() & Qt::AltModifier;
        m_clickedElementId = hitId;

        // 3. Selection movement: If clicked on ANY point inside the selected area OR on a selected element
        bool clickedSelectedArea = (!m_doc->selectedIds().isEmpty() &&
                                    (m_doc->selectionBounds().adjusted(-hitTol, -hitTol, hitTol, hitTol).contains(worldPos) ||
                                     (!hitId.isEmpty() && m_doc->isSelected(hitId))));

        if (clickedSelectedArea) {
            if (shiftHeld && !hitId.isEmpty()) {
                // Shift+Click on already selected element: toggle deselect
                auto sel = m_doc->selectedIds();
                sel.remove(hitId);
                m_doc->setSelection(sel);
            } else {
                if (altHeld) {
                    m_doc->duplicateSelected(QPointF(0, 0));
                }
                m_dragMode = DragMode::MoveElements;
                m_dragInitialElements = m_doc->elements();
                m_dragInitialSelectionBounds = m_doc->selectionBounds();
            }
        } else if (!hitId.isEmpty()) {
            // Clicked on an unselected element
            m_doc->select(hitId, shiftHeld);
            if (altHeld) {
                m_doc->duplicateSelected(QPointF(0, 0));
            }
            m_dragMode = DragMode::MoveElements;
            m_dragInitialElements = m_doc->elements();
            m_dragInitialSelectionBounds = m_doc->selectionBounds();
        } else {
            // Clicked in empty canvas space outside selection -> clear and start marquee selection
            if (!shiftHeld) m_doc->clearSelection();
            m_dragMode = DragMode::RubberbandSelect;
            m_dragStartSelection = shiftHeld ? m_doc->selectedIds() : QSet<QString>();
            m_rubberbandRect = QRectF(worldPos, QSizeF(0, 0));
        }
        update();
        break;
    }

    default:
        break;
    }

    bool isNowDrawing = (m_dragMode == DragMode::DrawStroke || m_dragMode == DragMode::DrawShape ||
                         m_dragMode == DragMode::DrawConnector || m_dragMode == DragMode::Erase ||
                         m_dragMode == DragMode::Laser);
    if (m_isDrawing != isNowDrawing) {
        m_isDrawing = isNowDrawing;
        emit isDrawingChanged();
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
        if (m_isStraightened) {
            StrokePoint pt;
            pt.x = worldPos.x();
            pt.y = worldPos.y();
            pt.pressure = eventPressure(event);
            pt.time = event->timestamp();
            if (m_currentStrokePoints.size() >= 2) {
                m_currentStrokePoints[1] = pt;
            } else {
                m_currentStrokePoints.append(pt);
            }
            update();
        } else {
            // Distance threshold: skip micro-jitter samples closer than 1.2 screen pixels
            qreal minDist = 1.2 / m_zoom;
            if (!m_currentStrokePoints.isEmpty()) {
                qreal dist = Geometry::distance(m_currentStrokePoints.last().toPointF(), worldPos);
                if (dist < minDist) {
                    break;
                }
            }

            StrokePoint pt;
            pt.x = worldPos.x();
            pt.y = worldPos.y();
            pt.pressure = eventPressure(event);
            pt.time = event->timestamp();
            m_currentStrokePoints.append(pt);
            m_holdToStraightenTimer.start(350);
            update();
        }
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

    case DragMode::Erase: {
        m_eraserTrail.append(worldPos);
        if (m_eraserTrail.size() > 36)
            m_eraserTrail.removeFirst();

        QPointF prevWorld = screenToWorld(event->position() - deltaScreen);
        qreal dist = Geometry::distance(prevWorld, worldPos);
        qreal stepSize = 4.0 / m_zoom;
        int steps = std::max(1, static_cast<int>(std::ceil(dist / stepSize)));
        qreal hitTol = 12.0 / m_zoom;

        if (m_doc) {
            const auto &elems = m_doc->elements();
            for (int s = 0; s <= steps; ++s) {
                qreal t = static_cast<qreal>(s) / steps;
                QPointF samplePt = prevWorld + t * (worldPos - prevWorld);

                for (int i = elems.size() - 1; i >= 0; --i) {
                    if (!m_markedForDeletion.contains(elems[i].id) &&
                        Geometry::hitTest(elems[i], samplePt, hitTol, false)) {
                        m_markedForDeletion.insert(elems[i].id);
                    }
                }
            }
        }
        update();
        break;
    }

    case DragMode::Laser: {
        if (!m_laserStrokes.isEmpty() && m_laserStrokes.last().finishedTime == 0) {
            auto &pts = m_laserStrokes.last().points;
            qreal minDist = 1.5 / m_zoom;
            if (pts.isEmpty() || Geometry::distance(pts.last(), worldPos) >= minDist) {
                pts.append(worldPos);
            }
        }
        if (!m_laserTimer.isActive()) {
            m_laserTimer.start(16);
        }
        update();
        break;
    }

    case DragMode::MoveElements: {
        QPointF totalDelta = worldPos - m_dragStartWorld;
        if (event->modifiers() & Qt::ShiftModifier) {
            // Shift locks to strictly horizontal or vertical
            if (std::abs(totalDelta.x()) > std::abs(totalDelta.y())) {
                totalDelta.setY(0);
            } else {
                totalDelta.setX(0);
            }
        }
        if (m_snapToGrid && !m_dragInitialSelectionBounds.isEmpty()) {
            QPointF newTopLeft = m_dragInitialSelectionBounds.topLeft() + totalDelta;
            QPointF snappedTopLeft = snap(newTopLeft);
            totalDelta = snappedTopLeft - m_dragInitialSelectionBounds.topLeft();
        }

        if (m_doc && !m_dragInitialElements.isEmpty()) {
            auto elements = m_dragInitialElements;
            const auto &selIds = m_doc->selectedIds();
            for (auto &el : elements) {
                if (!selIds.contains(el.id) || el.locked) continue;
                el.bounds.translate(totalDelta);
                if (el.type == ElementType::Stroke) {
                    for (auto &p : el.points) {
                        p.x += totalDelta.x();
                        p.y += totalDelta.y();
                    }
                } else if (el.type == ElementType::Line || el.type == ElementType::Arrow || el.type == ElementType::Connector) {
                    el.startPoint += totalDelta;
                    el.endPoint += totalDelta;
                }
            }
            m_doc->setElements(elements);
        }
        update();
        break;
    }

    case DragMode::ResizeElements: {
        QPointF totalDelta = worldPos - m_dragStartWorld;
        bool preserveAspect = (event->modifiers() & Qt::ShiftModifier);
        QRectF origBounds = m_dragInitialSelectionBounds;
        if (origBounds.width() <= 1e-4 || origBounds.height() <= 1e-4) break;

        QRectF newBounds = origBounds;
        switch (m_activeHandle) {
        case HandleType::NW: newBounds.setTopLeft(origBounds.topLeft() + totalDelta); break;
        case HandleType::N:  newBounds.setTop(origBounds.top() + totalDelta.y()); break;
        case HandleType::NE: newBounds.setTopRight(origBounds.topRight() + totalDelta); break;
        case HandleType::E:  newBounds.setRight(origBounds.right() + totalDelta.x()); break;
        case HandleType::SE: newBounds.setBottomRight(origBounds.bottomRight() + totalDelta); break;
        case HandleType::S:  newBounds.setBottom(origBounds.bottom() + totalDelta.y()); break;
        case HandleType::SW: newBounds.setBottomLeft(origBounds.bottomLeft() + totalDelta); break;
        case HandleType::W:  newBounds.setLeft(origBounds.left() + totalDelta.x()); break;
        default: break;
        }

        if (newBounds.width() < 10) newBounds.setWidth(10);
        if (newBounds.height() < 10) newBounds.setHeight(10);

        qreal scaleX = newBounds.width() / origBounds.width();
        qreal scaleY = newBounds.height() / origBounds.height();
        if (preserveAspect) {
            qreal scale = std::min(scaleX, scaleY);
            scaleX = scale;
            scaleY = scale;
            newBounds.setWidth(origBounds.width() * scale);
            newBounds.setHeight(origBounds.height() * scale);
        }

        if (m_doc && !m_dragInitialElements.isEmpty()) {
            auto elements = m_dragInitialElements;
            const auto &selIds = m_doc->selectedIds();
            for (auto &el : elements) {
                if (!selIds.contains(el.id) || el.locked) continue;

                qreal relX = (el.bounds.left() - origBounds.left()) / origBounds.width();
                qreal relY = (el.bounds.top() - origBounds.top()) / origBounds.height();
                qreal relW = el.bounds.width() / origBounds.width();
                qreal relH = el.bounds.height() / origBounds.height();

                el.bounds = QRectF(
                    newBounds.left() + relX * newBounds.width(),
                    newBounds.top() + relY * newBounds.height(),
                    relW * newBounds.width(),
                    relH * newBounds.height()
                );

                if (el.type == ElementType::Text) {
                    qreal fontScale = std::min(scaleX, scaleY);
                    el.fontSize = std::max(8.0, el.fontSize * fontScale);
                    el.bounds = Geometry::calculateTextBounds(el.text, el.bounds.topLeft(), el.fontSize, el.fontFamily);
                } else if (el.type == ElementType::Stroke || el.type == ElementType::Highlighter) {
                    for (auto &p : el.points) {
                        qreal px = (p.x - origBounds.left()) / origBounds.width();
                        qreal py = (p.y - origBounds.top()) / origBounds.height();
                        p.x = newBounds.left() + px * newBounds.width();
                        p.y = newBounds.top() + py * newBounds.height();
                    }
                } else if (el.type == ElementType::Line || el.type == ElementType::Arrow || el.type == ElementType::Connector) {
                    qreal sx = (el.startPoint.x() - origBounds.left()) / origBounds.width();
                    qreal sy = (el.startPoint.y() - origBounds.top()) / origBounds.height();
                    qreal ex = (el.endPoint.x() - origBounds.left()) / origBounds.width();
                    qreal ey = (el.endPoint.y() - origBounds.top()) / origBounds.height();
                    el.startPoint = QPointF(newBounds.left() + sx * newBounds.width(), newBounds.top() + sy * newBounds.height());
                    el.endPoint = QPointF(newBounds.left() + ex * newBounds.width(), newBounds.top() + ey * newBounds.height());
                }
            }
            m_doc->setElements(elements);
        }
        update();
        break;
    }

    case DragMode::RubberbandSelect: {
        qreal left = std::min(m_dragStartWorld.x(), worldPos.x());
        qreal top = std::min(m_dragStartWorld.y(), worldPos.y());
        qreal w = std::abs(worldPos.x() - m_dragStartWorld.x());
        qreal h = std::abs(worldPos.y() - m_dragStartWorld.y());
        m_rubberbandRect = QRectF(left, top, w, h);

        if (m_doc) {
            QSet<QString> selected = m_dragStartSelection;
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

    bool isNowDrawing = (m_dragMode == DragMode::DrawStroke || m_dragMode == DragMode::DrawShape ||
                         m_dragMode == DragMode::DrawConnector || m_dragMode == DragMode::Erase ||
                         m_dragMode == DragMode::Laser);
    if (m_isDrawing != isNowDrawing) {
        m_isDrawing = isNowDrawing;
        emit isDrawingChanged();
    }

    event->accept();
}

void CanvasItem::mouseReleaseEvent(QMouseEvent *event) {
    m_holdToStraightenTimer.stop();

    if (m_dragMode == DragMode::Pan) {
        m_dragMode = DragMode::None;
        m_isPanning = false;
        emit isPanningChanged();
        if (m_isDrawing) {
            m_isDrawing = false;
            emit isDrawingChanged();
        }
        event->accept();
        return;
    }

    switch (m_dragMode) {
    case DragMode::DrawStroke: {
        if (m_currentStrokePoints.size() >= 1 && m_doc) {
            QPointF relWorld = screenToWorld(event->position());
            if (m_currentStrokePoints.size() >= 2 &&
                Geometry::distance(m_currentStrokePoints.last().toPointF(), relWorld) > (1.0 / m_zoom)) {
                StrokePoint pt;
                pt.x = relWorld.x();
                pt.y = relWorld.y();
                pt.pressure = eventPressure(event);
                pt.time = event->timestamp();
                m_currentStrokePoints.append(pt);
            }

            Element elem;
            if (m_activeTool == Tool::Highlighter) {
                elem.type = ElementType::Highlighter;
                elem.strokeWidth = std::max(16.0, m_strokeWidth * 5.0);
            } else {
                elem.type = ElementType::Stroke;
                elem.strokeWidth = m_strokeWidth;
            }
            elem.points = m_isStraightened ? m_currentStrokePoints : Geometry::smoothPoints(m_currentStrokePoints);
            elem.strokeColor = m_strokeColor;
            elem.bounds = Geometry::calculateStrokeBounds(elem.points, elem.strokeWidth);
            m_doc->addElement(elem);
            if (m_history) {
                m_history->pushState(m_activeTool == Tool::Highlighter ? "Highlighter Stroke" : "Pen Stroke",
                                     m_doc->elements(), m_doc->selectedIds());
            }
        }
        m_isStraightened = false;
        m_currentStrokePoints.clear();
        break;
    }

    case DragMode::DrawShape: {
        if (m_doc) {
            Element elem;
            elem.strokeColor = m_strokeColor;
            elem.strokeWidth = m_strokeWidth;
            elem.fillColor = m_currentFillColor;
            bool added = false;

            if (m_activeTool == Tool::Rectangle && m_currentShapeBounds.width() > 3 && m_currentShapeBounds.height() > 3) {
                elem.type = ElementType::Rectangle;
                elem.bounds = m_currentShapeBounds;
                elem.cornerRadius = 4.0;
                m_doc->addElement(elem);
                added = true;
            } else if (m_activeTool == Tool::Ellipse && m_currentShapeBounds.width() > 3 && m_currentShapeBounds.height() > 3) {
                elem.type = ElementType::Ellipse;
                elem.bounds = m_currentShapeBounds;
                m_doc->addElement(elem);
                added = true;
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
                added = true;
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
                added = true;
            }

            if (added && m_history) {
                m_history->pushState("Add Shape", m_doc->elements(), m_doc->selectedIds());
            }
        }
        m_currentShapeBounds = QRectF();
        break;
    }

    case DragMode::DrawConnector: {
        if (m_doc && Geometry::distance(m_lineStart, m_lineEnd) > 5) {
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
            if (m_history) {
                m_history->pushState("Add Connector", m_doc->elements(), m_doc->selectedIds());
            }
        }
        m_connectorSourceId.clear();
        m_connectorTargetId.clear();
        break;
    }

    case DragMode::RubberbandSelect: {
        qreal dragDist = Geometry::distance(m_dragStartWorld, screenToWorld(event->position()));
        if (dragDist <= 3.0 && !(event->modifiers() & Qt::ShiftModifier)) {
            // Simple click on empty canvas clears selection
            if (m_doc) m_doc->clearSelection();
        }
        m_rubberbandRect = QRectF();
        break;
    }

    case DragMode::Erase: {
        if (!m_markedForDeletion.isEmpty() && m_doc) {
            for (const QString &id : m_markedForDeletion) {
                m_doc->removeElement(id);
            }
            if (m_history) {
                m_history->pushState("Erase", m_doc->elements(), m_doc->selectedIds());
            }
        }
        m_markedForDeletion.clear();
        m_eraserTrail.clear();
        break;
    }

    case DragMode::Laser: {
        if (!m_laserStrokes.isEmpty() && m_laserStrokes.last().finishedTime == 0) {
            m_laserStrokes.last().finishedTime = QDateTime::currentMSecsSinceEpoch();
        }
        if (!m_laserTimer.isActive()) {
            m_laserTimer.start(16);
        }
        update();
        break;
    }

    case DragMode::MoveElements: {
        qreal dist = Geometry::distance(m_dragStartWorld, screenToWorld(event->position()));
        if (dist > 1.0 && m_doc && m_history) {
            m_history->pushState("Move", m_doc->elements(), m_doc->selectedIds());
        } else if (dist <= 3.0 && !m_clickedElementId.isEmpty() && m_doc && m_doc->selectedIds().size() > 1 && !(event->modifiers() & Qt::ShiftModifier)) {
            // Single-click on a specific element inside a multi-selection narrows selection to that single element
            m_doc->select(m_clickedElementId, false);
        }
        m_clickedElementId.clear();
        break;
    }

    case DragMode::ResizeElements: {
        if (m_doc && m_history) {
            m_history->pushState("Resize", m_doc->elements(), m_doc->selectedIds());
        }
        break;
    }

    default:
        break;
    }

    m_dragMode = DragMode::None;
    m_activeHandle = HandleType::None;
    if (m_isDrawing) {
        m_isDrawing = false;
        emit isDrawingChanged();
    }
    update();
    event->accept();
}

void CanvasItem::mouseDoubleClickEvent(QMouseEvent *event) {
    if (!m_doc) return;
    QPointF worldPos = screenToWorld(event->position());

    const auto &elems = m_doc->elements();
    for (int i = elems.size() - 1; i >= 0; --i) {
        const auto &el = elems[i];
        if (el.type == ElementType::Text && Geometry::hitTest(el, worldPos, 8.0)) {
            QPointF sPos = worldToScreen(el.bounds.topLeft());
            emit editTextRequested(el.id, el.text, sPos.x(), sPos.y(), el.fontSize, el.strokeColor.name());
            event->accept();
            return;
        } else if (el.type == ElementType::StickyNote && Geometry::hitTest(el, worldPos, 8.0)) {
            QPointF sPos = worldToScreen(el.bounds.topLeft() + QPointF(12.0, 12.0));
            emit editTextRequested(el.id, el.text, sPos.x(), sPos.y(), el.fontSize,
                                  m_theme ? m_theme->current().noteText : "#ebdbb2");
            event->accept();
            return;
        }
    }

    // Double-click on empty canvas in Select mode creates a new text element (tldraw signature behavior)
    if (m_activeTool == Tool::Select) {
        QPointF snapped = snap(worldPos);
        QPointF sPos = worldToScreen(snapped);
        emit createTextRequested(snapped.x(), snapped.y(), sPos.x(), sPos.y(), 16.0, m_strokeColor.name());
        event->accept();
        return;
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
    if (event->key() == Qt::Key_Escape) {
        if (m_dragMode == DragMode::MoveElements || m_dragMode == DragMode::ResizeElements) {
            if (m_doc) m_doc->setElements(m_dragInitialElements);
            m_dragMode = DragMode::None;
            m_activeHandle = HandleType::None;
            update();
            event->accept();
            return;
        }
        if (m_dragMode == DragMode::Erase) {
            m_dragMode = DragMode::None;
            m_eraserTrail.clear();
            m_markedForDeletion.clear();
            update();
            event->accept();
            return;
        }
        if (m_dragMode != DragMode::None) {
            m_dragMode = DragMode::None;
            m_currentStrokePoints.clear();
            m_currentShapeBounds = QRectF();
            m_rubberbandRect = QRectF();
            update();
            event->accept();
            return;
        }
    }

    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = true;
        setCursor(Qt::OpenHandCursor);
        event->accept();
        return;
    }

    // Enter / Return on single selected text or note opens inline editor (tldraw behavior)
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && m_doc && m_doc->selectedIds().size() == 1) {
        QString selId = *m_doc->selectedIds().begin();
        const Element *el = m_doc->findElement(selId);
        if (el) {
            if (el->type == ElementType::Text) {
                QPointF sPos = worldToScreen(el->bounds.topLeft());
                emit editTextRequested(el->id, el->text, sPos.x(), sPos.y(), el->fontSize, el->strokeColor.name());
                event->accept();
                return;
            } else if (el->type == ElementType::StickyNote) {
                QPointF sPos = worldToScreen(el->bounds.topLeft() + QPointF(12.0, 12.0));
                emit editTextRequested(el->id, el->text, sPos.x(), sPos.y(), el->fontSize,
                                      m_theme ? m_theme->current().noteText : "#ebdbb2");
                event->accept();
                return;
            }
        }
    }

    if (m_backend && m_backend->handleKeyPress(event->key(), static_cast<int>(event->modifiers()))) {
        event->accept();
        return;
    }

    event->ignore();
    QQuickPaintedItem::keyPressEvent(event);
}

void CanvasItem::keyReleaseEvent(QKeyEvent *event) {
    if (event->key() == Qt::Key_Space && !event->isAutoRepeat()) {
        m_spaceDown = false;
        unsetCursor();
        event->accept();
        return;
    }
    event->ignore();
    QQuickPaintedItem::keyReleaseEvent(event);
}

void CanvasItem::hoverMoveEvent(QHoverEvent *event) {
    QPointF worldPos = screenToWorld(event->position());
    emit pointerMoved(worldPos.x(), worldPos.y());
    m_lastHoverWorld = worldPos;

    if (m_activeTool == Tool::Highlighter) {
        setCursor(Qt::CrossCursor);
        return;
    }

    if (m_activeTool == Tool::Laser) {
        setCursor(Qt::CrossCursor);
        update();
        return;
    }

    if (m_activeTool == Tool::Eraser) {
        setCursor(Qt::CrossCursor);
        return;
    }

    if (m_activeTool == Tool::Hand) {
        setCursor(Qt::OpenHandCursor);
        return;
    }

    if (m_activeTool == Tool::Select) {
        if (m_doc && !m_doc->selectedIds().isEmpty()) {
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
                break;
            }

            qreal hitTol = std::max(6.0, 10.0 / m_zoom);
            if (m_doc->selectionBounds().adjusted(-hitTol, -hitTol, hitTol, hitTol).contains(worldPos)) {
                setCursor(Qt::SizeAllCursor);
                return;
            }
        }

        if (m_doc) {
            qreal hitTol = std::max(6.0, 10.0 / m_zoom);
            for (const auto &el : m_doc->elements()) {
                if (Geometry::hitTest(el, worldPos, hitTol)) {
                    setCursor(Qt::PointingHandCursor);
                    return;
                }
            }
        }

        unsetCursor();
        return;
    }
}

void CanvasItem::dragEnterEvent(QDragEnterEvent *event) {
    if (event->mimeData()->hasUrls() || event->mimeData()->hasImage()) {
        event->acceptProposedAction();
    }
}

void CanvasItem::dragMoveEvent(QDragMoveEvent *event) {
    if (event->mimeData()->hasUrls() || event->mimeData()->hasImage()) {
        event->acceptProposedAction();
    }
}

void CanvasItem::dropEvent(QDropEvent *event) {
    if (!m_backend) return;
    QPointF worldPos = screenToWorld(event->position());
    if (event->mimeData()->hasImage()) {
        QImage img = qvariant_cast<QImage>(event->mimeData()->imageData());
        if (!img.isNull()) {
            m_backend->insertImage(img, "Drop Image", worldPos);
            event->acceptProposedAction();
            return;
        }
    }
    if (event->mimeData()->hasUrls()) {
        for (const QUrl &url : event->mimeData()->urls()) {
            QString path = url.toLocalFile();
            if (!path.isEmpty()) {
                m_backend->insertImageFromFile(path, worldPos);
            }
        }
        event->acceptProposedAction();
    }
}

} // namespace DrawBoard
