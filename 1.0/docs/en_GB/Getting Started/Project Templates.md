# Project Templates

## Goal

Select the template that matches the project's native-code, video and target-platform requirements.

## Template reference

The picker presents two template cards, **Standalone** and **C++ Source**, and an independent **Enable video playback (FFmpeg)** switch.

| Template | Native source | Native compiler required | Use when |
|---|---|---|---|
| Standalone | No | No | Authoring Lua, Blueprint, data and assets with the supplied runtime. |
| C++ Source | Yes | Yes | Changing native services, bindings or building an iOS, HarmonyOS or Android target. |

Each combination of card and switch has its own template. The template establishes the project baseline. Changing it later is a source-management task, not an editor toggle.

C++ Source templates include a project-root `.clang-format` for native source formatting. Use **Construct** for the first Debug build so the executable, preview binaries and metadata exist, then **Export** before the first Play. Standalone projects need Export before their first Play as well. The build and export checks, and how **Stop** works, are described in [Run, Debug and Package](<../Editor User Guide/Run Debug and Package.md>). Standalone templates include the preview runtime in `Binaries` and its metadata in `EditorCache/UiPreview.json` and `EditorCache/UiPreview.registry.json`.

## Obtaining templates

Development packages copy the selected local template from `Templates/Standalone`, `Templates/Cpp`, `Templates/Standalone-ffmpeg` or `Templates/Cpp-ffmpeg`. A missing development template is reported without downloading a replacement. Source builds also use development mode, independently of the Debug or Release compiler configuration.

Release editors obtain templates from the matching GitHub Release. Windows x64 uses ZIP archives; macOS ARM64 uses tar.gz archives. Each release provides eight separate template assets:

| Template | Windows x64 | macOS ARM64 |
|---|---|---|
| C++ Source | `game-source.zip` | `game-source.tar.gz` |
| C++ Source with FFmpeg | `game-source-ffmpeg.zip` | `game-source-ffmpeg.tar.gz` |
| Standalone | `game-standalone.zip` | `game-standalone.tar.gz` |
| Standalone with FFmpeg | `game-standalone-ffmpeg.zip` | `game-standalone-ffmpeg.tar.gz` |

The fixed download URL uses the editor's complete base version, including its patch number:

```text
https://github.com/JasonLeon01/Ludork/releases/download/v<base-version>/<asset-name>
```

For example, macOS editor `1.0.0` requests `v1.0.0/game-source-ffmpeg.tar.gz` for the Source/FFmpeg combination. If that release or asset has not been published, New Project reports that the current version's template is unavailable. Retry after it is published; the editor does not substitute another version or a CI artifact.

Downloaded archives are cached in `<Ludork installation directory>/Templates/v<base-version>/` on Windows and `~/Ludork/Templates/v<base-version>/` on macOS. A valid cached archive can be reused offline. Each project is extracted into its own directory and keeps its own runtime and resources; it does not run directly from the cache. Archives are checked for integrity, safe paths and matching project version, template type and FFmpeg setting before becoming reusable cache entries.

New Project shows download or copy progress and extraction progress. **Cancel**, or closing the window during creation, cancels the operation and removes its unfinished project and download files. Completed, valid cache entries remain available for later use.

New Project always creates the editor's current major.minor project version, such as `v1.0` for editor `1.0.7`. It offers no older-version creation option. Opening supported older projects is a separate [project-version check](<../Editor User Guide/Project Settings and Asset Layout.md#mainproj>).

## FFmpeg option

**Enable video playback (FFmpeg)** adds local MOV and MP4 playback to the selected template and increases the build size. The bundled build enables the FFmpeg file protocol, the MOV/MP4 demuxer, the H.264 and AAC decoders and parsers, libswscale and libswresample. It provides no network protocol, no hardware acceleration and no other codec.

Platform preparation differs as well. Windows and macOS use shared FFmpeg libraries, while iOS, HarmonyOS and Android use static libraries. Distribution requirements, including the LGPL relinking materials for static builds, are described in `Licenses/FFmpeg/README.md`. On macOS, selecting FFmpeg also displays a compatibility notice.

## Template targets

- [Ludork Server](<../Simple Server/Deployment and Editor Settings.md>) requires C++ Source. Its Game test-server menu and Pack configuration are hidden for Standalone. All generated templates start with the server disabled and contain no test or production credentials.
- Exporting mobile packages (iOS, Android and HarmonyOS) requires a C++ Source template.
- Script compilation, shader encryption, data encryption, save encryption and **Archive Assets, Data and Scripts (.ldpak)** are packaging options, not template types. See [Run, Debug and Package](<../Editor User Guide/Run Debug and Package.md>) for the complete option behaviour.

All four templates exclude Export outputs: `Scripts/Internal/UI/`, `Scripts/stub/Internal/UI/`, `Scripts/stub/Internal/UIWindows/`, generated locale language Lua files and `EditorCache/ProjectExport.json`. These exclusions apply even if the template source has already been exported. The project `.gitignore` carries the same generated-file rules. Templates retain UI JSON, handwritten Controllers and declarations, `Scripts/Source/Locale/Core.lua`, General Data and variable Lua with their declarations, and native binding declarations. Export recreates the UI and locale files in each project.

C++ Source templates also omit SFML's upstream `test` fixtures; the game build does not use them.

The retained `Scripts/stub/**/*.d.lua` files support editors and Lua language tooling. Packaging removes the entire `Scripts/stub` directory, and Lua compilation ignores `.d.lua`; no declaration file is shipped with a game.

Top-level `Licenses` is the sole licence-text source. Template generation copies the common runtime set into each C++ Source template before deriving Standalone. Only FFmpeg variants receive `Licenses/FFmpeg`. Editor and tool notices stay in the editor distribution, and asset notices stay beside their assets. Packaging preserves available texts but does not require them, so distributors remain responsible for legal completeness.

## Decision check

Use Standalone if every planned feature is expressible through the available Lua, Blueprint, asset and data APIs. Use C++ Source when even one required feature changes native code. Its targets also require the matching compiler, SDK and signing resources on the development host. Add FFmpeg only when video playback is an explicit requirement.

## Related pages

- [Create Your First Project](<Create Your First Project.md>)
- [Ludork Lua Advanced](<Ludork Lua Advanced.md>)
- [Build and Module Layout](<../Native C++ Development/Build and Module Layout.md>)
- [Run, Debug and Package](<../Editor User Guide/Run Debug and Package.md>)
