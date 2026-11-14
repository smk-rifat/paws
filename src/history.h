#pragma once

#include "types.h"
#include <QObject>
#include <QSet>
#include <QString>
#include <QVector>

namespace DrawBoard {

struct HistoryState {
    QString description;
    QVector<Element> elements;
    QSet<QString> selectedIds;
};

class History : public QObject {
    Q_OBJECT

public:
    explicit History(int maxDepth = 100, QObject *parent = nullptr);

    void clear();
    void pushState(const QString &desc, const QVector<Element> &elements, const QSet<QString> &selectedIds);

    bool canUndo() const { return m_currentIndex > 0; }
    bool canRedo() const { return m_currentIndex < m_states.size() - 1; }

    const HistoryState *undo();
    const HistoryState *redo();

    QString undoDescription() const;
    QString redoDescription() const;

signals:
    void stateChanged();

private:
    int m_maxDepth;
    int m_currentIndex = -1;
    QVector<HistoryState> m_states;
};

} // namespace DrawBoard
