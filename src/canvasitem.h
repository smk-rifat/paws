#pragma once

#include "document.h"
#include "geometry.h"
#include "history.h"
#include "theme.h"
#include "types.h"
#include <QPointF>
#include <QQuickPaintedItem>
#include <QRectF>
#include <QSet>

namespace DrawBoard {

class Backend;

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
    Q_PROPERTY(bool isDrawing READ isDrawing NOTIFY isDrawingChanged)

public:
    explicit CanvasItem(QQuickItem *parent = nullptr);

    void setDocument(Document *doc);
    void setHistory(History *history);
    void setTheme(Theme *theme);
    void setBackend(Backend *backend) { m_backend = backend; }

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
    bool isDrawing() const { return m_isDrawing; }
    bool isStraightened() const { return m_isStraightened; }
    int laserStrokeCount() const { return m_laserStrokes.size(); }

    Q_INVOKABLE void checkHoldToStraighten();

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
    void isDrawingChanged();
    void createTextRequested(qreal worldX, qreal worldY, qreal screenX, qreal screenY, qreal fontSize, const QString &color);
    void editTextRequested(const QString &id, const QString &text, qreal screenX, qreal screenY, qreal fontSize, const QString &color);
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
    void dragEnterEvent(QDragEnterEvent *event) override;
    void dragMoveEvent(QDragMoveEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QPointF snap(const QPointF &pt) const;
    void drawGrid(QPainter *painter, const QRectF &visibleWorld);
    void drawSelectionOverlay(QPainter *painter);
    void drawInProgress(QPainter *painter);

    Document *m_doc = nullptr;
    History *m_history = nullptr;
    Theme *m_theme = nullptr;
    Backend *m_backend = nullptr;

    qreal m_panX = 0.0;
    qreal m_panY = 0.0;
    qreal m_zoom = 1.0;
    GridStyle m_gridStyle = GridStyle::Dots;
    qreal m_gridSize = 20.0;
    bool m_snapToGrid = false;

    Tool m_activeTool = Tool::Select;
    qreal m_strokeWidth = 3.0;
    QColor m_strokeColor = QColor("#ebdbb2");
    QColor m_currentFillColor = Qt::transparent;

    // Interaction states
    bool m_isPanning = false;
    bool m_isDrawing = false;
    bool m_spaceDown = false;
    bool m_isStraightened = false;
    QTimer m_holdToStraightenTimer;
    QPointF m_lastMousePos;
    QPointF m_dragStartWorld;

    enum class DragMode {
        None,
        Pan,
        DrawStroke,
        DrawShape,
        Erase,
        MoveElements,
        ResizeElements,
        RubberbandSelect,
        DrawConnector,
        Laser
    };
    DragMode m_dragMode = DragMode::None;

    // Laser pointer in-progress data
    struct LaserStroke {
        QVector<QPointF> points;
        qint64 finishedTime = 0; // 0 while actively drawing
    };
    QVector<LaserStroke> m_laserStrokes;
    QTimer m_laserTimer;
    QPointF m_lastHoverWorld;

    // Eraser in-progress data
    QVector<QPointF> m_eraserTrail;
    QSet<QString> m_markedForDeletion;

    // In-progress data
    QVector<StrokePoint> m_currentStrokePoints;
    QRectF m_currentShapeBounds;
    QPointF m_lineStart;
    QPointF m_lineEnd;
    QRectF m_rubberbandRect;
    HandleType m_activeHandle = HandleType::None;
    QVector<Element> m_dragInitialElements;
    QRectF m_dragInitialSelectionBounds;
    QSet<QString> m_dragStartSelection;

    // Connector in-progress
    QString m_connectorSourceId;
    CardinalPoint m_connectorSourceCardinal = CardinalPoint::None;
    QString m_connectorTargetId;
    CardinalPoint m_connectorTargetCardinal = CardinalPoint::None;
    QPointF m_connectorHoverPoint;

    // Hovered target
    QString m_hoveredElementId;
    QString m_clickedElementId;
};

} // namespace DrawBoard
