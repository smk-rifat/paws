#pragma once

#include "types.h"
#include <QPainterPath>
#include <QPolygonF>

namespace DrawBoard {

class Geometry {
public:
    // Distance functions
    static qreal distance(const QPointF &p1, const QPointF &p2);
    static qreal distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b);

    // Freehand stroke smoothing
    static QPainterPath buildSmoothPath(const QVector<StrokePoint> &points);
    static QVector<StrokePoint> smoothPoints(const QVector<StrokePoint> &rawPoints);

    // Hit testing
    static bool hitTest(const Element &element, const QPointF &point, qreal tolerance = 6.0);
    static bool rectIntersectsElement(const QRectF &rect, const Element &element);

    // Cardinal connection points
    static QPointF getCardinalPoint(const Element &element, CardinalPoint cardinal);
    static CardinalPoint findNearestCardinal(const Element &element, const QPointF &point, qreal threshold = 25.0);

    // Connectors
    static QVector<QPointF> routeConnector(const QPointF &start, CardinalPoint startCard,
                                          const QPointF &end, CardinalPoint endCard);

    // Arrowhead geometry
    static QPolygonF makeArrowHead(const QPointF &tip, const QPointF &from, qreal size = 12.0);

    // Handles
    static QRectF getHandleRect(const QPointF &center, qreal handleSize = 8.0);
    static HandleType hitTestHandles(const QRectF &bounds, const QPointF &pos, qreal handleSize = 8.0);
    static QPointF getHandlePosition(const QRectF &bounds, HandleType handle);

    // Bounding box calculations
    static QRectF calculateStrokeBounds(const QVector<StrokePoint> &points, qreal strokeWidth);
};

} // namespace DrawBoard
