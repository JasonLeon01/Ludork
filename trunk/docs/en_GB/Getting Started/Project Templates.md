# Project Templates

Ludork projects start from either the **Standalone** or **C++ Source** template, each with an optional FFmpeg video-playback variant. Use this guide to choose a template, verify platform prerequisites and understand template downloads and layout.

## Template reference

**New Project** presents two template cards, **Standalone** and **C++ Source**, together with an independent **Enable video playback (FFmpeg)** switch.

Enter a project name for the new directory and a separate, required **APP_NAME** for the game. `APP_NAME` identifies the game's [user data directory](<../Editor User Guide/Project Settings and Asset Layout.md#game-configuration>) and [packaged application](<../Editor User Guide/Run Debug and Package.md#packaging>); choose a name dedicated to this game. An empty or invalid value keeps **Create** disabled, and renaming the project directory does not change `APP_NAME`.

Project creation substitutes `APP_NAME` into the template's `Scripts/Entry.lua.in` and writes an ordinary, editable `Scripts/Entry.lua` without the `.in` template. Later changes to the application name belong in the static `local APP_NAME` declaration in `Scripts/Entry.lua`.

| Template | Native source | Native compiler required | Use when |
|---|---|---|---|
| Standalone | No | No | Authoring Lua, Blueprint, data and assets with the prebuilt runtime. |
| C++ Source | Yes | Yes | Changing native services or bindings, or building for iOS, HarmonyOS, Android or WebGL. |

Each card and FFmpeg combination has its own template. To turn an existing **C++ Source** project into an editable **Standalone** project, use [Pack Project → Standalone project](<../Editor User Guide/Run Debug and Package.md#convert-source-to-a-standalone-project>); this creates a separate project that preserves the original export outputs rather than starting from a fresh template.

Before the first Play, run **Construct** and then **Export** for **C++ Source**; **Standalone** needs only **Export**. See [Run, Debug and Package](<../Editor User Guide/Run Debug and Package.md>) for subsequent builds and exports. **C++ Source** templates include a project-root `.clang-format` for native source formatting.

- **Windows:** Selecting **C++ Source** displays **Compiler** and **Get MinGW**. Choose an available compiler before creating the project; without one, **Create** is disabled. Detection prefers MSVC, then Ludork's downloaded MinGW, then any compiler on `PATH` including LLVM Clang, and downloading a compiler refreshes this list without restarting the editor. **Standalone** requires no compiler. See [Windows project compiler](<../Editor User Guide/Run Debug and Package.md#windows-project-compiler>) for detection, downloads and later changes.
- **macOS:** **C++ Source** requires a full Xcode installation; Command Line Tools alone do not qualify. **New Project** checks once when the window opens; if Xcode is missing, selecting **C++ Source** shows **Get Xcode** (opening Xcode in the Mac App Store) and disables **Create**. After installing Xcode, close and reopen **New Project** to check again. **Standalone** does not require Xcode.

## Obtaining templates

Development editor packages bundle local templates. If one is missing, **New Project** reports an error without downloading a replacement.

Release editors download templates from the matching GitHub Release. Windows x64 uses ZIP archives; macOS ARM64 uses `tar.gz` archives. Each release publishes eight template assets:

| Template | Windows x64 | macOS ARM64 |
|---|---|---|
| C++ Source | `game-source.zip` | `game-source.tar.gz` |
| C++ Source with FFmpeg | `game-source-ffmpeg.zip` | `game-source-ffmpeg.tar.gz` |
| Standalone | `game-standalone.zip` | `game-standalone.tar.gz` |
| Standalone with FFmpeg | `game-standalone-ffmpeg.zip` | `game-standalone-ffmpeg.tar.gz` |

Templates match the editor's complete version, including its patch number. If the matching release or template has not been published, **New Project** reports that it is unavailable.

All four downloaded variants are cached under `~/Ludork/Templates/v<base-version>/` for validation and offline reuse on Windows and macOS, independently of the installation directory (see [User data directory](<../Editor User Guide/Start Page Workspace and Shortcuts.md#user-data-directory>) for the native path). Bundled development templates are read-only inputs.

**New Project** displays download or copy progress and extraction progress. Selecting **Cancel** or closing the window during creation cancels the operation and deletes the unfinished project and download files; completed, valid cache entries stay cached for later use.

**New Project** always creates the project version matching the editor's `major.minor` version (such as `v1.0` for any `1.0.x` editor) and provides no older-version creation option. Opening supported older projects is handled by the [project-version check](<../Editor User Guide/Project Settings and Asset Layout.md#mainproj>).

## FFmpeg option

**Enable video playback (FFmpeg)** adds local MOV and MP4 playback for H.264 video and AAC audio, increasing build size. Other codecs, network playback and hardware acceleration are not included.

Windows and macOS use shared FFmpeg libraries, whereas iOS, HarmonyOS, Android and WebGL use static libraries. Distribution requirements, including LGPL relinking materials for static builds, are detailed in `Licenses/FFmpeg/README.md`. On macOS, selecting FFmpeg displays a compatibility notice.

## Template targets

- **Ludork Server:** Configuring [Ludork Server](<../Simple Server/Deployment and Editor Settings.md>) requires **C++ Source**; its Game test-server menu and Pack configuration are hidden in **Standalone**. All templates start with the server disabled and contain no test or production credentials. Converting an existing **C++ Source** project can embed explicitly enabled Release server settings in the resulting **Standalone** runtime.
- **Packaging options:** Script compilation, shader encryption, data encryption, save encryption and **Archive Assets, Data and Scripts (.ldpak)** are packaging options rather than template types. See [Run, Debug and Package](<../Editor User Guide/Run Debug and Package.md>) for complete option behaviour.
- **Excluded generated outputs:** Templates omit UI/locale export outputs (`Scripts/Internal/UI/`, `Scripts/stub/Internal/UI/`, `Scripts/stub/Internal/UIWindows/`, generated locale Lua files and `EditorCache/ProjectExport.json`, all in `.gitignore`), material caches and release `Data/RuntimeSettings.json`/`.ldc`. **C++ Source** also omits `build-web` and `dist`. Run **Export** for UI/locale files; the game [prepares materials at initialisation](<../Editor User Guide/Materials.md#runtime-generation>).
- **Included authored sources:** Templates include UI JSON, authored material graphs under `Data/Materials`, handwritten Controllers and declarations, `Scripts/Source/Locale/Core.lua`, General Data and variable Lua with declarations, native binding declarations and optional `Data/MaterialCombinations.json` for [additional material combinations](<../Editor User Guide/Materials.md#material-combinations>) (combined at initialisation with map combinations).
- **Language-tool declarations:** Templates include `Scripts/stub/**/*.d.lua` for Lua language tools; these declarations are stripped from [game packages](<../Editor User Guide/Run Debug and Package.md#loose-and-packed-output>).

Runtime licence texts live in `Licenses` (FFmpeg variants also include `Licenses/FFmpeg`), and asset notices sit beside their assets. Packaging copies available licence texts, but verifying completeness is the distributor's responsibility.
