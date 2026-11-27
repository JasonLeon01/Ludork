# Official Resource Cleanup

Official Resource Cleanup (`Plugins/OfficialResourceCleanup`) scans a saved project for resources with no detected static references, lets you protect dynamically loaded paths and moves confirmed candidates to the operating-system Recycle Bin or Trash.

## Workflow

Install Official Resource Cleanup via [Official release plug-ins](<../Installing and Managing Plug-ins.md#official-release-plug-ins>), then open a project, stop the game and save all pending edits before scanning.

1. Select **Plug-ins → Resource Cleanup**. The window separates the read-only built-in protected list from the project's additional keep paths.
2. Review the built-in protected list, which includes the editor's [fixed resource directories](<../../Editor User Guide/Project Settings and Asset Layout.md#fixed-resource-directories>) plus plug-in entries for UI defaults, application icons and asset licences. Assets referenced by files in protected directories are also kept.
3. Add project-relative files or directories loaded via dynamic Lua paths to the additional keep list, one per line (for example, `Assets/Characters/Dynamic` or `Data/Animations/GeneratedEffect.json`). Listing a directory protects everything inside it.
4. Select **Scan**. Scanning runs in the background displaying stage, progress and current path; **Cancel** stops scanning without recycling anything.
5. Inspect the **Data** and **Assets** tabs, each showing per-file project-relative paths, sizes, counts and totals. Listing a candidate means no supported static reference was found — not that runtime usage has been disproved.
6. To protect a listed file, select its row and choose **Keep selected and rescan**, or edit the keep list and rescan. Modifying the keep list invalidates the previous report, and any scan issues must be resolved before recycling unlocks.
7. Select **Move to Trash**, confirm the final file count and total size, and proceed. Recycling applies to every candidate in the current report after the host revalidates it against current project files.
8. Review moved files, any error and unprocessed files. Cancellation or errors stop further moves while already-moved files stay in the system Trash; run a fresh scan before cleaning up again.

Additional keep paths are saved to `EditorCache/ResourceCleanupKeep.list` when scanning, saving the list or closing the window. Opening with an empty list creates no file, clearing and saving deletes the file, and built-in entries are never copied into it. If saving fails, the window stays open and displays the error.

## Scan boundaries

> **Important:** Resource Cleanup performs static analysis only — it never executes Lua, traces gameplay or infers concatenated or runtime-computed paths. Add dynamic resource paths to the keep list and review every candidate even when scanning reports zero issues.

- **Scanned sources:** Reads every ordinary map and world child map, all Lua sources and all supported structured Data files. Data dependencies are traced before evaluating Assets, so an unreferenced Data file does not keep its own Asset dependencies alive. Blueprint fields and map-instance overrides are traversed recursively using metadata, including resource references inside components and typed containers.
- **Protected vs candidate types:** Maps, `Data/General`, `Data/UI`, `Data/Configs`, `Data/Locale` and their supported references stay protected. Data candidates are restricted to Materials, Tilesets, AutoTiles, Blueprints, CommonFunctions, Animations, Particles, Curves and TextConfigs (unknown Data formats are never candidates). Lua scripts are scanned for references but never deleted, generated UI mirrors and declaration stubs do not count as independent usage, and material caches live in the user data directory managed by [runtime generation](<../../Editor User Guide/Materials.md#runtime-generation>).
- **Lua expression support:** Recognises string literals, parentheses, constant aliases and `and`/`or` conditional branches in known resource API arguments, conservatively retaining constant paths from every branch and following their Data dependencies.
- **Filesystem safety checks:** Symbolic links, broken links, reparse points and inaccessible paths (including the project root) produce scan issues and block recycling; checks repeat before moving files. Keep entries may name paths that do not exist yet, but cannot bypass safety checks on existing ancestor directories.

## Recycling and editor state

Recycling follows the same Trash, failure and recovery rules as [resource deletion in the editor](<../../Editor User Guide/Start Page Workspace and Shortcuts.md#file-explorer-and-reference-tree>). The project must be saved before scanning, reports cannot be reused after recycling, and any modified project input invalidates the current report.

## Plug-in contract

Official Resource Cleanup registers one ordinary menu command and owns its Avalonia window, built-in protected list and translations. `PluginMenuContext.ResourceCleanupHost` supplies the narrow `IResourceCleanupHost` bridge for reading/saving project keep paths, scanning with progress and cancellation, and recycling a report by ID without exposing mutable editor documents or arbitrary file deletion.
