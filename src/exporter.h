#pragma once

#include "document.h"
#include "theme.h"
#include <QPainter>
#include <QString>

namespace DrawBoard {

class Exporter {
public:
    // Core painter rendering function shared across canvas item and all exporters
    static void paintElement(QPainter &painter, const Element &el, const ThemePalette &theme);
    static void paintElements(QPainter &painter, const QVector<Element> &elements, const ThemePalette &theme);

    // Export functions
    static bool exportPng(const QString &filePath, const Document &doc, const ThemePalette &theme,
                          qreal scale = 2.0, bool transparentBg = false);
    static bool exportSvg(const QString &filePath, const Document &doc, const ThemePalette &theme,
                          bool transparentBg = false);
    static bool exportPdf(const QString &filePath, const Document &doc, const ThemePalette &theme,
                          bool transparentBg = false);

    static QRectF calculateExportBounds(const Document &doc, qreal padding = 40.0);
};

} // namespace DrawBoard
