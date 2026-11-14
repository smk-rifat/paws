#pragma once

#include "document.h"
#include "geometry.h"
#include "history.h"
#include "theme.h"
#include "types.h"
#include <QPointF>
#include <QQuickPaintedItem>
#include <QRectF>

namespace DrawBoard {

class CanvasItem : public QQuickPaintedItem {
    Q_OBJECT

    Q_PROPERTY(qreal panX READ panX WRITE setPanX NOTIFY panChanged)
    Q_PROPERTY(qreal panY READ panY WRITE setPanY NOTIFY panChanged)
    Q_PROPERTY(qreal zoom READ zoom WRITE setZoom NOTIFY zoomChanged)
    Q_PROPERTY(int gridStyle READ gridStyle WRITE setGridStyle NOTIFY gridStyleChanged)
    Q_PROPERTY(qreal gridSize READ gridSize WRITE setGridSize NOTIFY gridSizeChanged)
    Q_PROPERTY(bool snapToGrid READ snapToGrid WRITE setSnapToGrid NOTIFY snapToGridChanged)
    Q_PROPERTY(QString activeTool READ activeTool WRITE setActiveTool NOTIFY activeToolChanged)
    Q_PROPERTY(qreal strokeWidth READ strokeWidth WRITE setStrokeWidth NOTIFY strokeWidthChanged)
    Q_PROPERTY(QColor strokeColor READ strokeColor WRITE setStrokeColor NOTIFY strokeColorChanged)
    Q_PROPERTY(QColor currentFillColor READ currentFillColor WRITE setCurrentFillColor NOTIFY currentFillColorChanged)
    Q_PROPERTY(bool isPanning READ isPanning NOTIFY isPanningChanged)

public:
    explicit CanvasItem(QQuickItem *parent = nullptr);

    void setDocument(Document *doc);
    void setHistory(History *history);
    void setTheme(Theme *theme);

    qreal panX() const { return m_panX; }
    void setPanX(qreal x);

    qreal panY() const { return m_panY; }
    void setPanY(qreal y);

    qreal zoom() const { return m_zoom; }
    void setZoom(qreal z);

    int gridStyle() const { return static_cast<int>(m_gridStyle); }
    void setGridStyle(int style);

    qreal gridSize() const { return m_gridSize; }
    void setGridSize(qreal size);

    bool snapToGrid() const { return m_snapToGrid; }
    void setSnapToGrid(bool snap);

    QString activeTool() const;
    void setActiveTool(const QString &toolStr);

    qreal strokeWidth() const { return m_strokeWidth; }
    void setStrokeWidth(qreal width);

    QColor strokeColor() const { return m_strokeColor; }
    void setStrokeColor(const QColor &color);

    QColor currentFillColor() const { return m_currentFillColor; }
    void setCurrentFillColor(const QColor &color);

    bool isPanning() const { return m_isPanning; }

    // Coordinate conversions
    Q_INVOKABLE QPointF screenToWorld(const QPointF &screenPoint) const;
    Q_INVOKABLE QPointF worldToScreen(const QPointF &worldPoint) const;
    Q_INVOKABLE QRectF worldRectToScreen(const QRectF &worldRect) const;

    // Viewport actions
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void zoomToFit();

    void paint(QPainter *painter) override;

signals:
    void panChanged();
    void zoomChanged();
    void gridStyleChanged();
    void gridSizeChanged();
    void snapToGridChanged();
    void activeToolChanged();
    void strokeWidthChanged();
    void strokeColorChanged();
    void currentFillColorChanged();
    void isPanningChanged();
    void editTextRequested(const QString &id, const QString &text, const QRectF &screenRect);
    void pointerMoved(qreal worldX, qreal worldY);

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void keyReleaseEvent(QKeyEvent *event) override;
    void hoverMoveEvent(QHoverEvent *event) override;

private:
    QPointF snap(const QPointF &pt) const;
    void drawGrid(QPainter *painter, const QRectF &visibleWorld);
    void drawSelectionOverlay(QPainter *painter);
    void drawInProgress(QPainter *painter);

    Document *m_doc = nullptr;
    History *m_history = nullptr;
    Theme *m_theme = nullptr;

    qreal m_panX = 0.0;
    qreal m_panY = 0.0;
    qreal m_zoom = 1.0;
    GridStyle m_gridStyle = GridStyle::Dots;
    qreal m_gridSize = 20.0;
    bool m_snapToGrid = false;

    Tool m_activeTool = Tool::Select;
    qreal m_strokeWidth = 2.0;
    QColor m_strokeColor = QColor("#ebdbb2");
    QColor m_currentFillColor = Qt::transparent;

    // Interaction states
    bool m_isPanning = false;
    bool m_spaceDown = false;
    QPointF m_lastMousePos;
    QPointF m_dragStartWorld;

    enum class DragMode {
        None,
        Pan,
        DrawStroke,
        DrawShape,
        MoveElements,
        ResizeElements,
        RubberbandSelect,
        DrawConnector
    };
    DragMode m_dragMode = DragMode::None;

    // In-progress data
    QVector<StrokePoint> m_currentStrokePoints;
    QRectF m_currentShapeBounds;
    QPointF m_lineStart;
    QPointF m_lineEnd;
    QRectF m_rubberbandRect;
    HandleType m_activeHandle = HandleType::None;

    // Connector in-progress
    QString m_connectorSourceId;
    CardinalPoint m_connectorSourceCardinal = CardinalPoint::None;
    QString m_connectorTargetId;
    CardinalPoint m_connectorTargetCardinal = CardinalPoint::None;
    QPointF m_connectorHoverPoint;

    // Hovered target
    QString m_hoveredElementId;
};

} // namespace DrawBoard
