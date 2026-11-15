# Developer & AI Agent Guidelines for Paws

Welcome! This guide documents the design philosophy, architectural foundation, and development workflows for any engineer or AI coding agent working on **Paws** (formerly Drawboard).

---

## 1. Project Philosophy & Core Tenets

1. **Complex Capability Underneath, Extremely Simple Interface on Top**:
   - The user should see an uncluttered, infinite canvas that stays out of their way.
   - Do **NOT** add giant SaaS toolbars, floating widgets, or decorative gradients.
   - Preserve the Neovim-style keyboard-first efficiency and distraction-free Zen mode.

2. **Butter-Smooth Performance is Non-Negotiable**:
   - Every stroke must feel organic, fluid, and immediate.
   - Freehand strokes use **Midpoint Quadratic Bézier** splines with mathematical $C^1$ continuity and **5-point binomial Gaussian smoothing**.
   - Canvas rendering must remain batched (e.g. `drawPoints` instead of individual per-frame `drawEllipse` calls) and utilize viewport culling.

3. **Local-First & Respectful**:
   - Files are stored as human-readable, versioned `.paws` / `.draw` JSON documents.
   - Write operations use atomic file replacement (write to temp file, then rename).
   - User settings and shortcuts reside in standard XDG directories (`~/.config/paws/`).

4. **Zero Regressions**:
   - All 29 automated tests in the test suite must pass after every change:
     ```bash
     ./bin/build && ./bin/test
     ```
   - Do **not** execute `./bin/install` unless the user explicitly requests package installation.

---

## 2. Architecture & Directory Structure

```
paws/
├── bin/
│   ├── build                 # Compiles binary into build/paws
│   ├── test                  # Compiles and runs test suite in build/tests/
│   └── install               # Builds Arch package and installs via pacman
├── src/
│   ├── types.h               # Core data structures (Element, StrokePoint, etc.)
│   ├── geometry.h / .cpp     # Splines, smoothing, hit-testing, connectors
│   ├── document.h / .cpp     # Document model, mutations, JSON serialization
│   ├── history.h / .cpp      # Undo/redo stack (immutable snapshots)
│   ├── theme.h / .cpp        # Semantic theme token system & palette definitions
│   ├── shortcuts.h / .cpp    # Keyboard shortcuts manager and persistence
│   ├── exporter.h / .cpp     # PNG, SVG, PDF vector rendering & headless CLI
│   ├── canvasitem.h / .cpp   # QQuickPaintedItem interactive canvas
│   ├── backend.h / .cpp      # QML C++ bridge, clipboard snapshot, autosave
│   ├── main.cpp              # Application entrypoint & CLI flag handler
│   ├── Main.qml              # Root application window
│   ├── CompactToolbar.qml    # Minimal floating top toolbar
│   ├── DotMenu.qml           # Floating top-left dot menu
│   ├── StatusLine.qml        # Neovim-inspired bottom status line
│   ├── SettingsDialog.qml    # Theme and preference settings modal
│   ├── CatOverlay.qml        # Playful startup cat animation
│   └── resources.qrc         # Qt resource manifest
├── tests/
│   ├── drawboard_tests.cpp   # Comprehensive QtTest automated test suite
│   └── drawboard_tests.pro   # Test qmake configuration
├── pkgbuild/
│   ├── PKGBUILD              # Arch Linux package specification
│   ├── paws.desktop          # Desktop launcher entry
│   └── paws.svg              # Application icon
├── docs/                     # Documentation and screenshots
├── paws.pro                  # Main Qt project file
├── README.md                 # User-facing README
└── AGENTS.md                 # Agent and developer guide (this file)
```

---

## 3. Core Subsystems

### A. Freehand Stroke Pipeline (`src/geometry.cpp`, `src/canvasitem.cpp`)
- **Point Sampling**:
  - Distance filtering: New mouse move samples within `1.2 / m_zoom` pixels of the previous point are discarded to eliminate sensor noise and sub-pixel clustering.
  - Hardware pressure: Stylus/pen pressure is extracted from `QEventPoint::pressure()` when available.
