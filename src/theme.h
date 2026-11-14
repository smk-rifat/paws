#pragma once

#include <QColor>
#include <QFileSystemWatcher>
#include <QHash>
#include <QObject>
#include <QProcess>
#include <QStringList>
#include <QTimer>

struct ThemePalette {
    QString id;
    QString family;
    QString variant;
    QString displayName;

    // Semantic tokens
    QString background;
    QString surface;
    QString surfaceElevated;
    QString border;
    QString borderMuted;
    QString text;
    QString textMuted;
    QString accent;
    QString accentForeground;
    QString canvas;
    QString canvasGrid;
    QString selection;
    QString selectionFill;

    // Three theme-aware primary drawing colors
    QString strokePrimary;
    QString strokeSecondary;
    QString strokeTertiary;

    // Sticky note tokens
    QString note;
    QString noteBorder;
    QString noteText;

    // Status & Feedback
    QString danger;
    QString success;
    QString warning;
    QString status;
};

class Theme : public QObject {
    Q_OBJECT

    // Semantic tokens exposed to QML
    Q_PROPERTY(QString id READ id WRITE setThemeId NOTIFY themeChanged)
    Q_PROPERTY(QString family READ family NOTIFY themeChanged)
    Q_PROPERTY(QString variant READ variant NOTIFY themeChanged)
    Q_PROPERTY(QString displayName READ displayName NOTIFY themeChanged)

    Q_PROPERTY(QString background READ background NOTIFY themeChanged)
    Q_PROPERTY(QString surface READ surface NOTIFY themeChanged)
    Q_PROPERTY(QString surfaceElevated READ surfaceElevated NOTIFY themeChanged)
    Q_PROPERTY(QString border READ border NOTIFY themeChanged)
    Q_PROPERTY(QString borderMuted READ borderMuted NOTIFY themeChanged)
    Q_PROPERTY(QString text READ text NOTIFY themeChanged)
    Q_PROPERTY(QString textMuted READ textMuted NOTIFY themeChanged)
    Q_PROPERTY(QString accent READ accent NOTIFY themeChanged)
    Q_PROPERTY(QString accentForeground READ accentForeground NOTIFY themeChanged)
    Q_PROPERTY(QString canvas READ canvas NOTIFY themeChanged)
    Q_PROPERTY(QString canvasGrid READ canvasGrid NOTIFY themeChanged)
    Q_PROPERTY(QString selection READ selection NOTIFY themeChanged)
    Q_PROPERTY(QString selectionFill READ selectionFill NOTIFY themeChanged)

    Q_PROPERTY(QString strokePrimary READ strokePrimary NOTIFY themeChanged)
    Q_PROPERTY(QString strokeSecondary READ strokeSecondary NOTIFY themeChanged)
    Q_PROPERTY(QString strokeTertiary READ strokeTertiary NOTIFY themeChanged)

    Q_PROPERTY(QString note READ note NOTIFY themeChanged)
    Q_PROPERTY(QString noteBorder READ noteBorder NOTIFY themeChanged)
    Q_PROPERTY(QString noteText READ noteText NOTIFY themeChanged)

    Q_PROPERTY(QString danger READ danger NOTIFY themeChanged)
    Q_PROPERTY(QString success READ success NOTIFY themeChanged)
    Q_PROPERTY(QString warning READ warning NOTIFY themeChanged)
    Q_PROPERTY(QString status READ status NOTIFY themeChanged)

    Q_PROPERTY(int radius READ radius NOTIFY roundingChanged)
    Q_PROPERTY(QStringList availableThemes READ availableThemes CONSTANT)
    Q_PROPERTY(QStringList availableFamilies READ availableFamilies CONSTANT)

public:
    explicit Theme(const QString &omarchyDirectory = {}, QObject *parent = nullptr, const QString &hyprctl = {});
    ~Theme() override;

    QString id() const { return m_current.id; }
    QString family() const { return m_current.family; }
    QString variant() const { return m_current.variant; }
    QString displayName() const { return m_current.displayName; }

    QString background() const { return m_current.background; }
    QString surface() const { return m_current.surface; }
    QString surfaceElevated() const { return m_current.surfaceElevated; }
    QString border() const { return m_current.border; }
    QString borderMuted() const { return m_current.borderMuted; }
    QString text() const { return m_current.text; }
    QString textMuted() const { return m_current.textMuted; }
    QString accent() const { return m_current.accent; }
    QString accentForeground() const { return m_current.accentForeground; }
    QString canvas() const { return m_current.canvas; }
    QString canvasGrid() const { return m_current.canvasGrid; }
    QString selection() const { return m_current.selection; }
    QString selectionFill() const { return m_current.selectionFill; }

    QString strokePrimary() const { return m_current.strokePrimary; }
    QString strokeSecondary() const { return m_current.strokeSecondary; }
    QString strokeTertiary() const { return m_current.strokeTertiary; }

    QString note() const { return m_current.note; }
    QString noteBorder() const { return m_current.noteBorder; }
    QString noteText() const { return m_current.noteText; }

    QString danger() const { return m_current.danger; }
    QString success() const { return m_current.success; }
    QString warning() const { return m_current.warning; }
    QString status() const { return m_current.status; }

    int radius() const { return m_radius; }

    QStringList availableThemes() const;
    QStringList availableFamilies() const;

    Q_INVOKABLE QStringList getVariantsForFamily(const QString &family) const;
    Q_INVOKABLE QString getThemeId(const QString &family, const QString &variant) const;
    Q_INVOKABLE void setThemeId(const QString &id);

    const ThemePalette &current() const { return m_current; }
    const ThemePalette &getTheme(const QString &id) const;

signals:
    void themeChanged();
    void roundingChanged();

private:
    void initPalettes();
    void reloadOmarchyTheme();
    void refreshRounding();

    ThemePalette m_current;
    QHash<QString, ThemePalette> m_palettes;
    QString m_omarchyDirectory;
    QFileSystemWatcher m_watcher;
    QTimer m_debounce;
    QString m_hyprctl;
    int m_radius = 4;
    QProcess m_roundingProcess;
    QTimer m_roundingPoll;
    QTimer m_roundingTimeout;
};
