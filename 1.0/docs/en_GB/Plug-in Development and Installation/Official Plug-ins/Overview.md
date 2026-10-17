# Official Plug-ins Overview

Ludork includes four official C# plug-ins: Official Locale Tools, Official Random Map, Official Blueprint AI and Official Resource Cleanup. Their source is also available as examples of the public plug-in API.

## The official plug-ins

| Plug-in | What it does |
|---|---|
| [Official Locale Tools](<Locale Tools.md>) | Opens the project's `Data/Locale/Locale.xlsx` workbook from a Database command, supplies editor text hints for `{ID}` fields, writes one Lua catalogue per language through a before-Export hook, and excludes the authoring workbook through a separate before-Pack hook. |
| [Official Random Map](<Random Map.md>) | Adds one map-item context-menu command that opens the plug-in's own Avalonia window, reads immutable map and tileset snapshots, and replaces a single layer with revision protection. |
| [Official Blueprint AI](<Blueprint AI.md>) | Exposes **Game → Blueprint AI**: configure a provider, model, endpoint and secret, hold a conversation bound to one existing Blueprint, and apply a validated proposal with a generated diff as one Undo step. Its plug-in ID is `Ludork.OfficialBlueprintAI`. |
| [Official Resource Cleanup](<Resource Cleanup.md>) | Opens **Plugins → Resource Cleanup** to scan for project resources with no static reference, protect resources reached through dynamic paths with a keep list, and move confirmed candidates to the operating-system Recycle Bin or Trash. |

All four share one official plug-in installation in `~/Ludork/Plugins`, with registrations in `~/Ludork/plugins.json`. Windows MSI and EXE install them by default, with an option to skip. On macOS, run **Install Official Plugins** from the DMG. Repository and portable runs use an explicit installation command; startup does not install the repository or packaged source automatically.

[Game Localisation Workflow](<Game Localisation Workflow.md>) is the fifth page in this group. It is not a separate plug-in but the authoring task page for Official Locale Tools, covering the workbook, Export and the runtime resolution of `{ID}` strings.

## Installation and management

See [Installing and Managing Plug-ins](<../Installing and Managing Plug-ins.md>) for installation commands, skip options, restart requirements, status and retained data. Installing the official plug-ins replaces all existing global and project plug-ins, their registrations and their data on every platform. Close Ludork and back up anything needed before continuing.

## Related pages

- [Installing and Managing Plug-ins](<../Installing and Managing Plug-ins.md>) — installation roots, import, status and uninstall
- [Registration and Hook Reference](<../Registration and Hook Reference.md>) — the registration methods and hook contracts behind these plug-ins
- [Game Localisation Workflow](<Game Localisation Workflow.md>) — authoring and exporting game text with Official Locale Tools
