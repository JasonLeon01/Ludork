# Official Plug-ins Overview

The official plug-ins are the four C# plug-ins distributed with the Ludork editor: Official Locale Tools, Official Random Map, Official Blueprint AI and Official Resource Cleanup. Locale Tools and Random Map are documented as reference implementations — the first for an ordinary menu command, a text hint provider and Export and before-Pack hooks, the second for a map-item command with its own Avalonia window — while Blueprint AI and Resource Cleanup show the narrow host bridges the editor exposes for those features. They reach the editor through the release installation rather than through the third-party import path, so a release installation does not show the trust confirmation for them.

## The official plug-ins

| Plug-in | What it does | Preinstalled |
|---|---|---|
| [Official Locale Tools](<Locale Tools.md>) | Opens the project's `Data/Locale/Locale.xlsx` workbook from a Database command, supplies editor text hints for `{ID}` fields, writes one Lua catalogue per language through a before-Export hook, and excludes the authoring workbook through a separate before-Pack hook. | Preinstalled and registered by the Windows release. On macOS, run **Install Official Plugins** from the DMG. |
| [Official Random Map](<Random Map.md>) | Adds one map-item context-menu command that opens the plug-in's own Avalonia window, reads immutable map and tileset snapshots, and replaces a single layer with revision protection. | Installed with the editor release. On macOS, also run **Install Official Plugins** from the DMG. |
| [Official Blueprint AI](<Blueprint AI.md>) | Exposes **Game → Blueprint AI**: configure a provider, model, endpoint and secret, hold a conversation bound to one existing Blueprint, and apply a validated proposal with a generated diff as one Undo step. | Preinstalled on Windows; installed from the DMG's hidden payload on macOS. Plug-in ID `Ludork.OfficialBlueprintAI`. |
| [Official Resource Cleanup](<Resource Cleanup.md>) | Opens **Plugins → Resource Cleanup** to scan for project resources with no static reference, protect resources reached through dynamic paths with a keep list, and move confirmed candidates to the operating-system Recycle Bin or Trash. | Installed with the editor's official plug-ins. |

The Windows distribution installs all four below the installation root's `Plugins` folder with a generated `plugins.json`, and they are already registered and enabled. The macOS DMG keeps them outside `Ludork.app`; the **Install Official Plugins** installer transactionally replaces `~/Ludork/Plugins` and `~/Ludork/plugins.json` as one installation, and success deletes existing third-party plug-in source, registrations and everything below `Plugins/.data`. Details are in [Official release plug-ins](<../Installing and Managing Plug-ins.md#official-release-plug-ins>).

[Game Localisation Workflow](<Game Localisation Workflow.md>) is the fifth page in this group. It is not a separate plug-in but the authoring task page for Official Locale Tools, covering the workbook, Export and the runtime resolution of `{ID}` strings.

## Checking what is installed

**Plugins → Manage Plugins** reports the Loaded, manifest-invalid, compile-failed, initialisation-failed and pending restart or deletion states with their diagnostics, and **Loaded** is the effective enabled state because there is no hot enable or disable switch. Plug-in source is compiled again at every startup and is never hot-loaded, so an import, an uninstall or a direct source edit needs a restart. See [Restart and status](<../Installing and Managing Plug-ins.md#restart-and-status>).

## Trust and installation caveats

Editor plug-ins are trusted code: they run with the permissions of the editor process and are not sandboxed, so only directories whose source is trusted should be imported. A release installation skips the third-party trust confirmation for the official plug-ins, and the macOS app and its installer remain unsigned and unnotarised. Ludork provides no plug-in marketplace, signature authority or dependency resolver, so distribution trust stays the publisher's responsibility. Read [Import a third-party plug-in](<../Installing and Managing Plug-ins.md#import-a-third-party-plug-in>) and [Testing and Distribution](<../Testing and Distribution.md#limitations>) before adding anything to the plug-in root.

## Related pages

- [Installing and Managing Plug-ins](<../Installing and Managing Plug-ins.md>) — installation roots, import, status and uninstall
- [Registration and Hook Reference](<../Registration and Hook Reference.md>) — the registration methods and hook contracts behind these plug-ins
- [Game Localisation Workflow](<Game Localisation Workflow.md>) — authoring and exporting game text with Official Locale Tools
