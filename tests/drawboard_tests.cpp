#include <QGuiApplication>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickWindow>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QTest>

#include "backend.h"
#include "canvasitem.h"
#include "document.h"
#include "exporter.h"
#include "geometry.h"
#include "history.h"
#include "shortcuts.h"
#include "theme.h"

using namespace DrawBoard;

class DrawboardTests : public QObject {
    Q_OBJECT

private slots:
    void initTestCase();

    // Geometry tests
    void testDistanceAndHitTesting();
    void testStrokeSmoothing();
    void testCardinalPointsAndConnectors();
    void testArrowHead();

    // Document tests
    void testDocumentMutations();
    void testSelectionAndTransforms();
    void testGrouping();
    void testZOrder();
    void testSerializationRoundTrip();
    void testAtomicFileSaveAndLoad();

    // History (Undo/Redo) tests
    void testHistoryStack();

    // Theme system tests
    void testThemeFamiliesAndTokens();

    // Shortcut manager tests
    void testShortcuts();

    // Exporter tests
    void testPngExport();
    void testSvgExport();
    void testPdfExport();

    // QML Offscreen smoke test
    void testQmlLoadsCleanly();
};

void DrawboardTests::initTestCase() {
    qmlRegisterType<DrawBoard::CanvasItem>("DrawBoard", 1, 0, "CanvasItem");
}

void DrawboardTests::testDistanceAndHitTesting() {
    QPointF a(0, 0);
    QPointF b(100, 0);
    QPointF p(50, 10);

    qreal dist = Geometry::distanceToSegment(p, a, b);
    QCOMPARE(dist, 10.0);

    Element rectElem;
    rectElem.type = ElementType::Rectangle;
    rectElem.bounds = QRectF(10, 10, 80, 80);
    rectElem.fillColor = QColor(Qt::red);

    QVERIFY(Geometry::hitTest(rectElem, QPointF(50, 50), 2.0));
    QVERIFY(!Geometry::hitTest(rectElem, QPointF(150, 150), 2.0));

    Element lineElem;
    lineElem.type = ElementType::Line;
    lineElem.startPoint = QPointF(0, 0);
    lineElem.endPoint = QPointF(100, 100);
    lineElem.strokeWidth = 2.0;

    QVERIFY(Geometry::hitTest(lineElem, QPointF(50, 50), 4.0));
    QVERIFY(!Geometry::hitTest(lineElem, QPointF(0, 100), 4.0));
}

void DrawboardTests::testStrokeSmoothing() {
    QVector<StrokePoint> points;
    for (int i = 0; i < 10; ++i) {
        StrokePoint pt;
        pt.x = i * 10.0;
        pt.y = (i % 2 == 0) ? 0.0 : 20.0;
        pt.pressure = 0.5;
        points.append(pt);
    }

    auto smoothed = Geometry::smoothPoints(points);
    QCOMPARE(smoothed.size(), points.size());

    QPainterPath path = Geometry::buildSmoothPath(smoothed);
    QVERIFY(!path.isEmpty());
    QCOMPARE(path.elementCount() > 5, true);
}

void DrawboardTests::testCardinalPointsAndConnectors() {
    Element rect;
    rect.type = ElementType::Rectangle;
    rect.bounds = QRectF(100, 100, 200, 100);

    QPointF top = Geometry::getCardinalPoint(rect, CardinalPoint::Top);
    QCOMPARE(top, QPointF(200, 100));

    QPointF right = Geometry::getCardinalPoint(rect, CardinalPoint::Right);
    QCOMPARE(right, QPointF(300, 150));

    QPointF bottom = Geometry::getCardinalPoint(rect, CardinalPoint::Bottom);
    QCOMPARE(bottom, QPointF(200, 200));

    QPointF left = Geometry::getCardinalPoint(rect, CardinalPoint::Left);
    QCOMPARE(left, QPointF(100, 150));

    CardinalPoint nearest = Geometry::findNearestCardinal(rect, QPointF(200, 105), 20.0);
    QCOMPARE(nearest, CardinalPoint::Top);

    auto route = Geometry::routeConnector(top, CardinalPoint::Top, QPointF(400, 50), CardinalPoint::Left);
    QVERIFY(route.size() >= 2);
}

