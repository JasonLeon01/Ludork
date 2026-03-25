# Ludork

Ludork is a PyQt5-based 2D RPG toolkit that pairs an editor UI with a lightweight runtime engine powered by SFML bindings through pybind11. It focuses on fast iteration with visual scripting, asset tooling, and gameplay foundations so creators can build content without constantly leaving the editor.

## Highlights
- Visual Node Graph editor for reusable Common Functions (NodeGraphQt)
- Tileset and map editing with preview, selection, and `.dat` persistence
- Asset browsing and preview for images, audio, shaders, and transitions
- Configuration management with localization support
- Integrated console, toast notifications, and material-themed UI
- Runtime engine that mirrors editor data formats for rapid iteration

## Architecture Overview
- Engine
  - Reference runtime implementation lives under `Sample/Engine/`
  - Gameplay: scenes, camera, tile map, particle system, and actor bases
  - UI: canvas, rect/text/image/sprite primitives, windows, text box
  - Node Graph: node/graph/class dictionaries and editor integration
  - Managers: audio, texture, font, time; effects/input/locale helpers
  - Utils & Filters: file IO, render, math, event; audio filters
  - Runtime: SFML core via pybind11 bindings for rendering, audio, and input
- Widgets
  - Editor windows: start/main, file explorer, file preview, console
  - Node Graph window and panels for editing Common Functions
  - Tileset editor, map tools, config/settings panels, toggles
- Data & Assets
  - Structured project data under `Data/` (Configs, Maps, Tilesets, CommonFunctions, Animations, Blueprints, General, Locale)
  - Sample Project layout under `Sample/` (`Sample/Data/` + `Sample/Assets/`)
- C Extensions
  - Native acceleration modules under `C_Extensions/` (Editor, GamePlay, Graphics, Utils)
  - Editor-side build outputs are typically placed under `EditorExtensions/`

## Visual Node Graph
- Purpose: author reusable logic blocks as visual graphs
- Node categories: Math, String, Utils, Containers, plus project-specific nodes
- Persistence: graphs saved under the Project `Data/CommonFunctions/*.dat` (see `Sample/Data/CommonFunctions/` for examples)
- Runtime: graphs are loaded and transformed into executable graph instances

## Editor Workflow
- Browse assets and inspect tilesets/maps in the main editor windows
- Create and refine Common Functions in the Node Graph editor
- Preview changes and persist data files (`.dat` / `.json`)
- Use engine primitives to assemble gameplay scenes and UI

## Data Layout
- `Data/Configs`: runtime and editor configuration files
- `Data/Maps`: map definitions and serialized tile layers
- `Data/Tilesets`: tileset metadata and selection presets
- `Data/CommonFunctions`: visual graph assets saved as `.dat`
- `Data/Animations`: animation definitions (for example `.json`)
- `Data/Blueprints`: actor/enemy/item blueprint definitions (project-driven)
- `Data/General`: shared data tables (for example enemies/items)
- `Data/Locale`: localization sources/outputs (for example `Locale.xlsx` and per-language folders)
- `Assets`: media resources (images, audio, fonts, shaders, etc.)
- `Sample`: example Project data and assets (`Sample/Data`, `Sample/Assets`)

## Getting Started
- Requirements: Python 3.12.0, PyQt5, NodeGraphQt, qt-material, psutil, pympler, av, nuitka, openpyxl, zstandard, pybind11, pybind11-stubgen
- Install:
  - Windows
    ```bat
    ./init.bat
    ```
  - Unix-like(macOS)
    ```bash
    ./init.sh
    ```

## Packaging
- Build scripts are available for bundling with tools like Nuitka

## License
This project is licensed as described in [LICENSE.md](LICENSE.md).
