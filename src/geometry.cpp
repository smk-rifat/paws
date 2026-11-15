#include "geometry.h"

#include <QFont>
#include <QFontMetricsF>
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

    int n = rawPoints.size();
    QVector<StrokePoint> smoothed;
    smoothed.reserve(n);
    smoothed.append(rawPoints.first());

    if (n == 3) {
        StrokePoint sp;
        sp.x = 0.25 * rawPoints[0].x + 0.5 * rawPoints[1].x + 0.25 * rawPoints[2].x;
        sp.y = 0.25 * rawPoints[0].y + 0.5 * rawPoints[1].y + 0.25 * rawPoints[2].y;
        sp.pressure = 0.25 * rawPoints[0].pressure + 0.5 * rawPoints[1].pressure + 0.25 * rawPoints[2].pressure;
        sp.time = rawPoints[1].time;
        smoothed.append(sp);
    } else {
        // First interior point: 3-point binomial filter
        StrokePoint sp1;
        sp1.x = 0.25 * rawPoints[0].x + 0.5 * rawPoints[1].x + 0.25 * rawPoints[2].x;
        sp1.y = 0.25 * rawPoints[0].y + 0.5 * rawPoints[1].y + 0.25 * rawPoints[2].y;
        sp1.pressure = 0.25 * rawPoints[0].pressure + 0.5 * rawPoints[1].pressure + 0.25 * rawPoints[2].pressure;
        sp1.time = rawPoints[1].time;
        smoothed.append(sp1);

        // Core interior points: 5-point binomial filter (1/16, 4/16, 6/16, 4/16, 1/16)
        // for optimal Gaussian smoothing with zero lag and zero phase distortion
        for (int i = 2; i < n - 2; ++i) {
            StrokePoint sp;
            sp.x = (rawPoints[i - 2].x + 4.0 * rawPoints[i - 1].x + 6.0 * rawPoints[i].x +
                    4.0 * rawPoints[i + 1].x + rawPoints[i + 2].x) / 16.0;
            sp.y = (rawPoints[i - 2].y + 4.0 * rawPoints[i - 1].y + 6.0 * rawPoints[i].y +
                    4.0 * rawPoints[i + 1].y + rawPoints[i + 2].y) / 16.0;
            sp.pressure = (rawPoints[i - 2].pressure + 4.0 * rawPoints[i - 1].pressure + 6.0 * rawPoints[i].pressure +
                           4.0 * rawPoints[i + 1].pressure + rawPoints[i + 2].pressure) / 16.0;
            sp.time = rawPoints[i].time;
            smoothed.append(sp);
        }

        // Last interior point: 3-point binomial filter
        StrokePoint spLast;
        spLast.x = 0.25 * rawPoints[n - 3].x + 0.5 * rawPoints[n - 2].x + 0.25 * rawPoints[n - 1].x;
        spLast.y = 0.25 * rawPoints[n - 3].y + 0.5 * rawPoints[n - 2].y + 0.25 * rawPoints[n - 1].y;
        spLast.pressure = 0.25 * rawPoints[n - 3].pressure + 0.5 * rawPoints[n - 2].pressure + 0.25 * rawPoints[n - 1].pressure;
        spLast.time = rawPoints[n - 2].time;
        smoothed.append(spLast);
    }

    smoothed.append(rawPoints.last());
    return smoothed;
}

QPainterPath Geometry::buildSmoothPath(const QVector<StrokePoint> &points) {
    QPainterPath path;
    if (points.isEmpty())
        return path;

    if (points.size() == 1) {
        path.moveTo(points[0].toPointF());
        path.lineTo(points[0].toPointF());
        return path;
    }

    if (points.size() == 2) {
        path.moveTo(points[0].toPointF());
        path.lineTo(points[1].toPointF());
        return path;
    }

    // Midpoint quadratic Bezier spline (Chaikin / Midpoint) - guaranteed C1 continuity,
    // zero overshoot, buttery smooth curvature and instantaneous evaluation
    path.moveTo(points[0].toPointF());
    QPointF p0 = points[0].toPointF();
    QPointF p1 = points[1].toPointF();
    QPointF firstMid = (p0 + p1) * 0.5;
    path.lineTo(firstMid);

    for (int i = 1; i < points.size() - 1; ++i) {
        QPointF curr = points[i].toPointF();
        QPointF next = points[i + 1].toPointF();
        QPointF mid = (curr + next) * 0.5;
        path.quadTo(curr, mid);
    }

    path.lineTo(points.last().toPointF());
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
    qreal w = std::max(1.0, maxX - minX);
    qreal h = std::max(1.0, maxY - minY);
    return QRectF(minX - pad, minY - pad, w + pad * 2.0, h + pad * 2.0);
}

QRectF Geometry::calculateTextBounds(const QString &text, const QPointF &topLeft,
                                     qreal fontSize, const QString &fontFamily) {
    QFont font(fontFamily.isEmpty() ? "sans-serif" : fontFamily, static_cast<int>(std::round(fontSize)));
    QFontMetricsF fm(font);

    QString content = text.isEmpty() ? QString(" ") : text;
    // Word-wrap / multi-line bounding rect
    QRectF tightRect = fm.boundingRect(QRectF(0, 0, 10000, 10000), Qt::AlignLeft | Qt::TextWordWrap, content);

    qreal w = std::max(tightRect.width() + 6.0, 20.0);
    qreal h = std::max(tightRect.height() + 4.0, fm.height() + 4.0);

    return QRectF(topLeft.x(), topLeft.y(), w, h);
}

bool Geometry::hitTest(const Element &elem, const QPointF &point, qreal tolerance, bool testInterior) {
    qreal tol = tolerance + elem.strokeWidth * 0.5;

    switch (elem.type) {
    case ElementType::Highlighter:
    case ElementType::Stroke: {
        if (!elem.bounds.adjusted(-tol, -tol, tol, tol).contains(point))
            return false;

        if (elem.points.size() == 1) {
            return distance(point, elem.points[0].toPointF()) <= std::max(tol, elem.strokeWidth * 0.5 + 4.0);
        }

        for (int i = 0; i < elem.points.size() - 1; ++i) {
            if (distanceToSegment(point, elem.points[i].toPointF(), elem.points[i + 1].toPointF()) <= tol)
                return true;
        }
        return false;
    }

    case ElementType::Rectangle: {
        QRectF b = elem.bounds;
        bool hasFill = (elem.fillColor != Qt::transparent && elem.fillColor.alpha() > 0);
        if (testInterior || hasFill) {
            return b.adjusted(-tol, -tol, tol, tol).contains(point);
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
        qreal normX = (point.x() - center.x()) / (rx + tol);
        qreal normY = (point.y() - center.y()) / (ry + tol);
        qreal d = normX * normX + normY * normY;

        bool hasFill = (elem.fillColor != Qt::transparent && elem.fillColor.alpha() > 0);
        if (testInterior || hasFill) {
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

    case ElementType::Image:
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

    // For strokes and highlighters, verify if any point lies inside rect
    if (element.type == ElementType::Stroke || element.type == ElementType::Highlighter) {
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