void DrawboardTests::testArrowHead() {
    QPointF tip(100, 100);
    QPointF from(50, 100);
    QPolygonF head = Geometry::makeArrowHead(tip, from, 12.0);

    QCOMPARE(head.size(), 3);
    QCOMPARE(head[0], tip);
}

void DrawboardTests::testDocumentMutations() {
    Document doc;
    QCOMPARE(doc.elementCount(), 0);

    Element e1;
    e1.type = ElementType::Rectangle;
    e1.bounds = QRectF(0, 0, 100, 50);
    QString id1 = doc.addElement(e1);
    QVERIFY(!id1.isEmpty());
    QCOMPARE(doc.elementCount(), 1);
    QVERIFY(doc.isModified());

    Element *found = doc.findElement(id1);
    QVERIFY(found != nullptr);
    QCOMPARE(found->bounds.width(), 100.0);

    e1.id = id1;
    e1.bounds.setWidth(150.0);
    doc.updateElement(e1);
    QCOMPARE(doc.findElement(id1)->bounds.width(), 150.0);

    doc.removeElement(id1);
    QCOMPARE(doc.elementCount(), 0);
}

void DrawboardTests::testSelectionAndTransforms() {
    Document doc;
    Element e1;
    e1.type = ElementType::Rectangle;
    e1.bounds = QRectF(10, 10, 50, 50);
    QString id1 = doc.addElement(e1);

    Element e2;
    e2.type = ElementType::Ellipse;
    e2.bounds = QRectF(100, 100, 40, 40);
    QString id2 = doc.addElement(e2);

    doc.select(id1);
    QCOMPARE(doc.selectedIds().size(), 1);
    QVERIFY(doc.isSelected(id1));

    doc.select(id2, true); // add to selection
    QCOMPARE(doc.selectedIds().size(), 2);

    QRectF b = doc.selectionBounds();
    QCOMPARE(b, QRectF(10, 10, 130, 130));

    doc.moveSelected(QPointF(20, 30));
    QCOMPARE(doc.findElement(id1)->bounds.topLeft(), QPointF(30, 40));
    QCOMPARE(doc.findElement(id2)->bounds.topLeft(), QPointF(120, 130));

    auto dups = doc.duplicateSelected(QPointF(10, 10));
    QCOMPARE(dups.size(), 2);
    QCOMPARE(doc.elementCount(), 4);
}

void DrawboardTests::testGrouping() {
    Document doc;
    Element e1; e1.bounds = QRectF(0, 0, 50, 50); QString id1 = doc.addElement(e1);
    Element e2; e2.bounds = QRectF(60, 0, 50, 50); QString id2 = doc.addElement(e2);

    doc.select(id1);
    doc.select(id2, true);
    doc.groupSelected();

    QString g1 = doc.findElement(id1)->groupId;
    QString g2 = doc.findElement(id2)->groupId;
    QVERIFY(!g1.isEmpty());
    QCOMPARE(g1, g2);

    // Selecting one selects the whole group
    doc.clearSelection();
    doc.select(id1);
    QCOMPARE(doc.selectedIds().size(), 2);

    doc.ungroupSelected();
    QVERIFY(doc.findElement(id1)->groupId.isEmpty());
}

void DrawboardTests::testZOrder() {
    Document doc;
    Element e1; QString id1 = doc.addElement(e1);
    Element e2; QString id2 = doc.addElement(e2);
    Element e3; QString id3 = doc.addElement(e3);

    doc.select(id1);
    doc.bringToFront();
    QCOMPARE(doc.elements().last().id, id1);

    doc.sendToBack();
    QCOMPARE(doc.elements().first().id, id1);
}

