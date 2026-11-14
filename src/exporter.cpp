#include "exporter.h"

#include "geometry.h"
#include <QFont>
#include <QFontMetricsF>
#include <QImage>
#include <QPageSize>
#include <QPdfWriter>
#include <QSvgGenerator>
#include <algorithm>

namespace DrawBoard {

void Exporter::paintElement(QPainter &painter, const Element &el, const ThemePalette &theme) {
    if (el.hidden)
        return;

    painter.save();

    if (el.rotation != 0.0) {
        QPointF center = el.bounds.center();
        painter.translate(center);
        painter.rotate(el.rotation);
        painter.translate(-center);
    }

    QPen pen(el.strokeColor, el.strokeWidth, el.strokeStyle, Qt::RoundCap, Qt::RoundJoin);
    QBrush brush(el.fillColor);

    switch (el.type) {
    case ElementType::Stroke: {
        if (!el.points.isEmpty()) {
            QPainterPath path = Geometry::buildSmoothPath(el.points);
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            painter.drawPath(path);
        }
        break;
    }

    case ElementType::Rectangle: {
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.drawRoundedRect(el.bounds, el.cornerRadius, el.cornerRadius);
        break;
    }

    case ElementType::Ellipse: {
        painter.setPen(pen);
        painter.setBrush(brush);
        painter.drawEllipse(el.bounds);
        break;
    }

    case ElementType::Line: {
        painter.setPen(pen);
        painter.drawLine(el.startPoint, el.endPoint);
        break;
    }

    case ElementType::Arrow: {
        painter.setPen(pen);
        painter.drawLine(el.startPoint, el.endPoint);
        if (el.arrowEnd) {
            qreal arrowSize = 10.0 + el.strokeWidth * 2.0;
            QPolygonF head = Geometry::makeArrowHead(el.endPoint, el.startPoint, arrowSize);
            painter.setBrush(el.strokeColor);
            painter.setPen(Qt::NoPen);
            painter.drawPolygon(head);
        }
        if (el.arrowStart) {
            qreal arrowSize = 10.0 + el.strokeWidth * 2.0;
            QPolygonF head = Geometry::makeArrowHead(el.startPoint, el.endPoint, arrowSize);
            painter.setBrush(el.strokeColor);
            painter.setPen(Qt::NoPen);
            painter.drawPolygon(head);
        }
        break;
    }

    case ElementType::Text: {
        QFont font(el.fontFamily.isEmpty() ? "sans-serif" : el.fontFamily, el.fontSize);
        painter.setFont(font);
        painter.setPen(el.strokeColor.isValid() ? el.strokeColor : QColor(theme.text));
        painter.drawText(el.bounds, el.textAlignment | Qt::TextWordWrap, el.text);
        break;
    }

    case ElementType::StickyNote: {
        QColor noteBg = (el.fillColor.isValid() && el.fillColor != Qt::transparent)
                            ? el.fillColor : QColor(theme.note);
        QColor noteBorder = (el.strokeColor.isValid() && el.strokeColor != Qt::transparent)
                                ? el.strokeColor : QColor(theme.noteBorder);

        painter.setPen(QPen(noteBorder, 1.5));
        painter.setBrush(noteBg);
        painter.drawRoundedRect(el.bounds, 6.0, 6.0);

        QRectF textRect = el.bounds.adjusted(12.0, 12.0, -12.0, -12.0);
        QFont font(el.fontFamily.isEmpty() ? "sans-serif" : el.fontFamily, el.fontSize);
        painter.setFont(font);
        painter.setPen(QColor(theme.noteText));
        painter.drawText(textRect, Qt::AlignLeft | Qt::AlignTop | Qt::TextWordWrap, el.text);
        break;
    }

    case ElementType::Connector: {
        auto segments = Geometry::routeConnector(el.startPoint, el.fromCardinal, el.endPoint, el.toCardinal);
        if (segments.size() >= 2) {
            painter.setPen(pen);
            painter.setBrush(Qt::NoBrush);
            for (int i = 0; i < segments.size() - 1; ++i) {
                painter.drawLine(segments[i], segments[i + 1]);
            }

            qreal arrowSize = 10.0 + el.strokeWidth * 2.0;
            if (el.arrowEnd) {
                QPolygonF head = Geometry::makeArrowHead(segments.last(), segments[segments.size() - 2], arrowSize);
                painter.setBrush(el.strokeColor);
                painter.setPen(Qt::NoPen);
                painter.drawPolygon(head);
            }
            if (el.arrowStart) {
                QPolygonF head = Geometry::makeArrowHead(segments.first(), segments[1], arrowSize);
                painter.setBrush(el.strokeColor);
                painter.setPen(Qt::NoPen);
                painter.drawPolygon(head);
            }
        }
        break;
    }
    }

    painter.restore();
}

void Exporter::paintElements(QPainter &painter, const QVector<Element> &elements, const ThemePalette &theme) {
    for (const auto &el : elements) {
        paintElement(painter, el, theme);
    }
}

QRectF Exporter::calculateExportBounds(const Document &doc, qreal padding) {
    if (doc.isEmpty())
        return QRectF(0, 0, 800, 600);

    QRectF bounds;
    for (const auto &el : doc.elements()) {
        if (!el.hidden) {
            if (bounds.isEmpty())
                bounds = el.bounds;
            else
                bounds = bounds.united(el.bounds);
        }
    }

    if (bounds.isEmpty() || bounds.width() < 10 || bounds.height() < 10)
        bounds = QRectF(0, 0, 800, 600);

    return bounds.adjusted(-padding, -padding, padding, padding);
}

bool Exporter::exportPng(const QString &filePath, const Document &doc, const ThemePalette &theme,
                         qreal scale, bool transparentBg) {
    QRectF bounds = calculateExportBounds(doc);
    int width = std::max(10, static_cast<int>(std::ceil(bounds.width() * scale)));
    int height = std::max(10, static_cast<int>(std::ceil(bounds.height() * scale)));

    QImage image(width, height, QImage::Format_ARGB32_Premultiplied);
    if (!transparentBg) {
        image.fill(QColor(theme.canvas));
    } else {
        image.fill(Qt::transparent);
    }

    QPainter painter(&image);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    painter.scale(scale, scale);
    painter.translate(-bounds.topLeft());

    paintElements(painter, doc.elements(), theme);
    painter.end();

    return image.save(filePath, "PNG");
}

bool Exporter::exportSvg(const QString &filePath, const Document &doc, const ThemePalette &theme,
                         bool transparentBg) {
    QRectF bounds = calculateExportBounds(doc);
    int width = std::max(10, static_cast<int>(std::ceil(bounds.width())));
    int height = std::max(10, static_cast<int>(std::ceil(bounds.height())));

    QSvgGenerator generator;
    generator.setFileName(filePath);
    generator.setSize(QSize(width, height));
    generator.setViewBox(QRect(0, 0, width, height));
    generator.setTitle(doc.title());
    generator.setDescription("Exported from drawboard");

    QPainter painter(&generator);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    if (!transparentBg) {
        painter.fillRect(QRect(0, 0, width, height), QColor(theme.canvas));
    }

    painter.translate(-bounds.topLeft());
    paintElements(painter, doc.elements(), theme);
    painter.end();

    return true;
}

bool Exporter::exportPdf(const QString &filePath, const Document &doc, const ThemePalette &theme,
                         bool transparentBg) {
    QRectF bounds = calculateExportBounds(doc);
    qreal ptWidth = bounds.width();
    qreal ptHeight = bounds.height();

    QPdfWriter writer(filePath);
    writer.setPageSize(QPageSize(QSizeF(ptWidth, ptHeight), QPageSize::Point));
    writer.setPageMargins(QMarginsF(0, 0, 0, 0));
    writer.setResolution(300);

    QPainter painter(&writer);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setRenderHint(QPainter::TextAntialiasing, true);

    // Scale painter coordinates from 72 dpi points to 300 dpi
    qreal scale = 300.0 / 72.0;
    painter.scale(scale, scale);

    if (!transparentBg) {
        painter.fillRect(QRectF(0, 0, ptWidth, ptHeight), QColor(theme.canvas));
    }

    painter.translate(-bounds.topLeft());
    paintElements(painter, doc.elements(), theme);
    painter.end();

    return true;
}

} // namespace DrawBoard
