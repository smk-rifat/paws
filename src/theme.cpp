#include "theme.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QStandardPaths>
#include <cmath>

static QString computeAccentForeground(const QString &accentHex) {
    QColor c(accentHex);
    auto linear = [](double v) { return v <= .04045 ? v / 12.92 : std::pow((v + .055) / 1.055, 2.4); };
    const double lum = .2126 * linear(c.redF()) + .7152 * linear(c.greenF()) + .0722 * linear(c.blueF());
    return lum > .179 ? "#1d2021" : "#fbf1c7";
}

Theme::Theme(const QString &omarchyDirectory, QObject *parent, const QString &hyprctl)
    : QObject(parent),
      m_omarchyDirectory(omarchyDirectory.isEmpty() ? QDir::homePath() + "/.local/state/omarchy/current" : omarchyDirectory) {
    initPalettes();

    // Default to Gruvbox Dark Medium
    setThemeId("gruvbox-dark-medium");

    m_debounce.setSingleShot(true);
    m_debounce.setInterval(80);
    connect(&m_watcher, &QFileSystemWatcher::directoryChanged, this, [this] { m_debounce.start(); });
    connect(&m_watcher, &QFileSystemWatcher::fileChanged, this, [this] { m_debounce.start(); });
    connect(&m_debounce, &QTimer::timeout, this, &Theme::reloadOmarchyTheme);

    // Watch for Hyprland rounding
    m_hyprctl = !hyprctl.isEmpty() ? hyprctl
              : (qEnvironmentVariableIsSet("HYPRLAND_INSTANCE_SIGNATURE")
                 ? QStandardPaths::findExecutable("hyprctl") : QString());

    m_roundingTimeout.setSingleShot(true);
    m_roundingTimeout.setInterval(500);
    connect(&m_roundingTimeout, &QTimer::timeout, &m_roundingProcess, &QProcess::kill);
    connect(&m_roundingProcess, &QProcess::errorOccurred, this, [this] { m_roundingTimeout.stop(); });
    connect(&m_roundingProcess, &QProcess::finished, this, [this](int code, QProcess::ExitStatus status) {
        m_roundingTimeout.stop();
        if (code != 0 || status != QProcess::NormalExit)
            return;
        const auto val = QJsonDocument::fromJson(m_roundingProcess.readAllStandardOutput()).object().value("int");
        if (val.isDouble() && val.toInt(-1) >= 0) {
            int newRadius = val.toInt();
            if (newRadius != m_radius) {
                m_radius = newRadius;
                emit roundingChanged();
            }
        }
    });

    m_roundingPoll.setInterval(2000);
    connect(&m_roundingPoll, &QTimer::timeout, this, &Theme::refreshRounding);
    if (!m_hyprctl.isEmpty()) {
        m_roundingPoll.start();
        refreshRounding();
    }

    reloadOmarchyTheme();
}

Theme::~Theme() {
    if (m_roundingProcess.state() != QProcess::NotRunning) {
        m_roundingProcess.kill();
        m_roundingProcess.waitForFinished(500);
    }
}

void Theme::refreshRounding() {
    if (m_hyprctl.isEmpty() || m_roundingProcess.state() != QProcess::NotRunning)
        return;
    m_roundingProcess.start(m_hyprctl, {"-j", "getoption", "decoration:rounding"});
    m_roundingTimeout.start();
}

void Theme::reloadOmarchyTheme() {
    refreshRounding();
    const auto paths = m_watcher.files() + m_watcher.directories();
    if (!paths.isEmpty())
        m_watcher.removePaths(paths);

    QString ancestor = m_omarchyDirectory;
    while (!QFileInfo::exists(ancestor) && ancestor != "/" && !ancestor.isEmpty()) {
        ancestor = QFileInfo(ancestor).absolutePath();
    }
    QStringList candidates{ancestor, QFileInfo(ancestor).absolutePath(), m_omarchyDirectory,
                           m_omarchyDirectory + "/theme", m_omarchyDirectory + "/theme/colors.toml"};
    candidates.removeDuplicates();
    for (const auto &path : candidates) {
        if (QFileInfo::exists(path))
            m_watcher.addPath(path);
    }
}