void DrawboardTests::testSerializationRoundTrip() {
    Document doc1;
    doc1.setThemeId("everforest-dark-medium");

    Element stroke;
    stroke.type = ElementType::Stroke;
    StrokePoint p1{10, 20, 0.8, 100};
    StrokePoint p2{30, 40, 0.6, 200};
    stroke.points << p1 << p2;
    stroke.strokeColor = QColor("#d3c6aa");
    stroke.strokeWidth = 3.5;
    QString strokeId = doc1.addElement(stroke);

    Element note;
    note.type = ElementType::StickyNote;
    note.bounds = QRectF(200, 200, 180, 140);
    note.text = "Architecture diagram plan";
    note.fontSize = 14.0;
    QString noteId = doc1.addElement(note);

    QJsonObject json = doc1.toJson();

    Document doc2;
    QVERIFY(doc2.fromJson(json));
    QCOMPARE(doc2.elementCount(), 2);
    QCOMPARE(doc2.themeId(), "everforest-dark-medium");

    const Element *s = doc2.findElement(strokeId);
    QVERIFY(s != nullptr);
    QCOMPARE(s->type, ElementType::Stroke);
    QCOMPARE(s->points.size(), 2);
    QCOMPARE(s->strokeWidth, 3.5);

    const Element *n = doc2.findElement(noteId);
    QVERIFY(n != nullptr);
    QCOMPARE(n->type, ElementType::StickyNote);
    QCOMPARE(n->text, "Architecture diagram plan");
}

void DrawboardTests::testAtomicFileSaveAndLoad() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString filePath = tempDir.path() + "/test_diagram.draw";

    Document doc;
    Element rect;
    rect.type = ElementType::Rectangle;
    rect.bounds = QRectF(50, 50, 200, 120);
    doc.addElement(rect);

    QVERIFY(doc.saveToFile(filePath));
    QVERIFY(QFile::exists(filePath));
    QVERIFY(!doc.isModified());

    Document loaded;
    QVERIFY(loaded.loadFromFile(filePath));
    QCOMPARE(loaded.elementCount(), 1);
    QCOMPARE(loaded.elements().first().bounds, QRectF(50, 50, 200, 120));
}

void DrawboardTests::testHistoryStack() {
    History history(50);
    QVERIFY(!history.canUndo());
    QVERIFY(!history.canRedo());

    QVector<Element> s1;
    Element e1; e1.id = "1"; s1.append(e1);
    history.pushState("Add 1", s1, {});

    QVector<Element> s2 = s1;
    Element e2; e2.id = "2"; s2.append(e2);
    history.pushState("Add 2", s2, {});

    QVERIFY(history.canUndo());
    const HistoryState *undoState = history.undo();
    QVERIFY(undoState != nullptr);
    QCOMPARE(undoState->elements.size(), 1);

    QVERIFY(history.canRedo());
    const HistoryState *redoState = history.redo();
    QVERIFY(redoState != nullptr);
    QCOMPARE(redoState->elements.size(), 2);
}

void DrawboardTests::testThemeFamiliesAndTokens() {
    Theme theme;

    QStringList families = theme.availableFamilies();
    QVERIFY(families.contains("Gruvbox"));
    QVERIFY(families.contains("Catppuccin"));
    QVERIFY(families.contains("Everforest"));
    QVERIFY(families.contains("VS Code"));
    QVERIFY(families.contains("IntelliJ IDEA"));
    QVERIFY(families.contains("Dracula"));

    // Check Gruvbox Dark Medium tokens
    theme.setThemeId("gruvbox-dark-medium");
    QCOMPARE(theme.id(), "gruvbox-dark-medium");
    QVERIFY(QColor(theme.background()).isValid());
    QVERIFY(QColor(theme.canvas()).isValid());
    QVERIFY(QColor(theme.accent()).isValid());
    QVERIFY(QColor(theme.strokePrimary()).isValid());
    QVERIFY(QColor(theme.strokeSecondary()).isValid());
    QVERIFY(QColor(theme.strokeTertiary()).isValid());

    // Check Catppuccin Mocha tokens
    theme.setThemeId("catppuccin-mocha");
    QCOMPARE(theme.id(), "catppuccin-mocha");
    QCOMPARE(theme.canvas(), "#1e1e2e");

    // Check Dracula
    theme.setThemeId("dracula");
    QCOMPARE(theme.accent(), "#bd93f9");
}