- **Smoothing Filter**:
  - `Geometry::smoothPoints()` applies a 5-point binomial Gaussian filter $(\frac{1}{16}, \frac{4}{16}, \frac{6}{16}, \frac{4}{16}, \frac{1}{16})$ to eliminate hand tremors without phase lag.
- **Spline Construction**:
  - `Geometry::buildSmoothPath()` uses Midpoint Quadratic Bézier interpolation:
    - Starts at $P_0$.
    - For each pair $P_i, P_{i+1}$, calculates midpoint $M_i = \frac{P_i + P_{i+1}}{2}$ and calls `path.quadTo(P_i, M_i)`.
    - Ends with a line to $P_n$.
  - Guaranteed $C^1$ continuity, zero overshoot, and no self-intersections.
- **Live Preview**:
  - `CanvasItem::drawInProgress()` passes real-time smoothed points to `buildSmoothPath()` while drawing so the stroke seen during drag matches the committed stroke exactly on mouse release.

### B. High-Performance Canvas Rendering (`src/canvasitem.cpp`)
- **Grid Rendering**:
  - Dot grids use `painter->drawPoints(dotPoints.data(), dotPoints.size())` with round pen caps. Never revert to looping over thousands of individual `drawEllipse()` calls.
  - Grid lines and crosses use batched `painter->drawLines()`.
- **Viewport Culling**:
  - `CanvasItem::paint()` computes visible world bounds with a 60px bleed buffer and skips off-screen elements.
- **Transient Laser Strokes (`K`)**:
  - Laser points are stored in `struct LaserStroke { QVector<QPointF> points; qint64 finishedTime = 0; }`.
  - While drawing (`finishedTime == 0`), strokes remain 100% visible.
  - After release, strokes linger for 1200ms at 100% opacity, then smoothly fade out over 800ms before being pruned.

### C. Semantic Theme System (`src/theme.cpp`, `src/theme.h`)
- Always use semantic tokens rather than hardcoded hex colors:
  - `canvas`: Background color
  - `canvasGrid`: Subtle grid dots/lines
  - `surface` / `surfaceElevated`: Menus and toolbars
  - `accent` / `accentForeground`: Highlights and active states
  - `strokePrimary`, `strokeSecondary`, `strokeTertiary`: Main drawing colors
  - `text`, `textMuted`, `border`, `borderMuted`, `selection`, `note`

### D. Document Model & History (`src/document.cpp`, `src/history.cpp`)
- Document mutations push states onto the `History` stack for Undo (`Ctrl+Z`) and Redo (`Ctrl+Shift+Z`).
- Serialization uses versioned JSON (`"format": "paws"`, `"schemaVersion": 1`, with backwards compatibility for `"drawboard"`).
- Disk writes must remain atomic via `QSaveFile` or writing to a temporary file before renaming.

---

## 4. Verification & Testing

Always verify changes using the automated test suite before concluding any task:

```bash
# Build both app and tests
./bin/build

# Run test suite
./bin/test
```

### Test Coverage Checklist
The test suite in `tests/drawboard_tests.cpp` covers:
1. Geometry distance & hit-testing
2. Stroke smoothing & spline construction
3. Cardinal points & smart connector routing
4. Arrowhead polygon math
5. Document mutations, grouping, and z-ordering
6. Atomic file save/load & JSON roundtrip
7. History stack (undo/redo bounds)
8. Eraser hit-testing workflow
9. Theme families & design tokens
10. Shortcut manager & keypress routing
11. Settings persistence
12. Direct clipboard snapshots (`Ctrl+Shift+C`)
13. Laser pointer persistence & lingering lifecycle
14. Highlighter & hold-to-straighten recognition
15. PNG, SVG, and PDF exports
16. Clean QML component compilation & engine loading

---

## 5. Adding New Features

- **New QML Files**: Must be registered in `src/resources.qrc`.
- **New Shortcuts**: Must be defined in `src/shortcuts.cpp` and mapped in `src/backend.cpp::handleKeyPress()`.
- **New Tools**: Must be added to `enum class Tool` in `src/types.h`, supported in `CanvasItem::setActiveTool()`, and mirrored in `src/backend.h`.
