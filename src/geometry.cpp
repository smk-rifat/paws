#include "geometry.h"

#include <QtMath>
#include <algorithm>

namespace DrawBoard {

qreal Geometry::distance(const QPointF &p1, const QPointF &p2) {
    qreal dx = p2.x() - p1.x();
    qreal dy = p2.y() - p1.y();
    return std::hypot(dx, dy);
}

qreal Geometry::distanceToSegment(const QPointF &p, const QPointF &a, const QPointF &b) {
    qreal l2 = (b.x() - a.x()) * (b.x() - a.x()) + (b.y() - a.y()) * (b.y() - a.y());
    if (l2 <= 1e-6)
        return distance(p, a);

    qreal t = ((p.x() - a.x()) * (b.x() - a.x()) + (p.y() - a.y()) * (b.y() - a.y())) / l2;
    t = std::clamp(t, 0.0, 1.0);
    QPointF projection = a + t * (b - a);
    return distance(p, projection);
}

QVector<StrokePoint> Geometry::smoothPoints(const QVector<StrokePoint> &rawPoints) {
    if (rawPoints.size() <= 2)
        return rawPoints;

    QVector<StrokePoint> smoothed;
    smoothed.reserve(rawPoints.size());
    smoothed.append(rawPoints.first());

    // Simple 3-point moving average to filter out high-frequency sensor jitter
    for (int i = 1; i < rawPoints.size() - 1; ++i) {
        const auto &prev = rawPoints[i - 1];
        const auto &curr = rawPoints[i];
        const auto &next = rawPoints[i + 1];

        StrokePoint sp;
        sp.x = 0.25 * prev.x + 0.5 * curr.x + 0.25 * next.x;
        sp.y = 0.25 * prev.y + 0.5 * curr.y + 0.25 * next.y;
        sp.pressure = 0.25 * prev.pressure + 0.5 * curr.pressure + 0.25 * next.pressure;
        sp.time = curr.time;
        smoothed.append(sp);
    }
    smoothed.append(rawPoints.last());
    return smoothed;
}

QPainterPath Geometry::buildSmoothPath(const QVector<StrokePoint> &points) {
    QPainterPath path;
    if (points.isEmpty())
        return path;

    if (points.size() == 1) {
        path.addEllipse(points[0].toPointF(), 1.0, 1.0);
        return path;
    }

    if (points.size() == 2) {
        path.moveTo(points[0].toPointF());
        path.lineTo(points[1].toPointF());
        return path;
    }

    // Catmull-Rom spline converted to cubic Bezier curves
    path.moveTo(points[0].toPointF());

    for (int i = 0; i < points.size() - 1; ++i) {
        QPointF p0 = (i > 0) ? points[i - 1].toPointF() : points[i].toPointF();
        QPointF p1 = points[i].toPointF();
        QPointF p2 = points[i + 1].toPointF();
        QPointF p3 = (i + 2 < points.size()) ? points[i + 2].toPointF() : points[i + 1].toPointF();

        QPointF c1 = p1 + (p2 - p0) / 6.0;
        QPointF c2 = p2 - (p3 - p1) / 6.0;

        path.cubicTo(c1, c2, p2);
    }

    return path;
}

QRectF Geometry::calculateStrokeBounds(const QVector<StrokePoint> &points, qreal strokeWidth) {
    if (points.isEmpty())
        return QRectF();

    qreal minX = points[0].x;
    qreal maxX = points[0].x;
    qreal minY = points[0].y;
    qreal maxY = points[0].y;

    for (const auto &p : points) {
        if (p.x < minX) minX = p.x;
        if (p.x > maxX) maxX = p.x;
        if (p.y < minY) minY = p.y;
        if (p.y > maxY) maxY = p.y;
    }

    qreal pad = strokeWidth * 0.5 + 4.0;
    return QRectF(minX - pad, minY - pad, (maxX - minX) + pad * 2.0, (maxY - minY) + pad * 2.0);
}

bool Geometry::hitTest(const Element &elem, const QPointF &point, qreal tolerance) {
    qreal tol = tolerance + elem.strokeWidth * 0.5;

    switch (elem.type) {
    case ElementType::Stroke: {
        if (!elem.bounds.adjusted(-tol, -tol, tol, tol).contains(point))
            return false;

        for (int i = 0; i < elem.points.size() - 1; ++i) {
            if (distanceToSegment(point, elem.points[i].toPointF(), elem.points[i + 1].toPointF()) <= tol)
                return true;
        }
        return false;
    }

    case ElementType::Rectangle: {
        QRectF b = elem.bounds;
        if (elem.fillColor != Qt::transparent && elem.fillColor.alpha() > 0) {
            return b.contains(point);
        }
        // Outline check: distance to 4 segments
        QPointF tl = b.topLeft();
        QPointF tr = b.topRight();
        QPointF br = b.bottomRight();
        QPointF bl = b.bottomLeft();
        return distanceToSegment(point, tl, tr) <= tol ||
               distanceToSegment(point, tr, br) <= tol ||
               distanceToSegment(point, br, bl) <= tol ||
               distanceToSegment(point, bl, tl) <= tol;
    }

    case ElementType::Ellipse: {
        QRectF b = elem.bounds;
        qreal rx = b.width() * 0.5;
        qreal ry = b.height() * 0.5;
        if (rx <= 1e-4 || ry <= 1e-4)
            return false;

        QPointF center = b.center();
        qreal normX = (point.x() - center.x()) / rx;
        qreal normY = (point.y() - center.y()) / ry;
        qreal d = normX * normX + normY * normY;

        if (elem.fillColor != Qt::transparent && elem.fillColor.alpha() > 0) {
            return d <= 1.0;
        }
        // Approximate distance to ellipse edge
        qreal rAvg = (rx + ry) * 0.5;
        qreal distToEdge = std::abs(std::sqrt(d) - 1.0) * rAvg;
        return distToEdge <= tol;
    }

    case ElementType::Line:
    case ElementType::Arrow: {
        return distanceToSegment(point, elem.startPoint, elem.endPoint) <= tol;
    }

    case ElementType::Text:
    case ElementType::StickyNote: {
        return elem.bounds.adjusted(-tol, -tol, tol, tol).contains(point);
    }

    case ElementType::Connector: {
        auto segments = routeConnector(elem.startPoint, elem.fromCardinal, elem.endPoint, elem.toCardinal);
        for (int i = 0; i < segments.size() - 1; ++i) {
            if (distanceToSegment(point, segments[i], segments[i + 1]) <= tol)
                return true;
        }
        return false;
    }
    }

    return false;
}

bool Geometry::rectIntersectsElement(const QRectF &rect, const Element &element) {
    if (rect.contains(element.bounds))
        return true;

    if (!rect.intersects(element.bounds))
        return false;

    // For strokes, verify if any point lies inside rect
    if (element.type == ElementType::Stroke) {
        for (const auto &p : element.points) {
            if (rect.contains(p.toPointF()))
                return true;
        }
        return false;
    }

    return true;
}

QPointF Geometry::getCardinalPoint(const Element &element, CardinalPoint cardinal) {
    const QRectF &b = element.bounds;
    switch (cardinal) {
    case CardinalPoint::Top:
        return QPointF(b.center().x(), b.top());
    case CardinalPoint::Right:
        return QPointF(b.right(), b.center().y());
    case CardinalPoint::Bottom:
        return QPointF(b.center().x(), b.bottom());
    case CardinalPoint::Left:
        return QPointF(b.left(), b.center().y());
    case CardinalPoint::Center:
        return b.center();
    default:
        return b.center();
    }
}

CardinalPoint Geometry::findNearestCardinal(const Element &element, const QPointF &point, qreal threshold) {
    CardinalPoint points[] = {
        CardinalPoint::Top,
        CardinalPoint::Right,
        CardinalPoint::Bottom,
        CardinalPoint::Left
    };

    CardinalPoint best = CardinalPoint::None;
    qreal minD = threshold;

    for (CardinalPoint cp : points) {
        QPointF p = getCardinalPoint(element, cp);
        qreal d = distance(point, p);
        if (d < minD) {
            minD = d;
            best = cp;
        }
    }

    return best;
}

QVector<QPointF> Geometry::routeConnector(const QPointF &start, CardinalPoint startCard,
                                         const QPointF &end, CardinalPoint endCard) {
    QVector<QPointF> route;
    route.append(start);

    // Direct routing if cardinal points are None or Center
    if (startCard == CardinalPoint::None || endCard == CardinalPoint::None ||
        startCard == CardinalPoint::Center || endCard == CardinalPoint::Center) {
        route.append(end);
        return route;
    }

    // Orthogonal routing with intermediate elbows
    qreal midX = (start.x() + end.x()) * 0.5;
    qreal midY = (start.y() + end.y()) * 0.5;

    if (startCard == CardinalPoint::Left || startCard == CardinalPoint::Right) {
        if (endCard == CardinalPoint::Left || endCard == CardinalPoint::Right) {
            route.append(QPointF(midX, start.y()));
            route.append(QPointF(midX, end.y()));
        } else {
            route.append(QPointF(end.x(), start.y()));
        }
    } else {
        if (endCard == CardinalPoint::Top || endCard == CardinalPoint::Bottom) {
            route.append(QPointF(start.x(), midY));
            route.append(QPointF(end.x(), midY));
        } else {
            route.append(QPointF(start.x(), end.y()));
        }
    }

    route.append(end);
    return route;
}

QPolygonF Geometry::makeArrowHead(const QPointF &tip, const QPointF &from, qreal size) {
    qreal angle = std::atan2(tip.y() - from.y(), tip.x() - from.x());
    qreal wingAngle = M_PI / 6.0; // 30 degrees

    QPointF p1 = tip - QPointF(size * std::cos(angle - wingAngle), size * std::sin(angle - wingAngle));
    QPointF p2 = tip - QPointF(size * std::cos(angle + wingAngle), size * std::sin(angle + wingAngle));

    QPolygonF poly;
    poly << tip << p1 << p2;
    return poly;
}

QRectF Geometry::getHandleRect(const QPointF &center, qreal handleSize) {
    qreal half = handleSize * 0.5;
    return QRectF(center.x() - half, center.y() - half, handleSize, handleSize);
}

QPointF Geometry::getHandlePosition(const QRectF &b, HandleType handle) {
    switch (handle) {
    case HandleType::NW: return b.topLeft();
    case HandleType::N:  return QPointF(b.center().x(), b.top());
    case HandleType::NE: return b.topRight();
    case HandleType::E:  return QPointF(b.right(), b.center().y());
    case HandleType::SE: return b.bottomRight();
    case HandleType::S:  return QPointF(b.center().x(), b.bottom());
    case HandleType::SW: return b.bottomLeft();
    case HandleType::W:  return QPointF(b.left(), b.center().y());
    case HandleType::Rotate: return QPointF(b.center().x(), b.top() - 24.0);
    default: return QPointF();
    }
}

HandleType Geometry::hitTestHandles(const QRectF &b, const QPointF &pos, qreal handleSize) {
    if (b.isEmpty())
        return HandleType::None;

    HandleType handles[] = {
        HandleType::NW, HandleType::N, HandleType::NE, HandleType::E,
        HandleType::SE, HandleType::S, HandleType::SW, HandleType::W,
        HandleType::Rotate
    };

    for (HandleType h : handles) {
        QPointF p = getHandlePosition(b, h);
        if (getHandleRect(p, handleSize + 4.0).contains(pos))
            return h;
    }

    return HandleType::None;
}

} // namespace DrawBoard
