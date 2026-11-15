#include "shortcuts.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace DrawBoard {

ShortcutManager::ShortcutManager(QObject *parent)
    : QObject(parent) {
    initDefaults();
    load();
}

QString ShortcutManager::configFilePath() const {
    QString configDir = QStandardPaths::writableLocation(QStandardPaths::AppConfigLocation);
    if (configDir.isEmpty())
        configDir = QDir::homePath() + "/.config/paws";
    return configDir + "/shortcuts.json";
}

void ShortcutManager::initDefaults() {
    m_items = {
        // Tools
        {"tool_pen", "Tools", "Freehand Pen", "P", "P"},
        {"tool_highlighter", "Tools", "Translucent Highlighter", "M", "M"},
        {"tool_eraser", "Tools", "Eraser", "E", "E"},
        {"tool_select", "Tools", "Select", "V", "V"},
        {"tool_rect", "Tools", "Rectangle", "R", "R"},
        {"tool_ellipse", "Tools", "Circle", "C", "C"},
        {"tool_line", "Tools", "Line", "L", "L"},
        {"tool_arrow", "Tools", "Arrow", "A", "A"},
        {"tool_text", "Tools", "Text", "T", "T"},
        {"tool_note", "Tools", "Sticky Note", "N", "N"},
        {"tool_connector", "Tools", "Connector", "X", "X"},
        {"tool_hand", "Tools", "Hand / Pan", "H", "H"},
        {"tool_laser", "Tools", "Laser Pointer", "K", "K"},

        // Navigation
        {"zoom_in", "Navigation", "Zoom In", "Ctrl+=", "Ctrl+="},
        {"zoom_out", "Navigation", "Zoom Out", "Ctrl+-", "Ctrl+-"},
        {"zoom_reset", "Navigation", "Reset Zoom", "Ctrl+0", "Ctrl+0"},
        {"zoom_fit", "Navigation", "Fit to Content", "Shift+1", "Shift+1"},

        // Edit
        {"edit_undo", "Edit", "Undo", "Ctrl+Z", "Ctrl+Z"},
        {"edit_redo", "Edit", "Redo", "Ctrl+Shift+Z", "Ctrl+Shift+Z"},
        {"edit_copy_image", "Edit", "Copy Snapshot to Clipboard", "Ctrl+Shift+C", "Ctrl+Shift+C"},
        {"edit_paste", "Edit", "Paste Screenshot / Image", "Ctrl+V", "Ctrl+V"},
        {"edit_delete", "Edit", "Delete Selected", "Delete", "Delete"},
        {"edit_duplicate", "Edit", "Duplicate", "Ctrl+D", "Ctrl+D"},
        {"edit_select_all", "Edit", "Select All", "Ctrl+A", "Ctrl+A"},
        {"edit_group", "Edit", "Group Selected", "Ctrl+G", "Ctrl+G"},
        {"edit_ungroup", "Edit", "Ungroup Selected", "Ctrl+Shift+G", "Ctrl+Shift+G"},
        {"edit_bring_forward", "Edit", "Bring Forward", "]", "]"},
        {"edit_send_backward", "Edit", "Send Backward", "[", "["},
        {"edit_bring_to_front", "Edit", "Bring to Front", "Shift+]", "Shift+]"},
        {"edit_send_to_back", "Edit", "Send to Back", "Shift+[", "Shift+["},

        // File
        {"file_new", "File", "New Document", "Ctrl+N", "Ctrl+N"},
        {"file_open", "File", "Open Document", "Ctrl+O", "Ctrl+O"},
        {"file_save", "File", "Save Document", "Ctrl+S", "Ctrl+S"},
        {"file_save_as", "File", "Save As...", "Ctrl+Shift+S", "Ctrl+Shift+S"},
        {"file_export", "File", "Export...", "Ctrl+E", "Ctrl+E"},

        // Pen Sizes
        {"pen_size_1", "Drawing", "Pen Size 1 (3px)", "1", "1"},
        {"pen_size_2", "Drawing", "Pen Size 2 (4px)", "2", "2"},
        {"pen_size_3", "Drawing", "Pen Size 3 (5px)", "3", "3"},
        {"pen_size_4", "Drawing", "Pen Size 4 (6px)", "4", "4"},

        // View & Cheatsheet
        {"view_zen", "View", "Zen Mode", "Z", "Z"},
        {"palette_cycle", "View", "Cycle Color", "Tab", "Tab"},
        {"toggle_menu", "View", "Toggle Menu", "Ctrl+M", "Ctrl+M"},
        {"toggle_grid", "View", "Toggle Grid", "Ctrl+'", "Ctrl+'"},
        {"show_shortcuts", "Help", "Shortcuts Cheatsheet", "?", "?"}
    };
}

