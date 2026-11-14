#include <QCommandLineOption>
#include <QCommandLineParser>
#include <QGuiApplication>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

#include "backend.h"
#include "canvasitem.h"
#include "exporter.h"
#include "theme.h"

int main(int argc, char *argv[]) {
    // If CLI headless commands or no display server available, run offscreen cleanly
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "--export") == 0 || strcmp(argv[i], "-e") == 0 ||
            strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0 ||
            strcmp(argv[i], "--version") == 0 || strcmp(argv[i], "-v") == 0) {
            qputenv("QT_QPA_PLATFORM", "offscreen");
            qputenv("QT_QPA_PLATFORMTHEME", "");
            break;
        }
    }

    if (!qEnvironmentVariableIsSet("WAYLAND_DISPLAY") && !qEnvironmentVariableIsSet("DISPLAY")) {
        qputenv("QT_QPA_PLATFORM", "offscreen");
        qputenv("QT_QPA_PLATFORMTHEME", "");
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName("drawboard");
    app.setApplicationDisplayName("Drawboard");
    app.setApplicationVersion("0.1.0");
    app.setDesktopFileName("drawboard");
    app.setWindowIcon(QIcon::fromTheme("drawboard"));

    QCommandLineParser parser;
    parser.setApplicationDescription("Neovim for visual thinking - Linux-first personal drawing & diagramming");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption exportOption(QStringList() << "e" << "export",
                                    "Export input file to output image (PNG, SVG, PDF)",
                                    "output_file");
    parser.addOption(exportOption);
    parser.addPositionalArgument("file", "Document file to open (.draw)");
    parser.process(app);

    Theme theme;
    DrawBoard::Backend backend(&theme);

    // Headless export mode
    if (parser.isSet(exportOption)) {
        const QStringList positional = parser.positionalArguments();
        if (positional.isEmpty()) {
            qWarning("Error: No input file specified for export.");
            return 1;
        }
        QString inputFile = positional.first();
        QString outputFile = parser.value(exportOption);

        if (!backend.openDocument(inputFile)) {
            qWarning("Error: Failed to open %s", qPrintable(inputFile));
            return 1;
        }

        QString format = "png";
        if (outputFile.endsWith(".svg", Qt::CaseInsensitive)) format = "svg";
        else if (outputFile.endsWith(".pdf", Qt::CaseInsensitive)) format = "pdf";

        bool ok = backend.exportFile(outputFile, format, 2.0, false);
        if (ok) {
            printf("Successfully exported %s to %s\n", qPrintable(inputFile), qPrintable(outputFile));
            return 0;
        } else {
            qWarning("Error: Failed to export to %s", qPrintable(outputFile));
            return 1;
        }
    }

    // Interactive GUI mode
    QQuickStyle::setStyle("Material");
    qmlRegisterType<DrawBoard::CanvasItem>("DrawBoard", 1, 0, "CanvasItem");

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("shortcuts", backend.shortcuts());

    engine.load(QUrl("qrc:/Main.qml"));
    if (engine.rootObjects().isEmpty())
        return 1;

    // Check positional argument to open file
    const QStringList args = parser.positionalArguments();
    if (!args.isEmpty()) {
        backend.openDocument(args.first());
    } else {
        backend.checkAutosaveRecovery();
    }

    return app.exec();
}