void DrawboardTests::testShortcuts() {
    ShortcutManager sc;
    QCOMPARE(sc.getShortcut("tool_pen"), "P");
    QCOMPARE(sc.getShortcut("tool_select"), "V");
    QCOMPARE(sc.getShortcut("edit_undo"), "Ctrl+Z");

    // Conflict detection
    QString conflict = sc.checkConflict("tool_eraser", "P");
    QCOMPARE(conflict, "Freehand Pen");

    // Rebinding
    sc.setShortcut("tool_pen", "X");
    QCOMPARE(sc.getShortcut("tool_pen"), "X");

    // Reset individual
    sc.resetShortcut("tool_pen");
    QCOMPARE(sc.getShortcut("tool_pen"), "P");
}

void DrawboardTests::testPngExport() {
    QTemporaryDir tempDir;
    QString outPath = tempDir.path() + "/output.png";

    Theme theme;
    theme.setThemeId("gruvbox-dark-medium");

    Document doc;
    Element rect;
    rect.type = ElementType::Rectangle;
    rect.bounds = QRectF(10, 10, 100, 60);
    rect.strokeColor = QColor(theme.strokePrimary());
    doc.addElement(rect);

    bool ok = Exporter::exportPng(outPath, doc, theme.current(), 1.0, false);
    QVERIFY(ok);
    QVERIFY(QFile::exists(outPath));
    QVERIFY(QFileInfo(outPath).size() > 100);
}

void DrawboardTests::testSvgExport() {
    QTemporaryDir tempDir;
    QString outPath = tempDir.path() + "/output.svg";

    Theme theme;
    Document doc;
    Element arrow;
    arrow.type = ElementType::Arrow;
    arrow.startPoint = QPointF(0, 0);
    arrow.endPoint = QPointF(100, 50);
    arrow.strokeColor = QColor("#fe8019");
    doc.addElement(arrow);

    bool ok = Exporter::exportSvg(outPath, doc, theme.current(), false);
    QVERIFY(ok);
    QVERIFY(QFile::exists(outPath));

    QFile f(outPath);
    QVERIFY(f.open(QIODevice::ReadOnly));
    QByteArray content = f.readAll();
    QVERIFY(content.contains("<svg"));
}

void DrawboardTests::testPdfExport() {
    QTemporaryDir tempDir;
    QString outPath = tempDir.path() + "/output.pdf";

    Theme theme;
    Document doc;
    Element note;
    note.type = ElementType::StickyNote;
    note.bounds = QRectF(20, 20, 180, 140);
    note.text = "PDF Test Note";
    doc.addElement(note);

    bool ok = Exporter::exportPdf(outPath, doc, theme.current(), false);
    QVERIFY(ok);
    QVERIFY(QFile::exists(outPath));
    QVERIFY(QFileInfo(outPath).size() > 500);
}

void DrawboardTests::testQmlLoadsCleanly() {
    Theme theme;
    Backend backend(&theme);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty("theme", &theme);
    engine.rootContext()->setContextProperty("backend", &backend);
    engine.rootContext()->setContextProperty("shortcuts", backend.shortcuts());

    engine.load(QUrl("qrc:/Main.qml"));
    QCOMPARE(engine.rootObjects().isEmpty(), false);

    backend.openDocument("/home/smk/db/examples/sample_arch.draw");

    QQuickWindow *window = qobject_cast<QQuickWindow*>(engine.rootObjects().first());
    if (window) {
        window->resize(1280, 800);
        window->show();
        QImage grab = window->grabWindow();
        QDir().mkpath("/home/smk/db/docs");
        grab.save("/home/smk/db/docs/screenshot.png");
        QVERIFY(!grab.isNull());
    }
}

int main(int argc, char *argv[]) {
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QGuiApplication app(argc, argv);
    DrawboardTests tc;
    return QTest::qExec(&tc, argc, argv);
}
#include "drawboard_tests.moc"
