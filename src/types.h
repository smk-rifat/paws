#pragma once

#include <QColor>
#include <QPointF>
#include <QRectF>
#include <QString>
#include <QVector>

namespace DrawBoard {

enum class Tool {
    Select,
    Pen,
    Eraser,
    Rectangle,
    Ellipse,
    Line,
    Arrow,
    Text,
    StickyNote,
    Connector,
    Hand
};

enum class ElementType {
    Stroke,
    Rectangle,
    Ellipse,
    Line,
    Arrow,
    Text,
    StickyNote,
    Connector
};

enum class CardinalPoint {
    None = 0,
    Top = 1,
    Right = 2,
    Bottom = 3,
    Left = 4,
    Center = 5
};

enum class HandleType {
    None = 0,
    NW,
    N,
    NE,
    E,
    SE,
    S,
    SW,
    W,
    Rotate,
    StartPoint, // for line/arrow/connector
    EndPoint
};

enum class GridStyle {
    Dots = 0,
    Lines = 1,
    Crosses = 2,
    None = 3
};

struct StrokePoint {
    qreal x = 0;
    qreal y = 0;
    qreal pressure = 0.5;
    qint64 time = 0;

    QPointF toPointF() const { return QPointF(x, y); }
};

struct Element {
    QString id;
    ElementType type = ElementType::Stroke;

    // Spatial bounds in world coordinates
    QRectF bounds;

    // Stroke points for freehand drawing
    QVector<StrokePoint> points;

    // Start and end points for Line, Arrow, Connector
    QPointF startPoint;
    QPointF endPoint;

    // Styling
    QColor strokeColor;
    qreal strokeWidth = 2.0;
    Qt::PenStyle strokeStyle = Qt::SolidLine;
    QColor fillColor = Qt::transparent;

    // Text & Typography
    QString text;
    QString fontFamily = "sans-serif";
    qreal fontSize = 16.0;
    Qt::Alignment textAlignment = Qt::AlignLeft | Qt::AlignTop;

    // Shapes
    qreal cornerRadius = 4.0;

    // Connectors
    QString fromId;
    QString toId;
    CardinalPoint fromCardinal = CardinalPoint::None;
    CardinalPoint toCardinal = CardinalPoint::None;
    bool arrowStart = false;
    bool arrowEnd = true;

    // Organization
    QString groupId;
    bool locked = false;
    bool hidden = false;
    int zIndex = 0;

    // Rotation (degrees)
    qreal rotation = 0.0;
};

} // namespace DrawBoard