int ShortcutManager::findIndex(const QString &id) const {
    for (int i = 0; i < m_items.size(); ++i) {
        if (m_items[i].id == id)
            return i;
    }
    return -1;
}

QVariantList ShortcutManager::getShortcuts() const {
    QVariantList list;
    for (const auto &item : m_items) {
        QVariantMap map;
        map["id"] = item.id;
        map["category"] = item.category;
        map["name"] = item.name;
        map["defaultKey"] = item.defaultKey;
        map["currentKey"] = item.currentKey;
        map["isModified"] = item.isModified();
        list.append(map);
    }
    return list;
}

QString ShortcutManager::getShortcut(const QString &id) const {
    int idx = findIndex(id);
    if (idx >= 0)
        return m_items[idx].currentKey;
    return QString();
}

QString ShortcutManager::checkConflict(const QString &id, const QString &key) const {
    if (key.trimmed().isEmpty())
        return QString();

    QKeySequence querySeq(key);
    for (const auto &item : m_items) {
        if (item.id != id) {
            QKeySequence itemSeq(item.currentKey);
            if (itemSeq == querySeq) {
                return item.name;
            }
        }
    }
    return QString();
}

bool ShortcutManager::setShortcut(const QString &id, const QString &key) {
    int idx = findIndex(id);
    if (idx < 0)
        return false;

    m_items[idx].currentKey = key.trimmed();
    save();
    emit shortcutsChanged();
    return true;
}

void ShortcutManager::resetShortcut(const QString &id) {
    int idx = findIndex(id);
    if (idx < 0)
        return;

    m_items[idx].currentKey = m_items[idx].defaultKey;
    save();
    emit shortcutsChanged();
}

void ShortcutManager::resetAll() {
    for (auto &item : m_items) {
        item.currentKey = item.defaultKey;
    }
    save();
    emit shortcutsChanged();
}

void ShortcutManager::load() {
    QFile file(configFilePath());
    if (!file.open(QIODevice::ReadOnly))
        return;

    QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
    if (!doc.isObject())
        return;

    QJsonObject obj = doc.object();
    for (auto it = obj.begin(); it != obj.end(); ++it) {
        int idx = findIndex(it.key());
        if (idx >= 0 && it.value().isString()) {
            m_items[idx].currentKey = it.value().toString();
        }
    }
}

void ShortcutManager::save() {
    QString path = configFilePath();
    QDir().mkpath(QFileInfo(path).absolutePath());

    QJsonObject obj;
    for (const auto &item : m_items) {
        if (item.isModified()) {
            obj[item.id] = item.currentKey;
        }
    }

    QFile file(path);
    if (file.open(QIODevice::WriteOnly)) {
        QJsonDocument doc(obj);
        file.write(doc.toJson(QJsonDocument::Indented));
    }
}

QString ShortcutManager::findActionForKey(int key, Qt::KeyboardModifiers modifiers) const {
    QKeySequence pressed(key | static_cast<int>(modifiers));
    for (const auto &item : m_items) {
        if (!item.currentKey.isEmpty() && QKeySequence(item.currentKey) == pressed) {
            return item.id;
        }
    }
    return QString();
}

} // namespace DrawBoard

