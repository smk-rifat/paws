#include "history.h"

namespace DrawBoard {

History::History(int maxDepth, QObject *parent)
    : QObject(parent), m_maxDepth(maxDepth) {}

void History::clear() {
    m_states.clear();
    m_currentIndex = -1;
    emit stateChanged();
}

void History::pushState(const QString &desc, const QVector<Element> &elements, const QSet<QString> &selectedIds) {
    // If we're not at the end of the history list, discard subsequent redo states
    if (m_currentIndex < m_states.size() - 1) {
        m_states.erase(m_states.begin() + (m_currentIndex + 1), m_states.end());
    }

    HistoryState state;
    state.description = desc;
    state.elements = elements;
    state.selectedIds = selectedIds;

    m_states.append(state);

    if (m_states.size() > m_maxDepth) {
        m_states.removeFirst();
    }

    m_currentIndex = m_states.size() - 1;
    emit stateChanged();
}

const HistoryState *History::undo() {
    if (!canUndo())
        return nullptr;

    --m_currentIndex;
    emit stateChanged();
    return &m_states[m_currentIndex];
}

const HistoryState *History::redo() {
    if (!canRedo())
        return nullptr;

    ++m_currentIndex;
    emit stateChanged();
    return &m_states[m_currentIndex];
}

QString History::undoDescription() const {
    if (canUndo() && m_currentIndex >= 0 && m_currentIndex < m_states.size())
        return m_states[m_currentIndex].description;
    return QString();
}

QString History::redoDescription() const {
    if (canRedo() && m_currentIndex + 1 < m_states.size())
        return m_states[m_currentIndex + 1].description;
    return QString();
}

} // namespace DrawBoard
