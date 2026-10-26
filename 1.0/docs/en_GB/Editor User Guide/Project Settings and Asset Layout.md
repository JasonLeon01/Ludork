# Project Settings and Asset Layout

A project stores editor and packaging settings in `Main.proj`, runtime settings in its [game configuration](<#game-configuration>), resources in `Assets`, authored data in `Data` and runtime code in `Scripts`. C++ Source projects also include the native sources described in [Build and Module Layout](<../Native C++ Development/Build and Module Layout.md>).

## Main.proj

`Main.proj` describes editor and packaging capabilities. The Game project enables the C++ runtime and FFmpeg and stores the preferred run-window mode:

```json
{
  "version": "v1.0",
  "Cpp": true,
  "ffmpeg": true,
  "IndividualWindow": true,
  "packaging": {
    "version": "1.0.0",
    "dev": false
  },
  "editor": {
    "fileExplorer": {
      "tabs": [
        {
          "number": 1,
          "path": "Assets",
          "iconView": true,
          "zoom": 100,
          "sourcesExpanded": false
        }
      ],
      "activeTabNumber": 1
    }
  }
}
```

The required top-level `version` is the engine compatibility version, written as `v<major>.<minor>`. For example, editor `1.0.7` creates `"v1.0"` projects. This edition supports `v1.0`. A missing, malformed or unsupported version prevents the project from opening. Opening a supported older project does not migrate its contents or change its version.

`packaging.version` independently stores the **game's** base release version, and `packaging.dev` selects internal test packages. Missing packaging values default to `"1.0.0"` and `false`; these defaults do not apply to the required top-level project version. Use Pack Options to save packaging settings, or override them for one command-line invocation; see [Release version and internal packages](<Run Debug and Package.md#release-version-and-internal-packages>).

Treat unknown keys as editor-owned state and preserve them. Choose the FFmpeg template to match the `ffmpeg` setting.

`editor.fileExplorer` stores the project browser workspace. `tabs` preserves tab order; each entry records its `number`, project-relative `path`, `iconView`, `zoom` (50–200) and `sourcesExpanded`. `activeTabNumber` identifies the most recently active browser. Tab 1 is always present and cannot be closed. Without saved workspace state, the editor opens it at `Assets` using the [global initial display preferences](<Start Page Workspace and Shortcuts.md#global-editor-preferences>).

Browser paths stay beneath `Assets` or `Data`. If a stored directory has disappeared, reopening uses its nearest existing parent within the same root; an invalid, absolute, linked or out-of-root path uses `Assets`. Search, navigation history, selection and scroll position are not written to `Main.proj`. Browser operations are described in [File explorer and reference tree](<Start Page Workspace and Shortcuts.md#file-explorer-and-reference-tree>).

The editor stores the last selected map key in `lastOpenedMapKey` and saves it immediately when the editing selection changes, without saving map content. Reopening the project restores that map, including world maps and their child maps. Missing or invalid records select the first available entry; an empty project has no selection. Live-debug map changes do not overwrite this preference.

Editor appearance and tool preferences are shared independently of this project workspace state. See [Global editor preferences](<Start Page Workspace and Shortcuts.md#global-editor-preferences>).

## Game configuration

Open **Game → Game Config** (`F3`) to edit the game’s language, display and audio settings.

macOS stores these settings in `Main.plist`, using XML plist. Other platforms, including iOS, use `Main.ini`. The section names, keys and defaults are the same in both formats. macOS does not read or migrate an existing `Main.ini`; if `Main.plist` is absent, new settings use the defaults and the INI file is left untouched. The plist structure is described in [Platform, configuration and files](<../Getting Started/Ludork Lua Advanced.md#platform-configuration-and-files>).

The language list uses the exported `Scripts/Source/Locale/*.lua` files, excluding `Core.lua`, and refreshes whenever Game Config opens. The current configured language remains available even if its export is missing. Export workbook changes before selecting a newly added language.

**Confirm** saves immediately; **Cancel** keeps the previous values. A failed write leaves the dialog open with an error. An unreadable or malformed plist is reported, and Confirm cannot overwrite it with defaults; repair the file before saving. These settings are outside document Undo and unsaved changes.

Game Config always shows Scale in the editor. The in-game Config window shows that row only when `Display.isDisplayScaleConfigurable()` reports that the host supports display scaling.

Windows, macOS and Linux desktop games use `~/<APP_NAME>` for configuration, `Save/` and runtime caches in editor Play, IDE launches, direct runs and packaged games. `~` is the current user's home directory, typically `C:\Users\<user>` on Windows and `/Users/<user>` on macOS. iOS uses `~/Library/Application Support/<APP_NAME>`; HarmonyOS and Android use `<APP_NAME>` below their app-private user-data root.

`APP_NAME` remains the static declaration in `Scripts/Entry.lua`, described in [Packaging](<Run Debug and Package.md#packaging>). Projects with the same name share configuration, saves and caches for the current user. Changing the name selects another directory; existing files are not migrated. Project-local configurations and `Save/` are neither read nor written, and an old local file does not supply defaults. Use [the runtime path APIs](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#configuration>) to resolve these locations. See [Game Settings Reference](<../Lua and Blueprint Scripting/Default Gameplay/Game Settings Reference.md>) for the keys, choices, defaults and application timing.

## System Config

Open **Database → System Config** (`F4`) to edit the configuration files under `Data/Configs`. The left-hand list shows each configuration by its relative key; selecting **Audio**, **System** or **Gameplay** displays only that configuration's fields on the right.

Configuration files use JSON objects with `type: "system"` or no `type`. Their keys are paths relative to `Data/Configs`, without the extension.

**Gameplay** configures shared gameplay settings. Each row has **Use default** on the right. Selecting it hides the value editor and saves `null` as the field's `value`, so the game uses the script default. New Gameplay data starts with every row selected. Select Use default again to clear a configured override.

Clear **Use default** to fill the editor with the current script default and create an override. The editor reads the saved Lua source each time, so save script changes first. An existing override does not follow later script changes; select and then clear Use default to read the default again. If the default cannot be read, the field stays on Use default and displays the reason.

Enum fields use a drop-down; numeric and text fields accept typed input, and boolean fields use a checkbox. Clearing a numeric input restores Use default. An empty string and `false` remain explicit values; use the Use default checkbox to clear those overrides.

A numeric zero is an explicit value, not Use default; enter a positive value for the save-slot count and tutorial duration before saving. The [shared Gameplay configuration reference](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#shared-gameplay-configuration>) lists the supported settings and their script defaults. Save the configuration and restart the game to apply it.

Switching configurations keeps pending edits and does not save automatically. An asterisk in the list and window title marks changes. Use Ctrl+S on Windows or Command+S on macOS for global Save; Undo/Redo applies to the configuration selected in the list.

![System Config with a configuration list on the left and the selected form on the right](../../../images/editor/system-config.png)

*Select System to edit the game's title, dimensions and startup settings, or Audio to edit sound and music resources.*

## Assets

Authoring files live under the physical `Assets` directory. Stored resources use exact-case `/Game/Assets/...` paths with `/` separators and file extensions. Paths such as `Assets/...`, category-relative paths, native absolute paths and traversal paths are invalid. `PathVars` marks a field as a validated canonical `/Game/Assets/...` reference, unless `PathRoot` selects `"Project"` files or `"Data"` documents. Config `base` only sets the picker root. `/Game` works through Ludork resource APIs, while Standard filesystem APIs and direct LuaSF file constructors use native paths.

Resource resolution rejects symbolic links, broken links and reparse points from the target through the project root. An inaccessible path is rejected. Path conversion and picker roots may name a file or directory that has not been created yet, provided the existing ancestors pass the same checks. Stored resource paths remain case-sensitive on every platform.

Keep the conventional first-level folders so that selectors can filter correctly: `Tilesets`, `Autotiles`, `Characters`, `Animations`, `Particles`, `Icons`, `Fonts`, `Musics`, `Sounds`, `Voices`, `Shaders`, `Panoramas`, `Transitions`, `System` and `Videos`. The `.ldpak` option archives the complete `Assets` tree as root `Assets.ldpak`, including ordinary files directly under `Assets`. Logical resource paths remain unchanged.

Rename or move files and folders within `Assets` through the editor to update recognised references. Renames outside the editor, handwritten source references and dynamically constructed paths require manual updates. For saving, Undo and pending-path restrictions, see [Editing and saving](<Start Page Workspace and Shortcuts.md#editing-and-saving>).

## Data and Scripts

`Data` stores editor-authored JSON and the localisation workbook:

| Location | References and purpose |
|---|---|
| `Data/Materials` | Full `Data/Materials/...json` paths; see [Materials](<Materials.md>) for graphs and runtime export. |
| `Data/Subtitles` | Full `Data/Subtitles/...json` paths; see [Video Subtitles](<Video Subtitles.md>). |
| `Data/Particles` | Extensionless relative keys such as `Combat/Hit`; see [Common Particles](<Common Particles.md>). |
| `Data/UI` | Extensionless relative asset keys; see [UI Asset Editor](<UI Asset Editor.md>). |
| `Data/TextConfigs` | Shared text styles and rich-text tags; ordinary UI text styles stay on their nodes. |
| `Data/Locale/Locale.xlsx` | Workbook managed by [Official Locale Tools](<../Plug-in Development and Installation/Official Plug-ins/Locale Tools.md>). |

`Scripts` contains runtime Lua, metadata and declarations. See [Lua Runtime and Modules](<../Lua and Blueprint Scripting/Lua Runtime and Modules.md>) for module locations and ownership, and [Declarative UI Authoring Workflow](<../Lua and Blueprint Scripting/Declarative UI/Authoring Workflow.md>) for UI assets and generated Views. [General Data and Text Config](<General Data and Text Config.md>) describes the files generated when saving project data.

Do not edit `Engine_meta.lua`, `GlobalCore_meta.lua`, `GlobalFunctions_meta.lua` or generated Core/LuaSF declarations by hand. Packaging removes `Scripts/stub` and rejects any misplaced `.d.lua`.

Keep `Data` and `Scripts` as loose directories while authoring. See [Run, Debug and Package](<Run Debug and Package.md#loose-and-packed-output>) for `.ldpak` output.

### Map and world-map assets

An ordinary map is `Data/Maps/<Name>.json`. A world is `Data/Maps/<WorldFolder>/` with exactly one `_world.json` and direct child-map `.json` files, and deeper nesting is not allowed. World loading reads only the direct `.json` files and ignores other regular files such as `.DS_Store`. The folder name is the world's path identity, while `_world.json` stores its independent `worldName`. Each direct child keeps its own editable `mapName`, so the file remains a complete map after it is moved out of the world, and the child file name remains its path and placement identity. The manifest is the world's selector and save identity, and it stays hidden in the Map List.

The manifest has this closed editor-owned shape:

```json
{
  "type": "worldMap",
  "worldName": "{WORLD_01}",
  "width": 256,
  "height": 192,
  "fog": {
    "enabled": false,
    "colour": [255, 255, 255, 255],
    "density": 0.5,
    "scale": 256,
    "velocity": [15, 15],
    "turbulence": 0.5,
    "seed": 0
  },
  "panorama": "",
  "layerOrder": ["floor", "default"],
  "placements": [
    {
      "map": "Map_01.json",
      "rect": [0, 0, 64, 64]
    }
  ]
}
```

`width`, `height` and `[x, y, width, height]` placement rects use zero-based cells. A rect must match the size of its child, and each child may appear once. Placements may touch, and uncovered cells are valid holes. Placements cannot extend beyond the world. The editor allows temporary overlaps while arranging maps, but rejects Save until they are resolved; saved manifests cannot contain overlapping placements.

`worldName` is a required non-empty display name. `layerOrder` is derived from the placed children and rejects contradictory cycles. The manifest owns the world display name, size, composition, global fog and global panorama, while audio, filters and ambient light remain child data.

Ordinary maps and worlds store fog in the same `fog` object. Configure its procedural world-space pattern through [Fog](<Tilesets Autotiles and Maps.md#fog>); it does not reference an asset image.

## Limitations

Map renames and moves update recognised map-file settings and destinations in `GotoMap` and `RecordTelepoint` graph nodes. Arbitrary strings and dynamically constructed paths must be updated manually.

## Related pages

- [Project Templates](<../Getting Started/Project Templates.md>)