void Theme::initPalettes() {
    auto add = [this](ThemePalette p) {
        if (p.accentForeground.isEmpty())
            p.accentForeground = computeAccentForeground(p.accent);
        m_palettes.insert(p.id, p);
    };

    // ==========================================
    // 1. Gruvbox Family
    // ==========================================
    add({
        "gruvbox-dark-medium", "Gruvbox", "Dark Medium", "Gruvbox Dark Medium",
        "#282828", "#32302f", "#3c3836", "#504945", "#3c3836",
        "#ebdbb2", "#a89984", "#d79921", "",
        "#282828", "#3c3836", "#83a598", "#83a59826",
        "#ebdbb2", "#fe8019", "#8ec07c",
        "#32302f", "#d79921", "#ebdbb2",
        "#fb4934", "#b8bb26", "#fabd2f", "#a89984"
    });

    add({
        "gruvbox-dark-hard", "Gruvbox", "Dark Hard", "Gruvbox Dark Hard",
        "#1d2021", "#282828", "#32302f", "#504945", "#3c3836",
        "#ebdbb2", "#928374", "#fabd2f", "",
        "#1d2021", "#32302f", "#83a598", "#83a59826",
        "#ebdbb2", "#fe8019", "#b8bb26",
        "#282828", "#fabd2f", "#ebdbb2",
        "#fb4934", "#b8bb26", "#fabd2f", "#928374"
    });

    add({
        "gruvbox-dark-soft", "Gruvbox", "Dark Soft", "Gruvbox Dark Soft",
        "#32302f", "#3c3836", "#504945", "#665c54", "#3c3836",
        "#ebdbb2", "#bdae93", "#d79921", "",
        "#32302f", "#3c3836", "#83a598", "#83a59826",
        "#ebdbb2", "#fabd2f", "#8ec07c",
        "#3c3836", "#d79921", "#ebdbb2",
        "#fb4934", "#b8bb26", "#fabd2f", "#bdae93"
    });

    // ==========================================
    // 2. Catppuccin Family
    // ==========================================
    add({
        "catppuccin-mocha", "Catppuccin", "Mocha", "Catppuccin Mocha",
        "#1e1e2e", "#181825", "#313244", "#45475a", "#313244",
        "#cdd6f4", "#a6adc8", "#89b4fa", "",
        "#1e1e2e", "#313244", "#89b4fa", "#89b4fa26",
        "#cdd6f4", "#f38ba8", "#a6e3a1",
        "#181825", "#89b4fa", "#cdd6f4",
        "#f38ba8", "#a6e3a1", "#f9e2af", "#a6adc8"
    });

    add({
        "catppuccin-macchiato", "Catppuccin", "Macchiato", "Catppuccin Macchiato",
        "#24273a", "#1e2030", "#363a4f", "#494d64", "#363a4f",
        "#cad3f5", "#a5adcb", "#8aadf4", "",
        "#24273a", "#363a4f", "#8aadf4", "#8aadf426",
        "#cad3f5", "#ed8796", "#a6da95",
        "#1e2030", "#8aadf4", "#cad3f5",
        "#ed8796", "#a6da95", "#eed49f", "#a5adcb"
    });

    add({
        "catppuccin-frappe", "Catppuccin", "Frappe", "Catppuccin Frappe",
        "#303446", "#292c3c", "#414559", "#51576d", "#414559",
        "#c6d0f5", "#a5adce", "#8caaee", "",
        "#303446", "#414559", "#8caaee", "#8caaee26",
        "#c6d0f5", "#e78284", "#a6d189",
        "#292c3c", "#8caaee", "#c6d0f5",
        "#e78284", "#a6d189", "#e5c890", "#a5adce"
    });

    add({
        "catppuccin-latte", "Catppuccin", "Latte", "Catppuccin Latte",
        "#eff1f5", "#e6e9ef", "#dce0e8", "#ccd0da", "#dce0e8",
        "#4c4f69", "#6c6f85", "#1e66f5", "",
        "#eff1f5", "#dce0e8", "#1e66f5", "#1e66f526",
        "#4c4f69", "#d20f39", "#40a02b",
        "#e6e9ef", "#1e66f5", "#4c4f69",
        "#d20f39", "#40a02b", "#df8e1d", "#6c6f85"
    });

    // ==========================================
    // 3. Everforest Family
    // ==========================================
    add({
        "everforest-dark-medium", "Everforest", "Dark Medium", "Everforest Dark Medium",
        "#2d353b", "#272e33", "#3d484d", "#4f5b58", "#3d484d",
        "#d3c6aa", "#859289", "#a7c080", "",
        "#2d353b", "#3d484d", "#a7c080", "#a7c08026",
        "#d3c6aa", "#e67e80", "#a7c080",
        "#272e33", "#a7c080", "#d3c6aa",
        "#e67e80", "#a7c080", "#dbbc7f", "#859289"
    });

    add({
        "everforest-dark-hard", "Everforest", "Dark Hard", "Everforest Dark Hard",
        "#272e33", "#1e2326", "#374145", "#475258", "#374145",
        "#d3c6aa", "#7a8478", "#a7c080", "",
        "#272e33", "#374145", "#a7c080", "#a7c08026",
        "#d3c6aa", "#e67e80", "#7fbbb3",
        "#1e2326", "#a7c080", "#d3c6aa",
        "#e67e80", "#a7c080", "#dbbc7f", "#7a8478"
    });

    add({
        "everforest-dark-soft", "Everforest", "Dark Soft", "Everforest Dark Soft",
        "#343f44", "#2d353b", "#434f55", "#546062", "#434f55",
        "#d3c6aa", "#8e998a", "#a7c080", "",
        "#343f44", "#434f55", "#a7c080", "#a7c08026",
        "#d3c6aa", "#e67e80", "#d699b6",
        "#2d353b", "#a7c080", "#d3c6aa",
        "#e67e80", "#a7c080", "#dbbc7f", "#8e998a"
    });

    // ==========================================
    // 4. VS Code Family
    // ==========================================
    add({
        "vscode-dark-plus", "VS Code", "Dark+", "VS Code Dark+",
        "#1e1e1e", "#252526", "#2d2d2d", "#3c3c3c", "#2d2d2d",
        "#d4d4d4", "#858585", "#007acc", "",
        "#1e1e1e", "#2d2d2d", "#007acc", "#007acc26",
        "#d4d4d4", "#4ec9b0", "#ce9178",
        "#252526", "#007acc", "#d4d4d4",
        "#f14c4c", "#89d185", "#cca700", "#858585"
    });

    add({
        "vscode-dark-modern", "VS Code", "Dark Modern", "VS Code Dark Modern",
        "#1f1f1f", "#282828", "#333333", "#383838", "#333333",
        "#cccccc", "#999999", "#0078d4", "",
        "#1f1f1f", "#333333", "#0078d4", "#0078d426",
        "#cccccc", "#4fc1ff", "#9cdcfe",
        "#282828", "#0078d4", "#cccccc",
        "#f85149", "#7ee787", "#d29922", "#999999"
    });

    // ==========================================
    // 5. IntelliJ IDEA Family
    // ==========================================
    add({
        "idea-darcula", "IntelliJ IDEA", "Darcula", "IntelliJ Darcula",
        "#2b2b2b", "#313335", "#3c3f41", "#4e5254", "#3c3f41",
        "#a9b7c6", "#808080", "#4a88c7", "",
        "#2b2b2b", "#3c3f41", "#4a88c7", "#4a88c726",
        "#a9b7c6", "#cc7832", "#6a8759",
        "#313335", "#4a88c7", "#a9b7c6",
        "#bc3f3c", "#629755", "#bbb529", "#808080"
    });

    add({
        "idea-dark-island", "IntelliJ IDEA", "Dark Island", "IntelliJ Dark Island",
        "#1e1f22", "#2b2d30", "#393b40", "#4e5157", "#393b40",
        "#bcbec4", "#707278", "#3574f0", "",
        "#1e1f22", "#393b40", "#3574f0", "#3574f026",
        "#bcbec4", "#56a8f5", "#f98b5c",
        "#2b2d30", "#3574f0", "#bcbec4",
        "#fa6675", "#59a869", "#e0ad41", "#707278"
    });

    // ==========================================
    // 6. Dracula Family
    // ==========================================
    add({
        "dracula", "Dracula", "Standard", "Dracula",
        "#282a36", "#21222c", "#383a59", "#44475a", "#383a59",
        "#f8f8f2", "#6272a4", "#bd93f9", "",
        "#282a36", "#383a59", "#bd93f9", "#bd93f926",
        "#f8f8f2", "#ff79c6", "#50fa7b",
        "#21222c", "#bd93f9", "#f8f8f2",
        "#ff5555", "#50fa7b", "#f1fa8c", "#6272a4"
    });
}

QStringList Theme::availableThemes() const {
    return m_palettes.keys();
}

QStringList Theme::availableFamilies() const {
    QStringList families;
    for (const auto &p : m_palettes) {
        if (!families.contains(p.family))
            families.append(p.family);
    }
    families.sort();
    return families;
}

QStringList Theme::getVariantsForFamily(const QString &family) const {
    QStringList variants;
    for (const auto &p : m_palettes) {
        if (p.family == family)
            variants.append(p.variant);
    }
    variants.sort();
    return variants;
}

QString Theme::getThemeId(const QString &family, const QString &variant) const {
    for (const auto &p : m_palettes) {
        if (p.family == family && p.variant == variant)
            return p.id;
    }
    return QString();
}

const ThemePalette &Theme::getTheme(const QString &id) const {
    auto it = m_palettes.find(id);
    if (it != m_palettes.end())
        return it.value();
    return m_current;
}

void Theme::setThemeId(const QString &id) {
    auto it = m_palettes.find(id);
    if (it == m_palettes.end())
        return;

    m_current = it.value();
    emit themeChanged();
}
