# Start Page, Workspace and Shortcuts

## Start page

The Start page lists up to three recently opened projects and provides **New Project** and **Open Project**. Opening a validated `.proj` file moves it to the top of the list. The Start page removes unavailable entries as it loads. Opening `Main.proj` replaces the current project only after the unsaved-work checks succeed.

Dropping an existing `.proj` file anywhere on the Start page opens it. When several files are dropped, the editor uses the first existing `.proj` file and ignores the others.

An unsupported project version cannot be opened. See [Main.proj](<Project Settings and Asset Layout.md#mainproj>) for supported versions.

## Main workspace

The main window contains the map list, map canvas, editing-mode inspector and **World Outliner**, with **File Explorer**, **Actor Library** and **Console** tabs below. Tile and Actor mode controls sit beside the canvas and are hidden for world maps. The toolbar provides **Construct → Export → Play** / **Stop**; Standalone projects hide Construct. See [Run, Debug and Package](<Run Debug and Package.md>).

Resource windows display a loading indicator while opening. If loading fails, use **Retry**.

![The Ludork main workspace with its map and project panels](../../../images/overview/editor-workspace.png)

*The visible inspector and canvas interaction change with the selected editing mode.*

## Online documentation

**F1** opens the online manual inside the editor. The installed editor version selects its matching major.minor documentation: 1.0.7 opens `/Ludork/docs/v1.0/`. The current editor language is selected initially; language and version selectors remain available beside the chapter tabs. About and third-party notices also load online, while About retains the installed package version.

The embedded pages hide website navigation. Documentation links stay in the window; the homepage, downloads and external links open in the system browser. An internet connection is required. Failed page loads offer **Retry**. Windows requires Microsoft Edge WebView2 Runtime and offers an installation link when it is missing; macOS uses its system WKWebView.

## Global editor preferences

`Ludork.ini` stores preferences shared by all projects and restores them when the editor starts.

| Area | Saved preferences |
|---|---|
| Main window | Normal position and size, maximised state, map-list and inspector widths, lower-area height, Actor outliner/inspector proportions and the world-map child-list width. |
| File Explorer | Icon/list view, content zoom, expanded/collapsed source list and preferred external open target. |
| Actor Library | Icon/list view. |
| Tool windows | Position, normal size and maximised state for each window type. |
| Tool content | Existing adjustable split widths or proportions, the UI timeline and system-control category expansion, and particle configuration section expansion. |

Defaults are icon view in both panels, 100% File Explorer zoom, a collapsed source list and the system file manager. Missing or invalid preferences use defaults. Project browsing and map selection stay in `Main.proj`; global preferences do not modify project resources or restore search text, input drafts, layer selection or canvas view state.

Window layouts adapt to the current monitors. For each window type, the most recent adjustment is restored; minimised state is not restored.

[Viewport Options](<Tilesets Autotiles and Maps.md#viewport>) are saved immediately in the project's `Main.proj` under `editor.mapViewport` and restored when the project is reopened. Projects without saved options start in Unlit with Realtime, fog, panorama and Actor origins enabled, and light bounds disabled.

## Global shortcuts

The primary modifier is `Ctrl` on Windows and `Command` on macOS.

| Shortcut | Action |
|---|---|
| `Ctrl/Command+N` | New Project |
| `Ctrl/Command+O` | Open Project |
| `Ctrl/Command+S` | Save |
| `Ctrl/Command+Z` | Undo |
| `Ctrl/Command+Shift+Z` or `Ctrl/Command+Y` | Redo |
| `Ctrl/Command+W` | Exit or close the project window |
| `Ctrl/Command+1` | Tile mode |
| `Ctrl/Command+2` | Actor mode |
| `F1` | Ludork Documentation |
| `F3` | Game Config |
| `F4` | System Config |
| `F5` | Animation Overview |
| `F6` | Common Particle Overview |
| `F7` | Tilesets Data |
| `F8` | Common Functions |
| `F9` | Game Variables |
| `F10` | General Data |

Some document windows add context-specific shortcuts. In the Blueprint editor, `F2` renames the selected event tab. In the Common Functions window it renames the selected Common Function, in the file explorer it renames the selected file, except on macOS, and in the animation editor it renames the selected time tag. No window displays an `F2` hint.

Single-button hint and error dialogs close with `Enter`, `Return`, `Space` or `Escape`. In confirmation dialogs, `Enter`, `Return` and `Space` confirm the action, while `Escape` cancels it.

## Editing and saving

Displayed field defaults are written only when the field is edited.

File fields that can be cleared offer **Clear** as the first item in the file selector, in every directory and for every file filter. Select it and choose **Open**, press Enter or double-click it to clear the field. **Cancel** or Escape keeps the previous value. Save dialogs, multi-file imports and selectors that require an actual file do not offer this item.

Managed resource changes stay in memory until Save. All Save buttons and `Ctrl/Command+S` write every modified document, including unopened resources, and Construct, Export, Play and Pack use the same workflow. Successful saves show no result dialog; a failed save reports only the failed items and their errors. Normal Save validates changed Blueprints, while forced saving can retain unfinished graphs. [Game Config](<Project Settings and Asset Layout.md#mainini>) writes immediately on confirmation.

Saving and renaming resources temporarily disable editing, project switching and window closing. Longer operations show progress and cannot be cancelled. If project data changes during an operation, the editor asks you to retry.

Undo and Redo apply only to the active file, which keeps its 100 most recent entries. Windows that show the same file share its data and history, including after they are closed and reopened. Continuous text or numeric input in one field merges into a single step. Changing the control or the editing view, an automatic update, Save, Undo and Redo all end that step. Buttons, check boxes and selections each remain separate steps.

Undo cannot restore a reference to a deleted resource; a rejected step leaves the document and history unchanged.

Automatic reference updates create a marked Undo barrier in every changed target file, including unopened files. Later edits remain undoable, but Undo cannot cross the barrier and reports the barrier's source. Undoing or redoing the source rename updates references against the current target content and adds a new barrier, which preserves unrelated edits. Saving preserves the history and the barriers, so Undo can mark a saved file as modified again.

Modified files show `*` in the file explorer and in editor titles, and they are marked in resource selectors. Closing or replacing a project lists unsaved files and offers **Save**, **Discard** and **Cancel**. **Save** writes every modified document. Save failures retain the modified markers and prevent closing. Closing a resource window keeps its unsaved data until the project closes.

## File explorer and reference tree

File Explorer browses `Assets` and `Data` through the source sidebar and breadcrumbs. Up stops at the selected root. The last valid browsing directory is restored, defaulting to `Assets`.

Right-click an entry for file operations or empty space for the current directory menu. Paste targets the current browsing directory.

Right-click a file or folder and choose **Show in File Explorer** on Windows or **Show in Finder** on macOS to open its containing directory and select it. This also works in expanded folders and search results. With multiple entries selected, it reveals only the right-clicked entry. The action is disabled for entries that do not yet exist on disk and does not save pending changes.

In list view, folder arrows expand or collapse nested folders in place; double-click a folder to enter it. `Right` expands a folder or selects its first child; `Left` collapses it or selects its parent. Expansion is remembered for the project session. Icon view and search results remain flat.

Search matches file names in the current directory and its subdirectories, ignoring case. Results include unsaved resources and display relative paths. Clearing the field restores browsing; changing directories or locating a file clears the search. Symbolic-link folders are excluded from expansion and recursive search.

Use `Ctrl/Command` with the mouse wheel or the bottom-right slider to scale icons and text from 50% to 200%. Press F5 to refresh external file changes.

Image and video entries show thumbnails when the format is supported. Selecting a Blueprint JSON file in a file selector previews its current appearance and animation, including unsaved changes; a missing appearance or texture displays the file icon.

Select an image, audio or video file in File Explorer and press **Space** to preview it. File selectors also preview audio and video in their right-hand pane. Playback starts paused; use the playback, seeking and volume controls, or **Escape** to close the preview or cancel selection.

Media previews support common audio and video formats, subject to the system’s codecs. Windows requires Microsoft Edge WebView2 Runtime. Previewing does not require the project’s FFmpeg option or a native game build, and files stay on your machine. Unsupported or damaged files display an error.

The reference tree shows which resources a file uses and which files use it. Create, rename and move managed resources through the editor to update recognised references. Arbitrary strings and references in source code must be updated manually.

Creating or copying a managed resource leaves it unsaved and adds no Undo step for the creation. A rename appears immediately in the editor. Global Save then writes the new paths and the updated references together. Deletion requires every recognised reference to be removed and saved first. Confirmed deletion is immediate and cannot be undone.

Copy/paste name conflicts produce names such as `name_copy.ext`, then `name_copy2.ext`, without overwriting existing files or folders.

The open menu at the upper right always offers the system file manager, which opens the current browsing directory. Selecting an entry saves it as the preferred open target. If it is unavailable in another project or on the current machine, the button uses the system file manager while retaining the saved preference. It lists VS Code or Cursor only when the corresponding application is installed. C++ Source projects also list an installed CLion, and Visual Studio 2022 on Windows. VS Code, Cursor and CLion open the project root regardless of the directory displayed in the file explorer. When the local `.idea` or `.vs` configuration is absent, opening CLion or Visual Studio for the first time prompts before running the project-provided environment generator. Cancelling leaves the project unchanged. Visual Studio opens the generated `Main.sln` at the project root.

## Console and performance monitor

The Console collects plug-in, build and runtime output. Input can be sent only while a compatible runtime connection is ready. **Edit → Development Tools and Settings → Performance Monitor** displays recent frame, logic and memory samples while the game is running.

### World-streaming samples

When profiling a composite world, the Performance Monitor includes an optional `worldStreaming` snapshot. Ordinary-map and unprofiled ticks do not include it.

| Field | Meaning |
|---|---|
| `queueDepth` | Child regions waiting in the streaming queue. |
| `reading` | Regions currently in `Reading`. |
| `prepared` | Loaded regions in `Prepared`. |
| `active` | Regions in `Active`. |
| `dormant` | Loaded regions in `Dormant`. |
| `cacheBytes` | Conservative estimate used by the non-Active region cache. It is not exact process, Lua, native or GPU memory. |
| `publishMilliseconds` | Time spent converting and publishing regions during the logic tick. |
| `visibleTileChunks` | Unique 32×32-cell TileLayer chunks submitted by visible child layers in the latest draw. |
| `activeActors` | Actors currently present in the world's Active update/render batches. |

![Console output after a Game project run](../../../images/editor/console-output.png)

*The Console records launch progress, runtime diagnostics and the final process result.*

![Performance Monitor waiting for runtime telemetry](../../../images/editor/console-performance.png)

*A waiting monitor is normal when no compatible runtime session is active; the timeline fills after telemetry is connected.*

## Related pages

- [Common Particles](<Common Particles.md>)
- [Project Settings and Asset Layout](<Project Settings and Asset Layout.md>)
- [Run, Debug and Package](<Run Debug and Package.md>)
