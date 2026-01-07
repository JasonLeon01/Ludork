# Ludork

Ludork is a PyQt5-based game toolkit that combines an editor UI and a lightweight runtime engine built on SFML via pybind11 bindings. It provides visual scripting via a Node Graph, asset management, tileset/map tooling, shader and audio utilities, and gameplay foundations. The goal is to make building and iterating on 2D RPG-style content fast and approachable.

## Highlights
- Visual Node Graph editor for authoring reusable "Common Functions" (powered by NodeGraphQt)
- Tileset and map editing with preview, selection, and persistence to `.dat`
- Asset browsing and preview for images, audio, shaders, and transitions
- Configuration management and localization support
- Integrated console, toast notifications, and material-themed UI

## Architecture Overview
- Engine
  - Gameplay: scenes, camera, tile map, particle system, and actor bases
  - UI: canvas, rect/text/image/sprite primitives, windows, text box
  - Node Graph: node/graph/class dictionaries and editor integration
  - Managers: audio, texture, font, time; effects/input/locale helpers
  - Utils & Filters: file IO, render, math, event; audio filters
  - Runtime: SFML core via pybind11 bindings for rendering, audio, and input
- Widgets
  - Editor windows: start/main, file explorer, file preview, console
  - Node Graph window and panels for editing common functions
  - Tileset editor, map tools, config/settings panels, toggles
- Data & Assets
  - Structured data under `Data/` (Configs, Maps, Tilesets, CommonFunctions)
  - Sample assets under `Sample/` (characters, tilesets, shaders, sounds)
- C Extensions
  - Gameplay map/tilemap modules for performance-critical operations

## Visual Node Graph
- Purpose: author reusable logic blocks as visual graphs
- Node categories: Math, String, Utils, Containers, plus project-specific nodes
- Persistence: graphs saved under `Data/CommonFunctions/*.dat`
- Runtime: graphs are loaded and transformed into executable graph instances

## Getting Started
- Requirements: Python 3.x, PyQt5, NodeGraphQt, qt-material, psutil, pympler, av, nuitka
- Install:
  - `pip install -r requirements.txt`
- Run:
  - `python main.py`

## Typical Use
- Open the editor, browse assets, and inspect tilesets/maps
- Create/edit Common Functions in the Node Graph window
- Preview changes and persist to data files (`.dat` / `.json`)
- Iterate on gameplay scenes and UI using engine primitives

## Packaging
- Build scripts available for bundling (e.g., via Nuitka) to produce distributables

## License
This project is licensed as described in [LICENSE.md](LICENSE.md).