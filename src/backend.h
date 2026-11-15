#pragma once
#include "canvasitem.h"
#include "document.h"
#include "history.h"
#include "shortcuts.h"
#include "theme.h"
#include <QColor>
#include <QObject>
#include <QStringList>
#include <QTimer>
#include <QVariantList>

namespace DrawBoard {

class Backend : public QObject {
    Q_OBJECT

    Q_PROPERTY(QString tool READ tool WRITE setTool NOTIFY toolChanged)
    Q_PROPERTY(qreal penSize READ penSize WRITE setPenSize NOTIFY penSizeChanged)
    Q_PROPERTY(int activeColorIndex READ activeColorIndex WRITE setActiveColorIndex NOTIFY activeColorIndexChanged)
    Q_PROPERTY(QStringList colorPalette READ colorPalette NOTIFY colorPaletteChanged)
    Q_PROPERTY(QColor activeColor READ activeColor NOTIFY activeColorChanged)
    Q_PROPERTY(bool canUndo READ canUndo NOTIFY historyChanged)
    Q_PROPERTY(bool canRedo READ canRedo NOTIFY historyChanged)
    Q_PROPERTY(QString documentTitle READ documentTitle NOTIFY documentChanged)
    Q_PROPERTY(QString filePath READ filePath NOTIFY documentChanged)
    Q_PROPERTY(bool isModified READ isModified NOTIFY modifiedChanged)
    Q_PROPERTY(int selectedCount READ selectedCount NOTIFY selectionChanged)
    Q_PROPERTY(QString statusText READ statusText NOTIFY statusTextChanged)
    Q_PROPERTY(QString zoomText READ zoomText NOTIFY zoomTextChanged)
    Q_PROPERTY(QStringList recentFiles READ recentFiles NOTIFY recentFilesChanged)
    Q_PROPERTY(bool snapToGrid READ snapToGrid WRITE setSnapToGrid NOTIFY snapToGridChanged)
    Q_PROPERTY(int gridStyle READ gridStyle WRITE setGridStyle NOTIFY gridStyleChanged)
    Q_PROPERTY(qreal gridSize READ gridSize WRITE setGridSize NOTIFY gridSizeChanged)
    Q_PROPERTY(int windowWidth READ windowWidth NOTIFY windowGeometryChanged)
    Q_PROPERTY(int windowHeight READ windowHeight NOTIFY windowGeometryChanged)
    Q_PROPERTY(int windowX READ windowX NOTIFY windowGeometryChanged)
    Q_PROPERTY(int windowY READ windowY NOTIFY windowGeometryChanged)
    Q_PROPERTY(bool windowMaximized READ windowMaximized NOTIFY windowGeometryChanged)

public:
    explicit Backend(Theme *theme, QObject *parent = nullptr);
    ~Backend() override;

    Q_INVOKABLE void registerCanvas(CanvasItem *canvas);

    Document *document() { return &m_doc; }
    History *history() { return &m_history; }
    Theme *theme() { return m_theme; }
    ShortcutManager *shortcuts() { return &m_shortcuts; }

    QString tool() const { return m_tool; }
    void setTool(const QString &tool);

    qreal penSize() const { return m_penSize; }
    void setPenSize(qreal size);

    int activeColorIndex() const { return m_activeColorIndex; }
    void setActiveColorIndex(int index);

    QStringList colorPalette() const;
    QColor activeColor() const;

    bool canUndo() const { return m_history.canUndo(); }
    bool canRedo() const { return m_history.canRedo(); }

    QString documentTitle() const { return m_doc.title(); }
    QString filePath() const { return m_doc.filePath(); }
    bool isModified() const { return m_doc.isModified(); }
    int selectedCount() const { return m_doc.selectedIds().size(); }

    QString statusText() const { return m_statusText; }
    QString zoomText() const { return m_zoomText; }
    QStringList recentFiles() const { return m_recentFiles; }

    bool snapToGrid() const { return m_snapToGrid; }
    void setSnapToGrid(bool snap);

    int gridStyle() const { return m_gridStyle; }
    void setGridStyle(int style);

    qreal gridSize() const { return m_gridSize; }
    void setGridSize(qreal size);

    int windowWidth() const { return m_windowWidth; }
    int windowHeight() const { return m_windowHeight; }
    int windowX() const { return m_windowX; }
    int windowY() const { return m_windowY; }
    bool windowMaximized() const { return m_windowMaximized; }

