#include <QClipboard>
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
    void testEraserWorkflow();
    void testTextWorkflow();

    // Theme system tests
    void testThemeFamiliesAndTokens();

    // Shortcut manager tests
    void testShortcuts();
    void testBackendKeyPressHandling();
    void testCanvasItemMultiSelectAndMove();
    void testSettingsPersistence();
    void testColorPaletteAndPenSizes();
    void testImageAndScreenshotWorkflow();
    void testLaserPointer();
    void testHighlighterAndHoldToStraighten();
    void testDirectClipboardSnapshotAndCycling();

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

    // Single-point stroke (pen dot) hit test
    Element dotElem;
    dotElem.type = ElementType::Stroke;
    dotElem.strokeWidth = 4.0;
    dotElem.points << StrokePoint{60, 60, 0.5, 0};
    dotElem.bounds = Geometry::calculateStrokeBounds(dotElem.points, dotElem.strokeWidth);
    QVERIFY(Geometry::hitTest(dotElem, QPointF(60, 60), 2.0));
    QVERIFY(Geometry::hitTest(dotElem, QPointF(62, 62), 2.0));
    QVERIFY(!Geometry::hitTest(dotElem, QPointF(100, 100), 2.0));
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

void DrawboardTests::testEraserWorkflow() {
    Document doc;
    History history(50);
    history.pushState("Initial", doc.elements(), doc.selectedIds());

    // Create 3 elements
    Element r1;
    r1.id = "r1";
    r1.type = ElementType::Rectangle;
    r1.bounds = QRectF(10, 10, 50, 50);
    r1.fillColor = QColor(Qt::blue);
    doc.addElement(r1);

    Element r2;
    r2.id = "r2";
    r2.type = ElementType::Rectangle;
    r2.bounds = QRectF(100, 10, 50, 50);
    r2.fillColor = QColor(Qt::green);
    doc.addElement(r2);

    Element r3;
    r3.id = "r3";
    r3.type = ElementType::Rectangle;
    r3.bounds = QRectF(200, 10, 50, 50);
    r3.fillColor = QColor(Qt::red);
    doc.addElement(r3);

    QCOMPARE(doc.elements().size(), 3);
    history.pushState("Add Shapes", doc.elements(), doc.selectedIds());

    // Simulate eraser sweep across r1 and r2 (from 0,35 to 160,35)
    QPointF start(0, 35);
    QPointF end(160, 35);
    qreal dist = Geometry::distance(start, end);
    int steps = std::max(1, static_cast<int>(std::ceil(dist / 4.0)));

    QSet<QString> marked;
    for (int s = 0; s <= steps; ++s) {
        qreal t = static_cast<qreal>(s) / steps;
        QPointF samplePt = start + t * (end - start);
        for (const auto &el : doc.elements()) {
            if (!marked.contains(el.id) && Geometry::hitTest(el, samplePt, 12.0)) {
                marked.insert(el.id);
            }
        }
    }

    // Both r1 and r2 must be hit and marked, r3 must not be marked
    QVERIFY(marked.contains("r1"));
    QVERIFY(marked.contains("r2"));
    QVERIFY(!marked.contains("r3"));

    // Commit deletion on release with single undo entry
    for (const auto &id : marked) {
        doc.removeElement(id);
    }
    history.pushState("Erase", doc.elements(), doc.selectedIds());

    QCOMPARE(doc.elements().size(), 1);
    QCOMPARE(doc.elements().first().id, QString("r3"));

    // Verify Undo restores both deleted elements in a single transaction
    QVERIFY(history.canUndo());
    const HistoryState *restored = history.undo();
    QVERIFY(restored != nullptr);
    doc.setElements(restored->elements);
    doc.setSelection(restored->selectedIds);

    QCOMPARE(doc.elements().size(), 3);
    QVERIFY(doc.findElement("r1") != nullptr);
    QVERIFY(doc.findElement("r2") != nullptr);
    QVERIFY(doc.findElement("r3") != nullptr);

    // Verify Redo erases them again
    QVERIFY(history.canRedo());
    const HistoryState *redone = history.redo();
    QVERIFY(redone != nullptr);
    doc.setElements(redone->elements);
    doc.setSelection(redone->selectedIds);
    QCOMPARE(doc.elements().size(), 1);
    QCOMPARE(doc.elements().first().id, QString("r3"));

    // Test erasing single-point stroke dot
    Element dot;
    dot.id = "dot1";
    dot.type = ElementType::Stroke;
    dot.strokeWidth = 4.0;
    dot.points << StrokePoint{50, 50, 0.5, 0};
    dot.bounds = Geometry::calculateStrokeBounds(dot.points, dot.strokeWidth);
    doc.addElement(dot);
    QCOMPARE(doc.elements().size(), 2);

    // Eraser tap on dot (within hit tolerance)
    QVERIFY(Geometry::hitTest(dot, QPointF(51, 51), 12.0));
    doc.removeElement(dot.id);
    QCOMPARE(doc.elements().size(), 1);
}

