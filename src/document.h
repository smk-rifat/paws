#pragma once

#include "geometry.h"
#include "types.h"
#include <QJsonObject>
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

namespace DrawBoard {

class Document : public QObject {
    Q_OBJECT

public:
    explicit Document(QObject *parent = nullptr);

    // Document state
    void clear();
    bool isEmpty() const { return m_elements.isEmpty(); }
    int elementCount() const { return m_elements.size(); }
    const QVector<Element> &elements() const { return m_elements; }
    QVector<Element> &elementsRef() { return m_elements; }

    const Element *findElement(const QString &id) const;
    Element *findElement(const QString &id);
    int findElementIndex(const QString &id) const;

    // Element mutations
    QString addElement(const Element &elem);
    bool updateElement(const Element &elem);
    bool removeElement(const QString &id);
    void setElements(const QVector<Element> &elements);

    // Selection management
    const QSet<QString> &selectedIds() const { return m_selectedIds; }
    void select(const QString &id, bool addToSelection = false);
    void selectAll();
    void clearSelection();
    void setSelection(const QSet<QString> &ids);
    bool isSelected(const QString &id) const { return m_selectedIds.contains(id); }
    QRectF selectionBounds() const;
    QVector<Element> selectedElements() const;

    // Manipulations on selected elements
    void moveSelected(const QPointF &delta);
    void resizeSelected(HandleType handle, const QPointF &delta, bool preserveAspect = false);
    QVector<QString> deleteSelected();
    QVector<QString> duplicateSelected(const QPointF &offset = QPointF(20, 20));

    // Grouping
    void groupSelected();
    void ungroupSelected();

    // Z-Order
    void bringForward();
    void sendBackward();
    void bringToFront();
    void sendToBack();

    // Connectors
    void updateConnectorsForElement(const QString &elementId);
    void updateAllConnectors();

    // Persistence & Serialization (Versioned JSON)
    QJsonObject toJson() const;
    bool fromJson(const QJsonObject &json);
    bool saveToFile(const QString &filePath);
    bool loadFromFile(const QString &filePath);

    QString filePath() const { return m_filePath; }
    void setFilePath(const QString &path) { m_filePath = path; }

    QString title() const;
    bool isModified() const { return m_isModified; }
    void setModified(bool modified);

    QString themeId() const { return m_themeId; }
    void setThemeId(const QString &id) { m_themeId = id; }

signals:
    void documentChanged();
    void selectionChanged();
    void modifiedChanged(bool modified);
    void fileLoaded();

private:
    QString generateId();

    QVector<Element> m_elements;
    QSet<QString> m_selectedIds;
    QString m_filePath;
    QString m_themeId = "gruvbox-dark-medium";
    bool m_isModified = false;
    quint64 m_idCounter = 1;
};

} // namespace DrawBoard

