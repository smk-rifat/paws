# Drawboard

> **Complex capability underneath, extremely simple interface on top.**  
> *Neovim for visual thinking on Arch Linux & Omarchy.*

Drawboard is a serious, local-first personal digital drawing and visual thinking application built for Linux developers and engineers. It combines the quiet, keyboard-first focus of a terminal editor with an infinite hardware-accelerated vector canvas for technical sketches, architecture diagrams, flowcharts, brainstorming, and digital drawing.

![Drawboard Screenshot](docs/screenshot.png)

---

## Highlights

- **Neovim / Terminal-Inspired Restraint**: No giant SaaS toolbars, floating clutter, or decorative animations. When opened, the canvas dominates completely.
- **Minimal Canvas Controls**: Main board features only the 3 theme-aware primary drawing colors, compact pen size control, and subtle Neovim-style statusline.
- **Top-Left Dot Menu (`•`)**: A compact, Hyprland-inspired floating surface following the desktop theme and window rounding rules.
- **First-Class Semantic Theme System**: Full semantic design-token system across 6 iconic themes and variants:
  - **Gruvbox** (Dark Medium, Dark Hard, Dark Soft)
  - **Catppuccin** (Mocha, Macchiato, Frappe, Latte)
  - **Everforest** (Dark Medium, Dark Hard, Dark Soft)
  - **VS Code** (Dark+, Dark Modern)
  - **IntelliJ IDEA** (Darcula, Modern Dark Island)
  - **Dracula**
- **Smooth Freehand Drawing**: Sub-pixel Catmull-Rom spline interpolation, jitter suppression, and pressure sensitivity support for natural, stabilized strokes.
- **Smart Connectors**: Cardinal connection points that attach to shapes and automatically route and stay connected as shapes move.
- **Full Drawing Tools**: Freehand Pen (`P`), Eraser (`E`), Selection (`V`), Rectangle (`R`), Ellipse (`O`), Line (`L`), Arrow (`A`), Text (`T`), Sticky Notes (`N`), Connectors (`C`), and Hand/Pan (`H`).
- **Real Vector Exports**: High-resolution PNG (with transparency option), pure vector SVG, and vector PDF generated via native C++ graphics backends.
- **Local-First & Autosave**: Versioned `.draw` JSON format with atomic disk writing and crash recovery.
- **Headless CLI Pipeline**: Agents and scripts can export diagrams without a display via `drawboard --export diagram.draw output.png`.

---

## Keyboard Shortcuts

Drawboard is built keyboard-first. Global shortcuts never interfere with text editing or modal input fields.

| Key | Action | Category |
|---|---|---|
| `P` | Freehand Pen | Tools |
| `V` | Select / Move / Resize | Tools |
| `E` | Eraser | Tools |
| `R` | Rectangle (Shift for square) | Tools |
| `O` | Ellipse / Circle (Shift for circle) | Tools |
| `L` | Line | Tools |
| `A` | Arrow | Tools |
| `T` | Text Tool | Tools |
| `N` | Sticky Note | Tools |
| `C` | Smart Connector | Tools |
| `H` | Hand / Pan | Tools |
| `Space` + Drag | Temporary Pan | Navigation |
| `Ctrl` + Wheel | Smooth Zoom to Mouse Anchor | Navigation |
| `Ctrl` + `=` | Zoom In | Navigation |
| `Ctrl` + `-` | Zoom Out | Navigation |
| `Ctrl` + `0` | Reset Zoom (100%) | Navigation |
| `Shift` + `1` | Zoom to Fit All Content | Navigation |
| `Ctrl` + `Z` | Undo | Edit |
| `Ctrl` + `Shift` + `Z` | Redo | Edit |
| `Delete` / `Backspace` | Delete Selected | Edit |
| `Ctrl` + `D` | Duplicate Selected | Edit |
| `Ctrl` + `A` | Select All | Edit |
| `Ctrl` + `G` | Group Selected | Edit |
| `Ctrl` + `Shift` + `G` | Ungroup Selected | Edit |
| `]` / `[` | Bring Forward / Send Backward | Edit |
| `Shift` + `]` / `Shift` + `[` | Bring to Front / Send to Back | Edit |
| `Ctrl` + `N` | New Document | File |
| `Ctrl` + `O` | Open Document | File |
| `Ctrl` + `S` | Save Document | File |
| `Ctrl` + `E` | Export (PNG / SVG / PDF) | File |
| `Ctrl` + `'` | Cycle Grid Style (Dots, Lines, Crosses, None) | View |
| `?` | Keyboard Shortcuts Cheatsheet | Help |
| `Esc` | Clear Selection / Return to Select | Navigation |

All shortcuts can be searched, customized, and reset in the Shortcuts Settings panel (`~/.config/drawboard/shortcuts.json`).

---

## Command Line Usage

Drawboard integrates directly into terminal workflows:

```bash
# Open an existing document or start fresh
drawboard path/to/diagram.draw

# Headless export from terminal or scripts (PNG, SVG, PDF)
drawboard --export architecture.png path/to/diagram.draw
drawboard --export architecture.svg path/to/diagram.draw
drawboard --export architecture.pdf path/to/diagram.draw
```

---

## Build & Test

Drawboard requires standard Arch Linux / Omarchy development packages (`qmake6`, `gcc`, `qt6-base`, `qt6-declarative`, `qt6-svg`, `qt6-wayland`).

```bash
# Compile binary to build/drawboard
bin/build

# Run automated test suite (offscreen QtTest)
bin/test

# Build and install Arch Linux package into launcher
bin/install
```

---

## Document Format Specification

Drawboard documents are local-first, human-readable, and versioned JSON:

```json
{
  "format": "drawboard",
  "schemaVersion": 1,
  "appVersion": "0.1.0",
  "theme": "gruvbox-dark-medium",
  "createdAt": "2026-11-14T22:30:00Z",
  "elements": [
    {
      "id": "el_1",
      "type": "rectangle",
      "bounds": [100, 100, 180, 80],
      "strokeColor": "#d79921",
      "strokeWidth": 2.0,
      "fillColor": "#32302f",
      "cornerRadius": 4.0
    }
  ]
}
```

---

## License

MIT License. Portions copyright (c) David Heinemeier Hansson (Omacut/Monologue design patterns).
