#pragma once

#include <QKeySequence>
#include <QObject>
#include <QString>
#include <QVariantList>
#include <QVariantMap>
#include <QVector>

namespace DrawBoard {

struct ShortcutItem {
    QString id;
    QString category;
    QString name;
    QString defaultKey;
    QString currentKey;

    bool isModified() const { return currentKey != defaultKey; }
};

class ShortcutManager : public QObject {
    Q_OBJECT

public:
    explicit ShortcutManager(QObject *parent = nullptr);

    void load();
    void save();

    Q_INVOKABLE QVariantList getShortcuts() const;
    Q_INVOKABLE QString getShortcut(const QString &id) const;
    Q_INVOKABLE bool setShortcut(const QString &id, const QString &key);
    Q_INVOKABLE void resetShortcut(const QString &id);
    Q_INVOKABLE void resetAll();
    Q_INVOKABLE QString checkConflict(const QString &id, const QString &key) const;

    QString findActionForKey(int key, Qt::KeyboardModifiers modifiers) const;

signals:
    void shortcutsChanged();

private:
    void initDefaults();
    int findIndex(const QString &id) const;
    QString configFilePath() const;

    QVector<ShortcutItem> m_items;
};

} // namespace DrawBoard

