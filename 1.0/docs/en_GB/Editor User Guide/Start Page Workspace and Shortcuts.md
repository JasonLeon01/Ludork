# Start Page, Workspace and Shortcuts

## Start page

The Start page lists up to three recently opened projects and provides **New Project** and **Open Project**. Opening a validated `.proj` file moves it to the top of the list. The Start page removes unavailable entries as it loads. Opening `Main.proj` replaces the current project only after the unsaved-work checks succeed.

Dropping an existing `.proj` file anywhere on the Start page opens it. When several files are dropped, the editor uses the first existing `.proj` file and ignores the others.

An unsupported project version cannot be opened. See [Main.proj](<Project Settings and Asset Layout.md#mainproj>) for supported versions.

## Main workspace

The **Map List** is on the left, with the map canvas in the centre and layer tabs above it. The right panel shows the tile palette in Tile mode, or **World Outliner** and **Actor Info** in Actor mode. **File Explorer** tabs and **Console** share the tab strip below the canvas. Tile and Actor mode controls sit beside the canvas and are hidden for world maps. The toolbar provides **Construct → Export → Play** / **Stop**; Standalone projects hide Construct. See [Run, Debug and Package](<Run Debug and Package.md>).

Resource windows display a loading indicator while opening. If loading fails, use **Retry**.

## Online documentation

**F1** opens the online manual inside the editor. The installed editor version selects its matching major.minor documentation: 1.0.7 opens `/Ludork/docs/v1.0/`. The current editor language is selected initially; language and version selectors remain available beside the chapter tabs. About and third-party notices also load online, while About retains the installed package version.

The embedded pages hide website navigation. Documentation links stay in the window; the homepage, downloads and external links open in the system browser. An internet connection is required. Failed page loads offer **Retry**. Windows requires Microsoft Edge WebView2 Runtime and offers an installation link when it is missing; macOS uses its system WKWebView.

## Global editor preferences

Preferences shared by all projects are stored in `~/Ludork/Ludork.plist` on macOS and `~/Ludork/Ludork.ini` on other platforms, and restored when the editor starts. The macOS file uses the [XML plist configuration structure](<../Getting Started/Ludork Lua Advanced.md#platform-configuration-and-files>). Its directory stays `~/Ludork`; an existing macOS `Ludork.ini` is neither read nor migrated. A missing plist starts with the default preferences.

| Area | Saved preferences |
|---|---|
| Main window | Normal position and size, maximised state, map-list and inspector widths, lower-area height, Actor outliner/inspector proportions and the world-map child-list width. |
| File Explorer | Initial icon/list view, content zoom and expanded/collapsed source list for a new workspace, plus the preferred external open target. |
| Tool windows | Position, normal size and maximised state for each window type. |
| Tool content | Existing adjustable split widths or proportions, the UI timeline and system-control category expansion, and particle configuration section expansion. |

File Explorer defaults to icon view, 100% zoom, a collapsed source list and the system file manager. Saved project tabs use their own display preferences; changing one tab does not change other open tabs. Missing or invalid preference values use defaults. An unreadable or malformed plist is reported and is not overwritten by preference saves; repair it and restart the editor before saving preferences again. Project browsing and map selection stay in `Main.proj`; global preferences do not modify project resources or restore search text, input drafts, layer selection or canvas view state.

Window layouts adapt to the current monitors. For each window type, the most recent adjustment is restored; minimised state is not restored.

Project-specific display preferences are described in [Viewport options](<../PreviewHost/Map and World Previews.md#viewport-options>).

## User data directory

Windows and macOS editors use `~/Ludork` for persistent data in installed, portable and repository runs. `~` is the current user's home directory: typically `C:\Users\<user>` on Windows and `/Users/<user>` on macOS.

| Data | Path below `~/Ludork` |
|---|---|
| Preferences, recent projects and window layouts | `Ludork.plist` on macOS; `Ludork.ini` on other platforms |
| Global and project plug-ins, registrations and retained data | `Plugins/` and `plugins.json` |
| Downloaded templates, all four variants | `Templates/v<base-version>/` |
| Signing profile indexes | `macos-signing.json`, `ios-signing.json`, `harmony-signing.json`, `android-signing.json` |
| Local server test settings | `.localserver/<project-directory-name>/test.json` |
| Editor Console output for a project | `Log/<project-key>/Ludork.log` |
| Windows embedded-browser data | `WebView2/` |

Plug-in discovery also reads the working directory’s registry and sources, with working-directory IDs taking priority over user copies. Retained plug-in data stays in `~/Ludork`; see [Global and project storage](<../Plug-in Development and Installation/Installing and Managing Plug-ins.md#global-and-project-storage>).

Project files, build output and `EditorCache` remain in each project. Game runtime settings and save files retain their own locations. Passwords continue to use the operating system's credential store; macOS manages its WebKit cache separately.

Windows MSI and EXE uninstallers offer **Delete user data**, selected by default. Keeping it selected deletes the verified initiating user's entire `~/Ludork` directory, including all plug-in sources, registrations and retained data, preferences, logs, template caches, browser data and signing indexes. Close Ludork and back up anything needed before uninstalling. Clear the option to retain this directory. Projects in `~/LudorkProjects` or other directories outside `~/Ludork`, and other users' data, are not deleted. Removing the macOS application continues to leave `~/Ludork` intact.

For silent MSI removal, pass `REMOVE_USER_DATA=0` to retain data or `REMOVE_USER_DATA=1` to delete it; the default is `1`. Cleanup runs only for an explicit full uninstall, after the removal transaction commits. Repair and the removal of an older version during a major upgrade do not delete user data. A deletion failure is reported and may leave data requiring manual cleanup. Service accounts and uninstall sessions whose initiating user cannot be verified must use `REMOVE_USER_DATA=0`.

The EXE uninstaller accepts `/REMOVEUSERDATA=0` or `/REMOVEUSERDATA=1`, also defaulting to `1`; silent removal honours the same choice. It retains the existing Windows uninstall entry. If another administrator's account supplies UAC credentials, no interactive desktop is available, or the initiating user's identity cannot be confirmed, it preserves user data and prompts for manual deletion, or records that instruction during silent removal. It does not substitute the administrator's or desktop user's directory. If manual cleanup is needed, sign in as the intended user and delete only that user's `~/Ludork` after preserving anything needed.

### Moving existing editor data

Apart from the documented working-directory plug-in scan, old storage locations are not read or migrated automatically. Close Ludork and back up anything needed **before uninstalling an older version**. Windows previously stored editor data in the installation directory, while repository runs could use the working directory for settings and the repository for plug-ins. macOS settings and signing indexes previously lived in `~/Library/Application Support/Ludork`; its published plug-ins and template cache already used `~/Ludork`.

After the new installation finishes, restore the required preferences file in the [current platform format](<#global-editor-preferences>), signing indexes, template cache and local server settings to the matching locations above. Restore `Plugins` and `plugins.json` together from the same backup. To preserve existing plug-ins during installation, clear the EXE's **Install Official Plugins** option or pass `INSTALL_OFFICIAL_PLUGINS=0` to MSI, then restore the backup. Restoring plug-ins before an installation that retains the default plug-in option would replace them again. Back up any existing destination files before replacing them. System credential-store entries and project files do not need to move.

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

Managed resource changes and asset file or folder renames and moves stay in memory until Save; Discard restores the last saved paths. All Save buttons and `Ctrl/Command+S` write every modified document, including unopened resources, and Construct, Export, Play and Pack use the same workflow. Successful saves show no result dialog; a failed save reports only the failed items and their errors. Normal Save validates changed Blueprints, while forced saving can retain unfinished graphs. [Game Config](<Project Settings and Asset Layout.md#game-configuration>) writes immediately on confirmation.

Saving, resource deletion and file transfers, renaming, Undo and Redo temporarily disable editing, project switching and window closing. Longer operations show an animated progress dialog after a short delay, including reference checks before deletion. The dialog reports the current stage and, when available, the current file and completed count. It gives way to any required confirmation and closes when the operation finishes or fails. These editing operations cannot be cancelled midway. If project data or external files change during validation, the editor asks you to retry.

Undo and Redo apply only to the active file, which keeps its 100 most recent entries. Windows that show the same file share its data and history, including after they are closed and reopened. Continuous text or numeric input in one field merges into a single step. Changing the control or the editing view, an automatic update, Save, Undo and Redo all end that step. Buttons, check boxes and selections each remain separate steps.

Undo cannot restore a reference to a deleted resource or to an unavailable asset path or General Data member ID renamed during the session; a rejected step leaves the document and history unchanged.

Automatic reference updates create a marked Undo barrier in every changed target file, including unopened files. Later edits remain undoable, but Undo cannot cross the barrier and reports the barrier's source. Undoing or redoing the source rename updates references against the current target content and adds a new barrier, which preserves unrelated edits. Saving preserves the history and the barriers, so Undo can mark a saved file as modified again.

To Undo or Redo an asset path change, select the file or folder in File Explorer. Undoing a folder move preserves later independent changes to its children. Save pending moves before copying, deleting, importing, creating or writing files at their source or destination paths; further renames and moves remain available. Destination conflicts reject the whole operation.

For external-change conflicts, **Keep memory** accepts the current disk contents while retaining pending paths, updated references and Undo/Redo history. **Use disk** cancels the affected pending moves and restores recognised references to the last saved paths, preserving unrelated edits.

If a source is missing or a destination is occupied, **Keep memory** fails without partial changes. **Use disk** can still cancel the moves, but does not restore missing files or clear their references; restore the files or update those references yourself.

Modified files show `*` in the file explorer and in editor titles, and they are marked in resource selectors. Closing or replacing a project lists unsaved files and offers **Save**, **Discard** and **Cancel**. **Save** writes every modified document. Save failures retain the modified markers and prevent closing. Closing a resource window keeps its unsaved data until the project closes.

## File explorer and reference tree

File Explorer browses `Assets` and `Data` through the source list and breadcrumbs. Up stops at the selected root.

Choose **View → New File Explorer Tab**, or right-click a File Explorer tab and choose **New File Explorer Tab**, to open another browser beside the existing tabs and Console. The first **File Explorer** tab stays open; additional tabs have numbers and a **×** close control. A new tab copies the source tab's current directory and display preferences, with an empty search, navigation history and selection. The context menu uses the right-clicked tab as its source; the View menu uses the most recently active File Explorer. Closing the active additional tab selects the File Explorer immediately to its left.

Each tab keeps its own browsing state. Use **Back** and **Forward**, `Alt+Left` and `Alt+Right`, or the mouse side buttons to revisit directories. History also restores that visit's search, expanded folders, selection, scroll position and display preferences. Navigating to a new directory after going back clears the forward history. Refreshing, changing a selection or search, and switching tabs do not add history entries.

Resource **Locate** actions use the most recently active File Explorer, including when Console is selected. Reopening the project restores the browser tabs, their directories and display preferences, and the active browser; search, navigation history, selection and scroll position last only for the current session. The saved fields and directory recovery rules are described in [Main.proj](<Project Settings and Asset Layout.md#mainproj>).

Right-click an entry for file operations or empty space for the current directory menu. Paste targets the current browsing directory.

Right-click a file or folder and choose **Show in File Explorer** on Windows or **Show in Finder** on macOS to open its containing directory and select it. This also works in expanded folders and search results. With multiple entries selected, it reveals only the right-clicked entry. The action is disabled for entries that do not yet exist on disk and does not save pending changes.

In list view, folder arrows expand or collapse nested folders in place; double-click a folder to enter it. `Right` expands a folder or selects its first child; `Left` collapses it or selects its parent. Expansion is remembered for the project session. Icon view and search results remain flat.

Search matches file names in the current directory and its subdirectories, ignoring case. Results include unsaved resources and display relative paths. Clearing the field restores browsing; changing directories or locating a file clears the search. Symbolic-link folders are excluded from expansion and recursive search.

Use `Ctrl/Command` with the mouse wheel or the bottom-right slider to scale icons and text from 50% to 200%. Press F5 to refresh external file changes.

Image and video entries show thumbnails when the format is supported. For Blueprint JSON entries, see [Actor previews](<../PreviewHost/UI Actor Particle and Subtitle Previews.md>).

Select an image, audio or video file in File Explorer and press **Space** to preview it. File selectors also preview audio and video in their right-hand pane. Playback starts paused; use the playback, seeking and volume controls, or **Escape** to close the preview or cancel selection.

Media previews support common audio and video formats, subject to the system’s codecs. Windows requires Microsoft Edge WebView2 Runtime. Previewing does not require the project’s FFmpeg option or a native game build, and files stay on your machine. Unsupported or damaged files display an error.

The reference tree shows which resources a file uses and which files use it. Create, rename and move managed resources through the editor to update recognised references. Arbitrary strings and references in source code must be updated manually.

Creating or copying a managed resource leaves it unsaved and adds no Undo step for the creation. A rename appears immediately in the editor. Global Save then writes the new paths and the updated references together. Deletion requires every recognised reference to be removed and saved first. Confirmed deletion in File Explorer and resource lists immediately moves saved files and folders to the Windows Recycle Bin or macOS Trash. If the system cannot recycle an item, the editor reports the error and keeps it; it never falls back to permanent deletion. In a batch, successfully recycled items remain deleted even if another item fails.

Recycling preserves the last saved disk contents and path, including when a resource has an unsaved rename. Unsaved edits are discarded. A new resource that has never been saved is removed from the editor without creating a recoverable copy. Successfully deleted resources are removed from managed documents and resource lists, so a later Save does not recreate them. Editor Undo cannot restore resource deletion. Restore saved files through the operating system, then reopen the project to load them again. Deleting items within a document, such as Actors, graph nodes and map layers, continues to use that document's Undo history.

Copy/paste name conflicts produce names such as `name_copy.ext`, then `name_copy2.ext`, without overwriting existing files or folders.

The open menu at the upper right always offers the system file manager, which opens the current browsing directory. Selecting an entry saves it as the preferred open target. If it is unavailable in another project or on the current machine, the button uses the system file manager while retaining the saved preference. It lists VS Code or Cursor only when the corresponding application is installed. C++ Source projects also list an installed CLion, and Visual Studio 2022 on Windows. VS Code, Cursor and CLion open the project root regardless of the directory displayed in the file explorer. When the local `.idea` or `.vs` configuration is absent, opening CLion or Visual Studio for the first time prompts before running the project-provided environment generator. Cancelling leaves the project unchanged. Visual Studio opens the generated `Main.sln` at the project root.

## Console and performance monitor

The Console collects plug-in, build and runtime output. Input can be sent only while a compatible runtime connection is ready. **Edit → Development Tools and Settings → Performance Monitor** displays recent frame, logic and memory samples while the game is running.

Console output is saved to `~/Ludork/Log/<project-key>/Ludork.log`. The key is derived from the project's absolute path, so projects with the same directory name at different paths have separate logs. A new logging session archives the preceding log with its modification timestamp. Moving or renaming a project starts a different log directory; existing logs remain available.

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

![Performance Monitor waiting for runtime telemetry](../../../images/editor/console-performance.png)

*A waiting monitor is normal when no compatible runtime session is active; the timeline fills after telemetry is connected.*

## Related pages

- [Common Particles](<Common Particles.md>)