void DrawboardTests::testTextWorkflow() {
    Theme theme;
    Backend backend(&theme);

    // 1. Creating text via backend
    backend.createText(100, 200, "System Architecture");
    QCOMPARE(backend.document()->elementCount(), 1);

    const Element &el = backend.document()->elements().first();
    QCOMPARE(el.type, ElementType::Text);
    QCOMPARE(el.text, QString("System Architecture"));
    QVERIFY(el.bounds.width() > 50.0);
    QVERIFY(el.bounds.height() >= 16.0);

    // 2. Hit testing text directly inside its computed bounds
    QVERIFY(Geometry::hitTest(el, QPointF(120, 205), 4.0));
    // Point far away should not hit
    QVERIFY(!Geometry::hitTest(el, QPointF(400, 400), 4.0));

    // 3. Deleting text via selection delete
    backend.selectAll();
    backend.deleteSelected();
    QCOMPARE(backend.document()->elementCount(), 0);

    // 4. Undo restores the text element
    backend.undo();
    QCOMPARE(backend.document()->elementCount(), 1);
    QCOMPARE(backend.document()->elements().first().text, QString("System Architecture"));

    // 5. Updating text to empty string automatically removes the element (tldraw discard)
    QString id = backend.document()->elements().first().id;
    backend.updateText(id, "   ");
    QCOMPARE(backend.document()->elementCount(), 0);

    // 6. Creating empty text is ignored
    backend.createText(50, 50, "");
    backend.createText(50, 50, "   ");
    QCOMPARE(backend.document()->elementCount(), 0);
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

void DrawboardTests::testBackendKeyPressHandling() {
    Theme theme;
    Backend backend(&theme);

    QCOMPARE(backend.tool(), "pen");

    // Test tool shortcut switches
    QVERIFY(backend.handleKeyPress(Qt::Key_R, 0));
    QCOMPARE(backend.tool(), "rectangle");

    QVERIFY(backend.handleKeyPress(Qt::Key_O, 0));
    QCOMPARE(backend.tool(), "ellipse");

    QVERIFY(backend.handleKeyPress(Qt::Key_L, 0));
    QCOMPARE(backend.tool(), "line");

    QVERIFY(backend.handleKeyPress(Qt::Key_A, 0));
    QCOMPARE(backend.tool(), "arrow");

    QVERIFY(backend.handleKeyPress(Qt::Key_T, 0));
    QCOMPARE(backend.tool(), "text");

    QVERIFY(backend.handleKeyPress(Qt::Key_N, 0));
    QCOMPARE(backend.tool(), "note");

    QVERIFY(backend.handleKeyPress(Qt::Key_C, 0));
    QCOMPARE(backend.tool(), "ellipse");

    QVERIFY(backend.handleKeyPress(Qt::Key_X, 0));
    QCOMPARE(backend.tool(), "connector");

    QVERIFY(backend.handleKeyPress(Qt::Key_E, 0));
    QCOMPARE(backend.tool(), "eraser");

    QVERIFY(backend.handleKeyPress(Qt::Key_V, 0));
    QCOMPARE(backend.tool(), "select");

    QVERIFY(backend.handleKeyPress(Qt::Key_P, 0));
    QCOMPARE(backend.tool(), "pen");

    QVERIFY(backend.handleKeyPress(Qt::Key_H, 0));
    QCOMPARE(backend.tool(), "hand");

    QVERIFY(backend.handleKeyPress(Qt::Key_K, 0));
    QCOMPARE(backend.tool(), "laser");

    // Test snap toggle
    bool origSnap = backend.snapToGrid();
    QVERIFY(backend.handleKeyPress(Qt::Key_S, 0));
    QCOMPARE(backend.snapToGrid(), !origSnap);

    // Test Esc to clear selection & return to select
    backend.setTool("pen");
    QVERIFY(backend.handleKeyPress(Qt::Key_Escape, 0));
    QCOMPARE(backend.tool(), "select");

    // Test arrow key nudging when elements are selected
    Element el;
    el.type = ElementType::Rectangle;
    el.bounds = QRectF(100, 100, 50, 50);
    QString elId = backend.document()->addElement(el);
    backend.document()->select(elId);

    // Nudge right 1px
    QVERIFY(backend.handleKeyPress(Qt::Key_Right, 0));
    QCOMPARE(backend.document()->findElement(elId)->bounds.left(), 101.0);

    // Nudge down 10px with Shift
    QVERIFY(backend.handleKeyPress(Qt::Key_Down, Qt::ShiftModifier));
    QCOMPARE(backend.document()->findElement(elId)->bounds.top(), 110.0);
}

void DrawboardTests::testCanvasItemMultiSelectAndMove() {
    CanvasItem canvas;
    Document doc;
    History history;
    canvas.setDocument(&doc);
    canvas.setHistory(&history);
    canvas.setWidth(1000);
    canvas.setHeight(800);
    canvas.setActiveTool("select");

    // e1: Stroke from (100, 100) to (150, 150)
    Element e1;
    e1.type = ElementType::Stroke;
    e1.strokeWidth = 4.0;
    StrokePoint p1{100, 100, 0.5, 0};
    StrokePoint p2{150, 150, 0.5, 0};
    e1.points << p1 << p2;
    e1.bounds = Geometry::calculateStrokeBounds(e1.points, e1.strokeWidth);
    QString id1 = doc.addElement(e1);

    // e2: Rectangle at (200, 200, 100, 100)
    Element e2;
    e2.type = ElementType::Rectangle;
    e2.bounds = QRectF(200, 200, 100, 100);
    QString id2 = doc.addElement(e2);

    // e3: Text at (500, 500, 100, 30)
    Element e3;
    e3.type = ElementType::Text;
    e3.text = "Hello";
    e3.bounds = QRectF(500, 500, 100, 30);
    QString id3 = doc.addElement(e3);

    // 1. Select all and move
    doc.selectAll();
    QCOMPARE(doc.selectedIds().size(), 3);

    // Click on e2 (at world (250, 250)) and drag by (+50, +60)
    QPointF pressScreen = canvas.worldToScreen(QPointF(250, 250));
    QMouseEvent press1(QEvent::MouseButtonPress, pressScreen, pressScreen, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &press1);

    QPointF moveScreen = canvas.worldToScreen(QPointF(300, 310));
    QMouseEvent move1(QEvent::MouseMove, moveScreen, moveScreen, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &move1);

    QMouseEvent release1(QEvent::MouseButtonRelease, moveScreen, moveScreen, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &release1);

    // Check all moved
    QCOMPARE(doc.findElement(id2)->bounds.topLeft(), QPointF(250, 260));
    QCOMPARE(doc.findElement(id3)->bounds.topLeft(), QPointF(550, 560));
    QCOMPARE(doc.findElement(id1)->points.first().x, 150.0);
    QCOMPARE(doc.findElement(id1)->points.first().y, 160.0);

    // 2. Select partial portion using rubberband select (marquee)
    // Box from world (120, 120) to world (380, 380) covers e1 and e2, but NOT e3 (which is at 550, 560)
    QPointF boxStartScreen = canvas.worldToScreen(QPointF(120, 120));
    QMouseEvent press2(QEvent::MouseButtonPress, boxStartScreen, boxStartScreen, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &press2);

    QPointF boxEndScreen = canvas.worldToScreen(QPointF(380, 380));
    QMouseEvent move2(QEvent::MouseMove, boxEndScreen, boxEndScreen, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &move2);

    QMouseEvent release2(QEvent::MouseButtonRelease, boxEndScreen, boxEndScreen, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &release2);

    // Check partial selection
    QVERIFY(doc.isSelected(id1));
    QVERIFY(doc.isSelected(id2));
    QVERIFY(!doc.isSelected(id3));
    QCOMPARE(doc.selectedIds().size(), 2);

    // 3. Move the partial portion by (+30, -20)
    QPointF pressPartial = canvas.worldToScreen(QPointF(270, 280)); // on e2
    QMouseEvent press3(QEvent::MouseButtonPress, pressPartial, pressPartial, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &press3);

    QPointF movePartial = canvas.worldToScreen(QPointF(300, 260));
    QMouseEvent move3(QEvent::MouseMove, movePartial, movePartial, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &move3);

    QMouseEvent release3(QEvent::MouseButtonRelease, movePartial, movePartial, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &release3);

    QCOMPARE(doc.findElement(id2)->bounds.topLeft(), QPointF(280, 240));
    QCOMPARE(doc.findElement(id1)->points.first().x, 180.0);
    QCOMPARE(doc.findElement(id1)->points.first().y, 140.0);
    // e3 should NOT have moved!
    QCOMPARE(doc.findElement(id3)->bounds.topLeft(), QPointF(550, 560));

    // 4. Clicking empty canvas space deselects
    QPointF emptyScreen = canvas.worldToScreen(QPointF(50, 50));
    QMouseEvent pressEmpty(QEvent::MouseButtonPress, emptyScreen, emptyScreen, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &pressEmpty);
    QMouseEvent releaseEmpty(QEvent::MouseButtonRelease, emptyScreen, emptyScreen, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &releaseEmpty);
    QCOMPARE(doc.selectedIds().size(), 0);

    // 5. Select e1 and e2, then click an empty point INSIDE selectionBounds (any point of selected area) and drag to move
    doc.select(id1);
    doc.select(id2, true);
    QCOMPARE(doc.selectedIds().size(), 2);
    QRectF selB = doc.selectionBounds();

    QPointF anyPointInside = selB.center();
    QPointF pressAny = canvas.worldToScreen(anyPointInside);
    QMouseEvent press4(QEvent::MouseButtonPress, pressAny, pressAny, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &press4);

    QPointF moveAny = canvas.worldToScreen(anyPointInside + QPointF(20, 20));
    QMouseEvent move4(QEvent::MouseMove, moveAny, moveAny, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &move4);

    QMouseEvent release4(QEvent::MouseButtonRelease, moveAny, moveAny, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &release4);

    // Both e1 and e2 must have moved by +20, +20
    QCOMPARE(doc.findElement(id2)->bounds.topLeft(), QPointF(300, 260));
    QCOMPARE(doc.findElement(id1)->points.first().x, 200.0);
    QCOMPARE(doc.findElement(id1)->points.first().y, 160.0);
}

void DrawboardTests::testSettingsPersistence() {
    QTemporaryDir tempDir;
    QVERIFY(tempDir.isValid());
    QString settingsPath = tempDir.filePath("settings.json");

    // Phase 1: Create instance, change all settings, save
    {
        Theme theme;
        Backend backend(&theme);
        backend.setSettingsFilePath(settingsPath);

        // Change settings
        theme.setThemeId("dracula");
        backend.setGridStyle(2); // Crosses
        backend.setGridSize(30.0);
        backend.setSnapToGrid(true);
        backend.setPenSize(8.0);
        backend.setActiveColorIndex(2);
        backend.setTool("rectangle");
        backend.saveWindowGeometry(120, 150, 1440, 960, true);

        // Explicitly ensure saved to test file
        backend.saveSettings();

        QVERIFY(QFile::exists(settingsPath));
    }

    // Phase 2: Check JSON contents directly
    {
        QFile file(settingsPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QJsonDocument doc = QJsonDocument::fromJson(file.readAll());
        QVERIFY(doc.isObject());
        QJsonObject obj = doc.object();

        QCOMPARE(obj.value("theme").toString(), "dracula");
        QCOMPARE(obj.value("gridStyle").toInt(), 2);
        QCOMPARE(obj.value("gridSize").toDouble(), 30.0);
        QCOMPARE(obj.value("snapToGrid").toBool(), true);
        QCOMPARE(obj.value("penSize").toDouble(), 8.0);
        QCOMPARE(obj.value("activeColorIndex").toInt(), 2);
        QCOMPARE(obj.value("tool").toString(), "rectangle");
        QCOMPARE(obj.value("windowX").toInt(), 120);
        QCOMPARE(obj.value("windowY").toInt(), 150);
        QCOMPARE(obj.value("windowWidth").toInt(), 1440);
        QCOMPARE(obj.value("windowHeight").toInt(), 960);
        QCOMPARE(obj.value("windowMaximized").toBool(), true);
    }

    // Phase 3: Construct fresh Backend with custom settings path pointing to the saved file
    {
        Theme theme2;
        Backend backend2(&theme2);
        backend2.setSettingsFilePath(settingsPath);
        backend2.loadSettings();

        QCOMPARE(theme2.id(), "dracula");
        QCOMPARE(backend2.gridStyle(), 2);
        QCOMPARE(backend2.gridSize(), 30.0);
        QCOMPARE(backend2.snapToGrid(), true);
        QCOMPARE(backend2.penSize(), 8.0);
        QCOMPARE(backend2.activeColorIndex(), 2);
        QCOMPARE(backend2.tool(), "rectangle");
        QCOMPARE(backend2.windowX(), 120);
        QCOMPARE(backend2.windowY(), 150);
        QCOMPARE(backend2.windowWidth(), 1440);
        QCOMPARE(backend2.windowHeight(), 960);
        QCOMPARE(backend2.windowMaximized(), true);

        // Phase 4: Test that modifying setting auto-updates the saved file
        backend2.setPenSize(4.0);
        backend2.setSnapToGrid(false);
        backend2.setTool("arrow");

        QFile file2(settingsPath);
        QVERIFY(file2.open(QIODevice::ReadOnly));
        QJsonDocument doc2 = QJsonDocument::fromJson(file2.readAll());
        QJsonObject obj2 = doc2.object();
        QCOMPARE(obj2.value("penSize").toDouble(), 4.0);
        QCOMPARE(obj2.value("snapToGrid").toBool(), false);
        QCOMPARE(obj2.value("tool").toString(), "arrow");
    }
}

void DrawboardTests::testColorPaletteAndPenSizes() {
    Theme theme;
    Backend backend(&theme);

    // Verify color options contain coral red #ff8787 from screenshot
    QStringList palette = backend.colorPalette();
    QVERIFY(palette.size() >= 4);
    QVERIFY(palette.contains("#ff8787"));

    // Select color 1 (#ff8787)
    backend.setActiveColorIndex(1);
    QCOMPARE(backend.activeColor(), QColor("#ff8787"));

    // Verify pen sizes 1-4 (3px, 4px, 5px, 6px)
    backend.setPenSize(3.0);
    QCOMPARE(backend.penSize(), 3.0);
    backend.setPenSize(4.0);
    QCOMPARE(backend.penSize(), 4.0);
    backend.setPenSize(5.0);
    QCOMPARE(backend.penSize(), 5.0);
    backend.setPenSize(6.0);
    QCOMPARE(backend.penSize(), 6.0);

    // Test changing color of selected elements
    Element rect;
    rect.type = ElementType::Rectangle;
    rect.bounds = QRectF(10, 10, 50, 50);
    rect.strokeColor = QColor("#ebdbb2");
    rect.strokeWidth = 2.0;
    QString rId = backend.document()->addElement(rect);

    Element stroke;
    stroke.type = ElementType::Stroke;
    stroke.strokeColor = QColor("#ebdbb2");
    stroke.strokeWidth = 2.0;
    stroke.points << StrokePoint{100, 100, 0.5, 0} << StrokePoint{150, 150, 0.5, 0};
    stroke.bounds = Geometry::calculateStrokeBounds(stroke.points, stroke.strokeWidth);
    QString sId = backend.document()->addElement(stroke);

    // Select both elements
    backend.document()->selectAll();
    QCOMPARE(backend.document()->selectedIds().size(), 2);
    backend.history()->pushState("Add Elements", backend.document()->elements(), backend.document()->selectedIds());

    // Change color to index 1 (#ff8787)
    backend.setActiveColorIndex(1);
    QCOMPARE(backend.document()->findElement(rId)->strokeColor, QColor("#ff8787"));
    QCOMPARE(backend.document()->findElement(sId)->strokeColor, QColor("#ff8787"));

    // Change pen size to 4.0
    backend.setPenSize(4.0);
    QCOMPARE(backend.document()->findElement(rId)->strokeWidth, 4.0);
    QCOMPARE(backend.document()->findElement(sId)->strokeWidth, 4.0);

    // Undo restores previous stroke width
    backend.undo();
    QCOMPARE(backend.document()->findElement(rId)->strokeWidth, 2.0);

    // Undo restores previous color
    backend.undo();
    QCOMPARE(backend.document()->findElement(rId)->strokeColor, QColor("#ebdbb2"));
    QCOMPARE(backend.document()->findElement(sId)->strokeColor, QColor("#ebdbb2"));
}

void DrawboardTests::testImageAndScreenshotWorkflow() {
    Theme theme;
    Backend backend(&theme);

    // 1. Create a synthetic test image (e.g. 400x300 screenshot with coral drawing)
    QImage testImg(400, 300, QImage::Format_ARGB32_Premultiplied);
    testImg.fill(QColor("#101011"));
    {
        QPainter p(&testImg);
        p.setPen(QPen(QColor("#ff8787"), 3.0));
        p.drawLine(10, 10, 390, 290);
    }

    // Insert image directly (as when pasting)
    bool ok = backend.insertImage(testImg, "Test Screenshot", QPointF(100, 100));
    QVERIFY(ok);
    QCOMPARE(backend.document()->elementCount(), 1);

    const Element *elem = backend.document()->findElement(*backend.document()->selectedIds().begin());
    QVERIFY(elem != nullptr);
    QCOMPARE(elem->type, ElementType::Image);
    QVERIFY(!elem->image.isNull());
    QVERIFY(!elem->assetData.isEmpty());
    QCOMPARE(backend.tool(), "select");

    // 2. Hit testing image
    QVERIFY(Geometry::hitTest(*elem, elem->bounds.center()));
    QVERIFY(!Geometry::hitTest(*elem, QPointF(1000, 1000)));

    // 3. Move and resize
    backend.document()->moveSelected(QPointF(50, 50));
    QCOMPARE(elem->bounds.center(), QPointF(150, 150));

    // 4. Test file serialization round trip (.draw file with embedded image)
    QTemporaryDir tempDir;
    QString drawFile = tempDir.filePath("test_image_doc.draw");
    QVERIFY(backend.saveDocument(drawFile));
    QVERIFY(QFile::exists(drawFile));

    // Reload in a new document
    Document doc2;
    QVERIFY(doc2.loadFromFile(drawFile));
    QCOMPARE(doc2.elementCount(), 1);
    const Element &reloadedElem = doc2.elements().first();
    QCOMPARE(reloadedElem.type, ElementType::Image);
    QVERIFY(!reloadedElem.image.isNull());
    QCOMPARE(reloadedElem.image.size(), testImg.size());

    // 5. Test insertImageFromFile (PNG file)
    QString pngFile = tempDir.filePath("sample.png");
    testImg.save(pngFile, "PNG");
    QVERIFY(backend.insertImageFromFile(pngFile, QPointF(300, 300)));
    QCOMPARE(backend.document()->elementCount(), 2);

    QString userUpload = "/home/smk/.gemini/antigravity/brain/16ae464e-6dcc-4acd-ad51-e8423ab46e08/.user_uploaded/media_1794726269713_740dc23e.png";
    if (QFile::exists(userUpload)) {
        QVERIFY(backend.insertImageFromFile(userUpload, QPointF(500, 500)));
        QCOMPARE(backend.document()->elementCount(), 3);
    }

    // 6. Test PDF export of canvas with image element
    QString outPdf = tempDir.filePath("exported_with_image.pdf");
    QVERIFY(backend.exportFile(outPdf, "pdf"));
    QVERIFY(QFile::exists(outPdf));
    QVERIFY(QFileInfo(outPdf).size() > 500);

    // 7. Test PNG export of canvas with image element
    QString outPng = tempDir.filePath("exported_with_image.png");
    QVERIFY(backend.exportFile(outPng, "png"));
    QVERIFY(QFile::exists(outPng));
    QVERIFY(QFileInfo(outPng).size() > 500);
}

void DrawboardTests::testLaserPointer() {
    Theme theme;
    Backend backend(&theme);
    CanvasItem canvas;
    Document doc;
    History history;
    canvas.setDocument(&doc);
    canvas.setHistory(&history);
    canvas.setBackend(&backend);
    canvas.setWidth(800);
    canvas.setHeight(600);

    // 1. Press K activates laser
    QVERIFY(backend.handleKeyPress(Qt::Key_K, 0));
    QCOMPARE(backend.tool(), "laser");
    canvas.setActiveTool("laser");
    QCOMPARE(canvas.activeTool(), "laser");

    // 2. Mouse drag with laser
    QPointF pressPos = canvas.worldToScreen(QPointF(100, 100));
    QMouseEvent pressEv(QEvent::MouseButtonPress, pressPos, pressPos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &pressEv);

    QPointF movePos = canvas.worldToScreen(QPointF(200, 200));
    QMouseEvent moveEv(QEvent::MouseMove, movePos, movePos, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &moveEv);

    // Stroke is actively tracking
    QCOMPARE(canvas.laserStrokeCount(), 1);

    // Laser is transient: does not add elements, does not create undo history
    QCOMPARE(doc.elements().size(), 0);
    QVERIFY(!history.canUndo());

    QMouseEvent releaseEv(QEvent::MouseButtonRelease, movePos, movePos, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &releaseEv);

    // Stroke remains on screen after release to linger and smoothly fade
    QCOMPARE(canvas.laserStrokeCount(), 1);
    QCOMPARE(doc.elements().size(), 0);
    QVERIFY(!history.canUndo());

    // 3. Verify laser stroke color matches selected pen color
    backend.setActiveColorIndex(1); // coral #ff8787
    canvas.setStrokeColor(backend.activeColor());
    QCOMPARE(canvas.strokeColor(), QColor("#ff8787"));

    // 4. Switch back to pen cleans up laser strokes immediately
    QVERIFY(backend.handleKeyPress(Qt::Key_P, 0));
    QCOMPARE(backend.tool(), "pen");
    canvas.setActiveTool("pen");
    QCOMPARE(canvas.laserStrokeCount(), 0);
}

void DrawboardTests::testHighlighterAndHoldToStraighten() {
    Theme theme;
    Backend backend(&theme);
    CanvasItem canvas;
    Document doc;
    History history;
    canvas.setDocument(&doc);
    canvas.setHistory(&history);
    canvas.setBackend(&backend);
    canvas.setWidth(800);
    canvas.setHeight(600);

    // 1. Tool switching to Highlighter via Key M
    QVERIFY(backend.handleKeyPress(Qt::Key_M, 0));
    QCOMPARE(backend.tool(), "highlighter");
    canvas.setActiveTool("highlighter");
    QCOMPARE(canvas.activeTool(), "highlighter");

    // 2. Element creation and hit-testing
    Element hl;
    hl.type = ElementType::Highlighter;
    hl.strokeColor = QColor("#fabd2f");
    hl.strokeWidth = 20.0;
    StrokePoint p1{10.0, 50.0, 0.5, 100};
    StrokePoint p2{50.0, 51.0, 0.5, 200};
    StrokePoint p3{100.0, 50.0, 0.5, 300};
    hl.points = {p1, p2, p3};
    hl.bounds = Geometry::calculateStrokeBounds(hl.points, hl.strokeWidth);

    QString id = doc.addElement(hl);
    QVERIFY(!id.isEmpty());
    QCOMPARE(doc.elements().size(), 1);
    QCOMPARE(doc.elements().first().type, ElementType::Highlighter);

    // Hit test on stroke point
    QVERIFY(Geometry::hitTest(doc.elements().first(), QPointF(50.0, 51.0), 6.0));
    // Hit test far away
    QVERIFY(!Geometry::hitTest(doc.elements().first(), QPointF(50.0, 200.0), 6.0));

    // 3. Serialization and Deserialization roundtrip
    QJsonObject json = doc.toJson();
    Document doc2;
    doc2.fromJson(json);
    QCOMPARE(doc2.elements().size(), 1);
    QCOMPARE(doc2.elements().first().type, ElementType::Highlighter);
    QCOMPARE(doc2.elements().first().strokeWidth, 20.0);
    QCOMPARE(doc2.elements().first().points.size(), 3);

    // 4. Render image with exporter
    QImage img = Exporter::renderImage(doc.elements(), theme.current(), 2.0, true, 20.0);
    QVERIFY(!img.isNull());
    QVERIFY(img.width() > 10);
    QVERIFY(img.height() > 10);

    // 5. Test hold-to-straighten logic
    // Simulate drawing stroke on canvas
    canvas.setActiveTool("pen");
    QPointF startScreen = canvas.worldToScreen(QPointF(50, 100));
    QMouseEvent pressEv(QEvent::MouseButtonPress, startScreen, startScreen, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &pressEv);
    QVERIFY(canvas.isDrawing());

    // Generate points along a roughly straight line
    for (int x = 60; x <= 200; x += 20) {
        QPointF pScreen = canvas.worldToScreen(QPointF(x, 100 + (x % 4))); // minimal deviation
        QMouseEvent moveEv(QEvent::MouseMove, pScreen, pScreen, Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        QCoreApplication::sendEvent(&canvas, &moveEv);
    }

    // Call checkHoldToStraighten directly
    canvas.checkHoldToStraighten();
    QVERIFY(canvas.isStraightened());

    // Release mouse
    QPointF endScreen = canvas.worldToScreen(QPointF(200, 100));
    QMouseEvent relEv(QEvent::MouseButtonRelease, endScreen, endScreen, Qt::LeftButton, Qt::NoButton, Qt::NoModifier);
    QCoreApplication::sendEvent(&canvas, &relEv);
    QVERIFY(!canvas.isDrawing());
    QVERIFY(!canvas.isStraightened());

    // The newly added stroke should have exactly 2 points (straightened)
    QCOMPARE(doc.elements().size(), 2);
    const Element &straightened = doc.elements().last();
    QCOMPARE(straightened.points.size(), 2);
}

void DrawboardTests::testDirectClipboardSnapshotAndCycling() {
    Theme theme;
    Backend backend(&theme);
    Document *doc = backend.document();

    // 1. Keyboard Pen Size Switching (1-4, where 1 is 3px, 2 is 4px, 3 is 5px, 4 is 6px)
    QVERIFY(backend.handleKeyPress(Qt::Key_1, 0));
    QCOMPARE(backend.penSize(), 3.0);

    QVERIFY(backend.handleKeyPress(Qt::Key_2, 0));
    QCOMPARE(backend.penSize(), 4.0);

    QVERIFY(backend.handleKeyPress(Qt::Key_3, 0));
    QCOMPARE(backend.penSize(), 5.0);

    QVERIFY(backend.handleKeyPress(Qt::Key_4, 0));
    QCOMPARE(backend.penSize(), 6.0);

    // 2. Tab and Shift+Tab color cycling
    backend.setActiveColorIndex(0);
    QVERIFY(backend.handleKeyPress(Qt::Key_Tab, 0));
    QCOMPARE(backend.activeColorIndex(), 1);

    QVERIFY(backend.handleKeyPress(Qt::Key_Backtab, Qt::ShiftModifier));
    QCOMPARE(backend.activeColorIndex(), 0);

    // 3. Pen Size cycling
    qreal origSize = backend.penSize();
    backend.cyclePenSize(true);
    QVERIFY(!qFuzzyCompare(backend.penSize(), origSize));

    // 4. Direct Clipboard Snapshot (Ctrl+Shift+C)
    // When document is empty, copySelectionToClipboard returns false
    QVERIFY(!backend.copySelectionToClipboard());

    // Add rectangle element to document
    Element el;
    el.type = ElementType::Rectangle;
    el.bounds = QRectF(20, 20, 150, 100);
    el.strokeColor = QColor("#ff8787");
    el.strokeWidth = 3.0;
    QString elId = doc->addElement(el);
    QVERIFY(!elId.isEmpty());

    // Copy entire canvas snapshot to clipboard
    QVERIFY(backend.copySelectionToClipboard());
    QClipboard *cb = QGuiApplication::clipboard();
    QVERIFY(cb != nullptr);
    QImage cbImg = cb->image();
    QVERIFY(!cbImg.isNull());
    QVERIFY(cbImg.width() > 100);
    QVERIFY(cbImg.height() > 80);

    // Test with selection
    doc->select(elId, false);
    QCOMPARE(doc->selectedIds().size(), 1);
    QVERIFY(backend.handleKeyPress(Qt::Key_C, Qt::ControlModifier | Qt::ShiftModifier));
    QImage selImg = cb->image();
    QVERIFY(!selImg.isNull());
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