    QString settingsFilePath() const;
    void setSettingsFilePath(const QString &path) { m_customSettingsPath = path; }

    // QML Invokables
    Q_INVOKABLE void undo();
    Q_INVOKABLE void redo();
    Q_INVOKABLE void deleteSelected();
    Q_INVOKABLE void duplicateSelected();
    Q_INVOKABLE void selectAll();
    Q_INVOKABLE void clearSelection();
    Q_INVOKABLE void groupSelected();
    Q_INVOKABLE void ungroupSelected();
    Q_INVOKABLE void bringForward();
    Q_INVOKABLE void sendBackward();
    Q_INVOKABLE void bringToFront();
    Q_INVOKABLE void sendToBack();

    Q_INVOKABLE void newDocument();
    Q_INVOKABLE bool openDocument(const QString &path);
    Q_INVOKABLE bool saveDocument(const QString &path = {});
    Q_INVOKABLE bool exportFile(const QString &path, const QString &format, qreal scale = 2.0, bool transparent = false);

    Q_INVOKABLE void createText(qreal worldX, qreal worldY, const QString &text);
    Q_INVOKABLE void updateText(const QString &id, const QString &text);
    Q_INVOKABLE bool handleKeyPress(int key, int modifiers);
    Q_INVOKABLE void resetView();
    Q_INVOKABLE void zoomIn();
    Q_INVOKABLE void zoomOut();
    Q_INVOKABLE void zoomToFit();

    Q_INVOKABLE void checkAutosaveRecovery();
    Q_INVOKABLE void saveWindowGeometry(int x, int y, int width, int height, bool maximized);
    Q_INVOKABLE void loadSettings(const QString &customPath = QString());
    Q_INVOKABLE void saveSettings(const QString &customPath = QString());

    Q_INVOKABLE bool pasteFromClipboard();
    Q_INVOKABLE bool copySelectionToClipboard();
    Q_INVOKABLE void cycleColor(bool forward = true);
    Q_INVOKABLE void cyclePenSize(bool forward = true);
    Q_INVOKABLE bool insertImageFromFile(const QString &filePath, const QPointF &preferredWorldPos = QPointF());
    bool insertImage(const QImage &image, const QString &sourceName = "Screenshot", const QPointF &preferredWorldPos = QPointF());
    bool insertPdf(const QString &pdfPath, const QPointF &preferredWorldPos = QPointF());

signals:
    void toolChanged();
    void penSizeChanged();
    void activeColorIndexChanged();
    void colorPaletteChanged();
    void activeColorChanged();
    void historyChanged();
    void documentChanged();
    void modifiedChanged();
    void selectionChanged();
    void statusTextChanged();
    void zoomTextChanged();
    void recentFilesChanged();
    void snapToGridChanged();
    void gridStyleChanged();
    void gridSizeChanged();
    void windowGeometryChanged();
    void showExportDialog();
    void showShortcutsDialog();
    void showSettingsDialog();
    void showOpenDialog();
    void showImportImageDialog();
    void showDotMenu();
    void toastRequested(const QString &message);
    void toggleZenModeRequested();

private:
    void updateStatus();
    void syncActiveColors();
    void addRecentFile(const QString &path);
    void loadRecentFiles();
    void saveRecentFiles();
    void performAutosave();

    Theme *m_theme = nullptr;
    Document m_doc;
    History m_history;
    ShortcutManager m_shortcuts;
    CanvasItem *m_canvas = nullptr;

    QString m_tool = "pen";
    qreal m_penSize = 3.0;
    int m_activeColorIndex = 0;
    bool m_snapToGrid = false;
    int m_gridStyle = 0; // Dots
    qreal m_gridSize = 20.0;

    int m_windowWidth = 1280;
    int m_windowHeight = 800;
    int m_windowX = -1;
    int m_windowY = -1;
    bool m_windowMaximized = false;
    bool m_loadingSettings = false;
    QString m_customSettingsPath;

    QString m_statusText = "[PEN] 3px | 100% | 0 items";
    QString m_zoomText = "100%";
    QStringList m_recentFiles;

    QTimer m_autosaveTimer;
    qreal m_cursorWorldX = 0.0;
    qreal m_cursorWorldY = 0.0;
};

} // namespace DrawBoard
