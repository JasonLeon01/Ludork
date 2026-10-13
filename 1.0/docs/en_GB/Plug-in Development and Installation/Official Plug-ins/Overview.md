# Official Plug-ins Overview

Ludork includes four official C# plug-ins: Official Locale Tools, Official Random Map, Official Blueprint AI and Official Resource Cleanup. Their source is also available as examples of the public plug-in API.

## The official plug-ins

| Plug-in | What it does | Preinstalled |
|---|---|---|
| [Official Locale Tools](<Locale Tools.md>) | Opens the project's `Data/Locale/Locale.xlsx` workbook from a Database command, supplies editor text hints for `{ID}` fields, writes one Lua catalogue per language through a before-Export hook, and excludes the authoring workbook through a separate before-Pack hook. | Preinstalled and registered by the Windows release. On macOS, run **Install Official Plugins** from the DMG. |
| [Official Random Map](<Random Map.md>) | Adds one map-item context-menu command that opens the plug-in's own Avalonia window, reads immutable map and tileset snapshots, and replaces a single layer with revision protection. | Installed with the editor release. On macOS, also run **Install Official Plugins** from the DMG. |
| [Official Blueprint AI](<Blueprint AI.md>) | Exposes **Game → Blueprint AI**: configure a provider, model, endpoint and secret, hold a conversation bound to one existing Blueprint, and apply a validated proposal with a generated diff as one Undo step. | Preinstalled on Windows; installed from the DMG's hidden payload on macOS. Plug-in ID `Ludork.OfficialBlueprintAI`. |
| [Official Resource Cleanup](<Resource Cleanup.md>) | Opens **Plugins → Resource Cleanup** to scan for project resources with no static reference, protect resources reached through dynamic paths with a keep list, and move confirmed candidates to the operating-system Recycle Bin or Trash. | Installed with the editor's official plug-ins. |

The Windows distribution installs all four below the installation root's `Plugins` folder with a generated `plugins.json`, and they are already registered and enabled. The macOS DMG keeps them outside `Ludork.app`; the **Install Official Plugins** installer transactionally replaces `~/Ludork/Plugins` and `~/Ludork/plugins.json` as one installation, and success deletes existing third-party plug-in source, registrations and everything below `Plugins/.data`. Details are in [Official release plug-ins](<../Installing and Managing Plug-ins.md#official-release-plug-ins>).

[Game Localisation Workflow](<Game Localisation Workflow.md>) is the fifth page in this group. It is not a separate plug-in but the authoring task page for Official Locale Tools, covering the workbook, Export and the runtime resolution of `{ID}` strings.

## Installation and management

See [Installing and Managing Plug-ins](<../Installing and Managing Plug-ins.md>) for release installation, restart requirements, status and retained data. On macOS, **Install Official Plugins** replaces the existing plug-in installation, including its data; read the replacement notice before continuing.

## Related pages

- [Installing and Managing Plug-ins](<../Installing and Managing Plug-ins.md>) — installation roots, import, status and uninstall
- [Registration and Hook Reference](<../Registration and Hook Reference.md>) — the registration methods and hook contracts behind these plug-ins
- [Game Localisation Workflow](<Game Localisation Workflow.md>) — authoring and exporting game text with Official Locale Tools
