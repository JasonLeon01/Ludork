# Project Templates

## Template reference

The picker presents two template cards, **Standalone** and **C++ Source**, and an independent **Enable video playback (FFmpeg)** switch.

| Template | Native source | Native compiler required | Use when |
|---|---|---|---|
| Standalone | No | No | Authoring Lua, Blueprint, data and assets with the supplied runtime. |
| C++ Source | Yes | Yes | Changing native services, bindings or building an iOS, HarmonyOS or Android target. |

Each card/FFmpeg combination has its own template. To turn an existing C++ Source project into an editable Standalone project, use [Pack Project → Standalone project](<../Editor User Guide/Run Debug and Package.md#convert-source-to-a-standalone-project>). This creates a separate project with its existing export outputs, rather than a fresh template.

Before the first Play, use **Construct** for C++ Source, then **Export**. Standalone needs only Export. See [Run, Debug and Package](<../Editor User Guide/Run Debug and Package.md>) for later builds and exports. C++ Source templates include a project-root `.clang-format` for native source formatting.

On Windows, selecting C++ Source shows **Compiler** and **Get MinGW**. Choose an available compiler before creating the project; without one, **Create** remains disabled. The initial preference is MSVC, then Ludork's downloaded MinGW, then a MinGW found on `PATH`. Downloading a compiler refreshes this list without restarting the editor. Standalone does not require a compiler. See [Windows project compiler](<../Editor User Guide/Run Debug and Package.md#windows-project-compiler>) for detection, downloads and later changes.

## Obtaining templates

Development editor packages include local templates. If one is missing, New Project reports an error without downloading a replacement.

Release editors obtain templates from the matching GitHub Release. Windows x64 uses ZIP archives; macOS ARM64 uses tar.gz archives. Each release provides eight separate template assets:

| Template | Windows x64 | macOS ARM64 |
|---|---|---|
| C++ Source | `game-source.zip` | `game-source.tar.gz` |
| C++ Source with FFmpeg | `game-source-ffmpeg.zip` | `game-source-ffmpeg.tar.gz` |
| Standalone | `game-standalone.zip` | `game-standalone.tar.gz` |
| Standalone with FFmpeg | `game-standalone-ffmpeg.zip` | `game-standalone-ffmpeg.tar.gz` |

Templates match the editor’s complete version, including its patch number. If a matching release or template has not been published, New Project reports that it is unavailable.

All four downloaded variants use `~/Ludork/Templates/v<base-version>/` for validation and offline reuse on Windows and macOS, independently of the installation directory. See [User data directory](<../Editor User Guide/Start Page Workspace and Shortcuts.md#user-data-directory>) for the native path. Bundled development templates remain read-only inputs.

New Project shows download or copy progress and extraction progress. **Cancel**, or closing the window during creation, cancels the operation and removes its unfinished project and download files. Completed, valid cache entries remain available for later use.

New Project always creates the editor's current major.minor project version, such as `v1.0` for editor `1.0.7`. It offers no older-version creation option. Opening supported older projects is a separate [project-version check](<../Editor User Guide/Project Settings and Asset Layout.md#mainproj>).

## FFmpeg option

**Enable video playback (FFmpeg)** adds local MOV and MP4 playback for H.264 video and AAC audio, and increases the build size. Other codecs, network playback and hardware acceleration are not included.

Platform preparation differs as well. Windows and macOS use shared FFmpeg libraries, while iOS, HarmonyOS and Android use static libraries. Distribution requirements, including the LGPL relinking materials for static builds, are described in `Licenses/FFmpeg/README.md`. On macOS, selecting FFmpeg also displays a compatibility notice.

## Template targets

- Configuring [Ludork Server](<../Simple Server/Deployment and Editor Settings.md>) requires C++ Source. Its Game test-server menu and Pack configuration are hidden for Standalone. All generated templates start with the server disabled and contain no test or production credentials. Converting an existing Source project can instead embed explicitly enabled Release server settings in its Standalone runtime.
- Script compilation, shader encryption, data encryption, save encryption and **Archive Assets, Data and Scripts (.ldpak)** are packaging options, not template types. See [Run, Debug and Package](<../Editor User Guide/Run Debug and Package.md>) for the complete option behaviour.

All four templates exclude Export outputs: `Scripts/Internal/UI/`, `Scripts/stub/Internal/UI/`, `Scripts/stub/Internal/UIWindows/`, generated locale language Lua files, `Assets/Shaders/MaterialGenerated/` and `EditorCache/ProjectExport.json`. The project `.gitignore` excludes the same generated files. Templates retain UI JSON, authored material graphs under `Data/Materials`, handwritten Controllers and declarations, `Scripts/Source/Locale/Core.lua`, General Data and variable Lua with their declarations, and native binding declarations. Export creates the UI, locale and material runtime files for the new project; Standalone uses its supplied preview compiler.

Templates also retain the optional root `MaterialExport.json` for [additional material combinations](<../Editor User Guide/Materials.md#material-combinations>); Export combines those entries with the new project's map usage.

The retained `Scripts/stub/**/*.d.lua` files support Lua language tools. They are removed from [game packages](<../Editor User Guide/Run Debug and Package.md#loose-and-packed-output>).

Runtime licence texts are in `Licenses`; FFmpeg variants also include `Licenses/FFmpeg`. Asset notices remain beside their assets. Packaging includes available licence texts, but their completeness remains the distributor’s responsibility.

## Related pages

- [Create Your First Project](<Create Your First Project.md>)
- [Ludork Lua Advanced](<Ludork Lua Advanced.md>)
- [Build and Module Layout](<../Native C++ Development/Build and Module Layout.md>)
