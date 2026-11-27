# Official Plug-ins Overview

Ludork ships five official C# plug-ins covering localisation, procedural map layout, Mota Actor placement, Blueprint AI assistance and resource cleanup. Their source directories under `Plugins/` also serve as reference implementations of the public plug-in API.

## Included plug-ins

| Plug-in | Purpose |
|---|---|
| [Official Locale Tools](<Locale Tools.md>) | Opens the translation workbook, provides editor text hints and exports Lua language catalogues |
| [Official Random Map](<Random Map.md>) | Generates procedural terrain regions and saves one layer under revision protection |
| [Official Mota Map Fill](<Mota Map Fill.md>) | Populates map regions with doors, enemies and rewards using a saved player's battle damage |
| [Official Blueprint AI](<Blueprint AI.md>) | Discusses one existing Blueprint and applies a validated, reviewable proposal as a single Undo step |
| [Official Resource Cleanup](<Resource Cleanup.md>) | Finds statically unreferenced resources, protects keep-listed paths and moves confirmed candidates to the system Trash |

For workbook authoring, Export integration and runtime `{ID}` resolution with Official Locale Tools, see [Game Localisation Workflow](<Game Localisation Workflow.md>).

## Installation and management

See [Installing and Managing Plug-ins](<../Installing and Managing Plug-ins.md>) for installer options, command-line installation, restart requirements, status diagnostics and retained data.

> **Warning:** Installing the official plug-ins replaces all existing global and project plug-ins, their registries and their data on every platform. Close Ludork and back up anything you need before installing.
