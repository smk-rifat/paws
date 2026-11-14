#include "document.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QSaveFile>
#include <QUuid>
#include <algorithm>

namespace DrawBoard {

Document::Document(QObject *parent)
    : QObject(parent) {}

void Document::clear() {
    m_elements.clear();
    m_selectedIds.clear();
    m_filePath.clear();
    setModified(false);
    emit documentChanged();
    emit selectionChanged();
}

QString Document::generateId() {
    return QString("el_%1_%2").arg(QDateTime::currentMSecsSinceEpoch()).arg(m_idCounter++);
}

const Element *Document::findElement(const QString &id) const {
    for (const auto &elem : m_elements) {
        if (elem.id == id)
            return &elem;
    }
    return nullptr;
}

Element *Document::findElement(const QString &id) {
    for (auto &elem : m_elements) {
        if (elem.id == id)
            return &elem;
    }
    return nullptr;
}

int Document::findElementIndex(const QString &id) const {
    for (int i = 0; i < m_elements.size(); ++i) {
        if (m_elements[i].id == id)
            return i;
    }
    return -1;
}

QString Document::addElement(const Element &elem) {
    Element copy = elem;
    if (copy.id.isEmpty())
        copy.id = generateId();

    if (copy.type == ElementType::Stroke && copy.bounds.isEmpty()) {
        copy.bounds = Geometry::calculateStrokeBounds(copy.points, copy.strokeWidth);
    }

    copy.zIndex = m_elements.size();
    m_elements.append(copy);

    updateConnectorsForElement(copy.id);
    setModified(true);
    emit documentChanged();
    return copy.id;
}

bool Document::updateElement(const Element &elem) {
    int idx = findElementIndex(elem.id);
    if (idx < 0)
        return false;

    Element copy = elem;
    if (copy.type == ElementType::Stroke) {
        copy.bounds = Geometry::calculateStrokeBounds(copy.points, copy.strokeWidth);
    }

    m_elements[idx] = copy;
    updateConnectorsForElement(copy.id);
    setModified(true);
    emit documentChanged();
    return true;
}

bool Document::removeElement(const QString &id) {
    int idx = findElementIndex(id);
    if (idx < 0)
        return false;

    m_elements.removeAt(idx);
    m_selectedIds.remove(id);

    // Also update/detach connectors referencing this element
    for (auto &el : m_elements) {
        if (el.type == ElementType::Connector) {
            if (el.fromId == id) {
                el.fromId.clear();
                el.fromCardinal = CardinalPoint::None;
            }
            if (el.toId == id) {
                el.toId.clear();
                el.toCardinal = CardinalPoint::None;
            }
        }
    }

    setModified(true);
    emit documentChanged();
    emit selectionChanged();
    return true;
}

void Document::setElements(const QVector<Element> &elements) {
    m_elements = elements;
    // prune selected ids
    QSet<QString> validIds;
    for (const auto &el : m_elements)
        validIds.insert(el.id);
    m_selectedIds.intersect(validIds);

    updateAllConnectors();
    setModified(true);
    emit documentChanged();
    emit selectionChanged();
}

void Document::select(const QString &id, bool addToSelection) {
    if (!addToSelection) {
        m_selectedIds.clear();
    }

    if (!id.isEmpty()) {
        const Element *el = findElement(id);
        if (el && !el->groupId.isEmpty()) {
            // Select whole group
            for (const auto &member : m_elements) {
                if (member.groupId == el->groupId)
                    m_selectedIds.insert(member.id);
            }
        } else if (el) {
            m_selectedIds.insert(id);
        }
    }

    emit selectionChanged();
}

void Document::selectAll() {
    m_selectedIds.clear();
    for (const auto &el : m_elements) {
        if (!el.locked && !el.hidden)
            m_selectedIds.insert(el.id);
    }
    emit selectionChanged();
}

void Document::clearSelection() {
    if (!m_selectedIds.isEmpty()) {
        m_selectedIds.clear();
        emit selectionChanged();
    }
}

void Document::setSelection(const QSet<QString> &ids) {
    m_selectedIds = ids;
    emit selectionChanged();
}

QRectF Document::selectionBounds() const {
    if (m_selectedIds.isEmpty())
        return QRectF();

    QRectF bounds;
    for (const auto &id : m_selectedIds) {
        const Element *el = findElement(id);
        if (el) {
            if (bounds.isEmpty())
                bounds = el->bounds;
            else
                bounds = bounds.united(el->bounds);
        }
    }
    return bounds;
}

QVector<Element> Document::selectedElements() const {
    QVector<Element> result;
    for (const auto &id : m_selectedIds) {
        const Element *el = findElement(id);
        if (el)
            result.append(*el);
    }
    return result;
}

void Document::moveSelected(const QPointF &delta) {
    if (delta.isNull() || m_selectedIds.isEmpty())
        return;

    for (const auto &id : m_selectedIds) {
        Element *el = findElement(id);
        if (!el || el->locked)
            continue;

        el->bounds.translate(delta);

        if (el->type == ElementType::Stroke) {
            for (auto &p : el->points) {
                p.x += delta.x();
                p.y += delta.y();
            }
        } else if (el->type == ElementType::Line || el->type == ElementType::Arrow || el->type == ElementType::Connector) {
            el->startPoint += delta;
            el->endPoint += delta;
        }

        updateConnectorsForElement(el->id);
    }

    setModified(true);
    emit documentChanged();
}

void Document::resizeSelected(HandleType handle, const QPointF &delta, bool preserveAspect) {
    if (m_selectedIds.isEmpty() || handle == HandleType::None)
        return;

    QRectF origBounds = selectionBounds();
    if (origBounds.width() <= 1e-4 || origBounds.height() <= 1e-4)
        return;

    QRectF newBounds = origBounds;

    switch (handle) {
    case HandleType::NW:
        newBounds.setTopLeft(origBounds.topLeft() + delta);
        break;
    case HandleType::N:
        newBounds.setTop(origBounds.top() + delta.y());
        break;
    case HandleType::NE:
        newBounds.setTopRight(origBounds.topRight() + delta);
        break;
    case HandleType::E:
        newBounds.setRight(origBounds.right() + delta.x());
        break;
    case HandleType::SE:
        newBounds.setBottomRight(origBounds.bottomRight() + delta);
        break;
    case HandleType::S:
        newBounds.setBottom(origBounds.bottom() + delta.y());
        break;
    case HandleType::SW:
        newBounds.setBottomLeft(origBounds.bottomLeft() + delta);
        break;
    case HandleType::W:
        newBounds.setLeft(origBounds.left() + delta.x());
        break;
    default:
        return;
    }

    if (newBounds.width() < 10) newBounds.setWidth(10);
    if (newBounds.height() < 10) newBounds.setHeight(10);

    qreal scaleX = newBounds.width() / origBounds.width();
    qreal scaleY = newBounds.height() / origBounds.height();

    if (preserveAspect) {
        qreal scale = std::min(scaleX, scaleY);
        scaleX = scale;
        scaleY = scale;
    }

    for (const auto &id : m_selectedIds) {
        Element *el = findElement(id);
        if (!el || el->locked)
            continue;

        qreal relX = (el->bounds.left() - origBounds.left()) / origBounds.width();
        qreal relY = (el->bounds.top() - origBounds.top()) / origBounds.height();
        qreal relW = el->bounds.width() / origBounds.width();
        qreal relH = el->bounds.height() / origBounds.height();

        el->bounds = QRectF(
            newBounds.left() + relX * newBounds.width(),
            newBounds.top() + relY * newBounds.height(),
            relW * newBounds.width(),
            relH * newBounds.height()
        );

        if (el->type == ElementType::Stroke) {
            for (auto &p : el->points) {
                qreal px = (p.x - origBounds.left()) / origBounds.width();
                qreal py = (p.y - origBounds.top()) / origBounds.height();
                p.x = newBounds.left() + px * newBounds.width();
                p.y = newBounds.top() + py * newBounds.height();
            }
        } else if (el->type == ElementType::Line || el->type == ElementType::Arrow || el->type == ElementType::Connector) {
            qreal sx = (el->startPoint.x() - origBounds.left()) / origBounds.width();
            qreal sy = (el->startPoint.y() - origBounds.top()) / origBounds.height();
            qreal ex = (el->endPoint.x() - origBounds.left()) / origBounds.width();
            qreal ey = (el->endPoint.y() - origBounds.top()) / origBounds.height();
            el->startPoint = QPointF(newBounds.left() + sx * newBounds.width(), newBounds.top() + sy * newBounds.height());
            el->endPoint = QPointF(newBounds.left() + ex * newBounds.width(), newBounds.top() + ey * newBounds.height());
        }

        updateConnectorsForElement(el->id);
    }

    setModified(true);
    emit documentChanged();
}

QVector<QString> Document::deleteSelected() {
    QVector<QString> deleted;
    for (const auto &id : m_selectedIds) {
        deleted.append(id);
    }

    for (const auto &id : deleted) {
        removeElement(id);
    }
    return deleted;
}

QVector<QString> Document::duplicateSelected(const QPointF &offset) {
    QVector<QString> newIds;
    QVector<Element> toAdd;

    for (const auto &id : m_selectedIds) {
        const Element *orig = findElement(id);
        if (!orig)
            continue;

        Element dup = *orig;
        dup.id = generateId();
        dup.bounds.translate(offset);

        if (dup.type == ElementType::Stroke) {
            for (auto &p : dup.points) {
                p.x += offset.x();
                p.y += offset.y();
            }
        } else if (dup.type == ElementType::Line || dup.type == ElementType::Arrow || dup.type == ElementType::Connector) {
            dup.startPoint += offset;
            dup.endPoint += offset;
        }

        // Connector targets are detached on duplication
        dup.fromId.clear();
        dup.toId.clear();

        toAdd.append(dup);
        newIds.append(dup.id);
    }

    m_selectedIds.clear();
    for (const auto &el : toAdd) {
        addElement(el);
        m_selectedIds.insert(el.id);
    }

    emit selectionChanged();
    return newIds;
}

void Document::groupSelected() {
    if (m_selectedIds.size() < 2)
        return;

    QString newGroupId = QString("group_%1").arg(QDateTime::currentMSecsSinceEpoch());
    for (const auto &id : m_selectedIds) {
        Element *el = findElement(id);
        if (el)
            el->groupId = newGroupId;
    }
    setModified(true);
    emit documentChanged();
}

void Document::ungroupSelected() {
    for (const auto &id : m_selectedIds) {
        Element *el = findElement(id);
        if (el)
            el->groupId.clear();
    }
    setModified(true);
    emit documentChanged();
}

void Document::bringForward() {
    for (int i = m_elements.size() - 2; i >= 0; --i) {
        if (m_selectedIds.contains(m_elements[i].id) && !m_selectedIds.contains(m_elements[i + 1].id)) {
            m_elements.swapItemsAt(i, i + 1);
        }
    }
    setModified(true);
    emit documentChanged();
}

void Document::sendBackward() {
    for (int i = 1; i < m_elements.size(); ++i) {
        if (m_selectedIds.contains(m_elements[i].id) && !m_selectedIds.contains(m_elements[i - 1].id)) {
            m_elements.swapItemsAt(i, i - 1);
        }
    }
    setModified(true);
    emit documentChanged();
}

void Document::bringToFront() {
    QVector<Element> unselected;
    QVector<Element> selected;
    for (const auto &el : m_elements) {
        if (m_selectedIds.contains(el.id))
            selected.append(el);
        else
            unselected.append(el);
    }
    m_elements = unselected + selected;
    setModified(true);
    emit documentChanged();
}

void Document::sendToBack() {
    QVector<Element> unselected;
    QVector<Element> selected;
    for (const auto &el : m_elements) {
        if (m_selectedIds.contains(el.id))
            selected.append(el);
        else
            unselected.append(el);
    }
    m_elements = selected + unselected;
    setModified(true);
    emit documentChanged();
}

void Document::updateConnectorsForElement(const QString &elementId) {
    const Element *sourceOrTarget = findElement(elementId);
    if (!sourceOrTarget)
        return;

    for (auto &el : m_elements) {
        if (el.type == ElementType::Connector) {
            bool updated = false;
            if (el.fromId == elementId && el.fromCardinal != CardinalPoint::None) {
                el.startPoint = Geometry::getCardinalPoint(*sourceOrTarget, el.fromCardinal);
                updated = true;
            }
            if (el.toId == elementId && el.toCardinal != CardinalPoint::None) {
                el.endPoint = Geometry::getCardinalPoint(*sourceOrTarget, el.toCardinal);
                updated = true;
            }
            if (updated) {
                qreal minX = std::min(el.startPoint.x(), el.endPoint.x());
                qreal maxX = std::max(el.startPoint.x(), el.endPoint.x());
                qreal minY = std::min(el.startPoint.y(), el.endPoint.y());
                qreal maxY = std::max(el.startPoint.y(), el.endPoint.y());
                el.bounds = QRectF(minX - 10, minY - 10, (maxX - minX) + 20, (maxY - minY) + 20);
            }
        }
    }
}

void Document::updateAllConnectors() {
    for (auto &el : m_elements) {
        if (el.type == ElementType::Connector) {
            if (!el.fromId.isEmpty() && el.fromCardinal != CardinalPoint::None) {
                const Element *src = findElement(el.fromId);
                if (src) el.startPoint = Geometry::getCardinalPoint(*src, el.fromCardinal);
            }
            if (!el.toId.isEmpty() && el.toCardinal != CardinalPoint::None) {
                const Element *tgt = findElement(el.toId);
                if (tgt) el.endPoint = Geometry::getCardinalPoint(*tgt, el.toCardinal);
            }
            qreal minX = std::min(el.startPoint.x(), el.endPoint.x());
            qreal maxX = std::max(el.startPoint.x(), el.endPoint.x());
            qreal minY = std::min(el.startPoint.y(), el.endPoint.y());
            qreal maxY = std::max(el.startPoint.y(), el.endPoint.y());
            el.bounds = QRectF(minX - 10, minY - 10, (maxX - minX) + 20, (maxY - minY) + 20);
        }
    }
}

static QString elementTypeToString(ElementType t) {
    switch (t) {
    case ElementType::Stroke: return "stroke";
    case ElementType::Rectangle: return "rectangle";
    case ElementType::Ellipse: return "ellipse";
    case ElementType::Line: return "line";
    case ElementType::Arrow: return "arrow";
    case ElementType::Text: return "text";
    case ElementType::StickyNote: return "stickyNote";
    case ElementType::Connector: return "connector";
    }
    return "stroke";
}

static ElementType stringToElementType(const QString &s) {
    if (s == "rectangle") return ElementType::Rectangle;
    if (s == "ellipse") return ElementType::Ellipse;
    if (s == "line") return ElementType::Line;
    if (s == "arrow") return ElementType::Arrow;
    if (s == "text") return ElementType::Text;
    if (s == "stickyNote") return ElementType::StickyNote;
    if (s == "connector") return ElementType::Connector;
    return ElementType::Stroke;
}

QJsonObject Document::toJson() const {
    QJsonObject root;
    root["format"] = "drawboard";
    root["schemaVersion"] = 1;
    root["appVersion"] = "0.1.0";
    root["createdAt"] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root["theme"] = m_themeId;

    QJsonArray elementsArr;
    for (const auto &el : m_elements) {
        QJsonObject o;
        o["id"] = el.id;
        o["type"] = elementTypeToString(el.type);

        QJsonArray boundsArr{el.bounds.x(), el.bounds.y(), el.bounds.width(), el.bounds.height()};
        o["bounds"] = boundsArr;

        if (el.type == ElementType::Stroke) {
            QJsonArray pts;
            for (const auto &p : el.points) {
                QJsonArray pt{p.x, p.y, p.pressure, static_cast<double>(p.time)};
                pts.append(pt);
            }
            o["points"] = pts;
        }

        if (el.type == ElementType::Line || el.type == ElementType::Arrow || el.type == ElementType::Connector) {
            o["startX"] = el.startPoint.x();
            o["startY"] = el.startPoint.y();
            o["endX"] = el.endPoint.x();
            o["endY"] = el.endPoint.y();
        }

        o["strokeColor"] = el.strokeColor.name(QColor::HexArgb);
        o["strokeWidth"] = el.strokeWidth;
        o["strokeStyle"] = static_cast<int>(el.strokeStyle);
        o["fillColor"] = el.fillColor.name(QColor::HexArgb);

        if (!el.text.isEmpty()) {
            o["text"] = el.text;
            o["fontFamily"] = el.fontFamily;
            o["fontSize"] = el.fontSize;
            o["textAlignment"] = static_cast<int>(el.textAlignment);
        }

        if (el.type == ElementType::Rectangle) {
            o["cornerRadius"] = el.cornerRadius;
        }

        if (el.type == ElementType::Connector) {
            o["fromId"] = el.fromId;
            o["toId"] = el.toId;
            o["fromCardinal"] = static_cast<int>(el.fromCardinal);
            o["toCardinal"] = static_cast<int>(el.toCardinal);
            o["arrowStart"] = el.arrowStart;
            o["arrowEnd"] = el.arrowEnd;
        }

        if (!el.groupId.isEmpty()) o["groupId"] = el.groupId;
        if (el.locked) o["locked"] = true;
        if (el.hidden) o["hidden"] = true;
        o["rotation"] = el.rotation;

        elementsArr.append(o);
    }
    root["elements"] = elementsArr;
    return root;
}

bool Document::fromJson(const QJsonObject &root) {
    if (root.value("format").toString() != "drawboard")
        return false;

    clear();
    m_themeId = root.value("theme").toString("gruvbox-dark-medium");

    QJsonArray elementsArr = root.value("elements").toArray();
    for (const auto &val : elementsArr) {
        QJsonObject o = val.toObject();
        Element el;
        el.id = o.value("id").toString();
        el.type = stringToElementType(o.value("type").toString());

        QJsonArray boundsArr = o.value("bounds").toArray();
        if (boundsArr.size() == 4) {
            el.bounds = QRectF(boundsArr[0].toDouble(), boundsArr[1].toDouble(),
                               boundsArr[2].toDouble(), boundsArr[3].toDouble());
        }

        if (el.type == ElementType::Stroke) {
            QJsonArray pts = o.value("points").toArray();
            for (const auto &pv : pts) {
                QJsonArray pt = pv.toArray();
                if (pt.size() >= 2) {
                    StrokePoint sp;
                    sp.x = pt[0].toDouble();
                    sp.y = pt[1].toDouble();
                    sp.pressure = (pt.size() > 2) ? pt[2].toDouble() : 0.5;
                    sp.time = (pt.size() > 3) ? static_cast<qint64>(pt[3].toDouble()) : 0;
                    el.points.append(sp);
                }
            }
        }

        if (el.type == ElementType::Line || el.type == ElementType::Arrow || el.type == ElementType::Connector) {
            el.startPoint = QPointF(o.value("startX").toDouble(), o.value("startY").toDouble());
            el.endPoint = QPointF(o.value("endX").toDouble(), o.value("endY").toDouble());
        }

        el.strokeColor = QColor::fromString(o.value("strokeColor").toString("#ebdbb2"));
        el.strokeWidth = o.value("strokeWidth").toDouble(2.0);
        el.strokeStyle = static_cast<Qt::PenStyle>(o.value("strokeStyle").toInt(Qt::SolidLine));
        el.fillColor = QColor::fromString(o.value("fillColor").toString("#00000000"));

        el.text = o.value("text").toString();
        el.fontFamily = o.value("fontFamily").toString("sans-serif");
        el.fontSize = o.value("fontSize").toDouble(16.0);
        el.textAlignment = static_cast<Qt::Alignment>(o.value("textAlignment").toInt(Qt::AlignLeft | Qt::AlignTop));

        el.cornerRadius = o.value("cornerRadius").toDouble(4.0);

        if (el.type == ElementType::Connector) {
            el.fromId = o.value("fromId").toString();
            el.toId = o.value("toId").toString();
            el.fromCardinal = static_cast<CardinalPoint>(o.value("fromCardinal").toInt(0));
            el.toCardinal = static_cast<CardinalPoint>(o.value("toCardinal").toInt(0));
            el.arrowStart = o.value("arrowStart").toBool(false);
            el.arrowEnd = o.value("arrowEnd").toBool(true);
        }

        el.groupId = o.value("groupId").toString();
        el.locked = o.value("locked").toBool(false);
        el.hidden = o.value("hidden").toBool(false);
        el.rotation = o.value("rotation").toDouble(0.0);

        m_elements.append(el);
    }

    updateAllConnectors();
    setModified(false);
    emit documentChanged();
    emit fileLoaded();
    return true;
}

bool Document::saveToFile(const QString &filePath) {
    QSaveFile saveFile(filePath);
    if (!saveFile.open(QIODevice::WriteOnly))
        return false;

    QJsonDocument doc(toJson());
    saveFile.write(doc.toJson(QJsonDocument::Indented));

    if (!saveFile.commit())
        return false;

    m_filePath = filePath;
    setModified(false);
    return true;
}

bool Document::loadFromFile(const QString &filePath) {
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly))
        return false;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return false;

    bool ok = fromJson(doc.object());
    if (ok) {
        m_filePath = filePath;
        setModified(false);
    }
    return ok;
}

QString Document::title() const {
    if (m_filePath.isEmpty())
        return "Untitled";
    return QFileInfo(m_filePath).fileName();
}

void Document::setModified(bool modified) {
    if (m_isModified != modified) {
        m_isModified = modified;
        emit modifiedChanged(m_isModified);
    }
}

} // namespace DrawBoard
