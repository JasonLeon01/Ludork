# Run, Debug and Package

Build, export, run, live-debug and package Ludork projects from the main editor toolbar or command-line tools. This guide covers desktop, mobile and WebGL builds, Windows compiler management, Lua hot reload and release signing.

## Construct, Export and Play

The main toolbar orders its workflow actions as **Construct → Export → Play**. Standalone projects hide **Construct**.

| Action | What it does | When to run |
|---|---|---|
| **Construct** | Saves the project and builds the Debug executable, native bindings and preview runtime without launching the game. | C++ Source projects before the first Play, and after native C++ or binding changes. |
| **Export** | Saves the project, runs Export plug-in hooks and generates UI Views, Lua declarations and locale files. | Before the first Play, and whenever UI assets, localisation workbooks, generated outputs or tool identity change. |
| **Play** / **Stop** | Launches the game after validation. Becomes **Stop** while preparing or running to cancel preparation or terminate the game. | After the first successful Export (and Debug build for C++ Source). |

A bare CMake build does not enable **Play**; build Debug through **Construct** or `tools/build_cpp`. A failed or cancelled Export does not count as a successful export, and any export failure blocks **Play** and **Pack** until the first validation, hook or generation error is resolved.

- **Outside Export:** Variables and General Data take effect on Save; their generated Lua files live under version control outside Export. Ordinary Save, Construct, direct run and template creation do not generate UI files.
- **Standalone CLI generator:** `ui-assets generate` outputs UI files only; it does not run Export hooks or write a complete editor export record.
- **Materials and previews:** Materials prepare during [game initialisation](<Materials.md#runtime-generation>), not during Export. Preview runtime setup is covered in [PreviewHost: Build, Publish and Troubleshooting](<../PreviewHost/Build Publish and Troubleshooting.md>).

### Automatic preparation on project open

When opening a C++ Source project, the editor checks generated native bindings, build and preview outputs, and exported UI and plug-in files before validating project data. Missing files or preparation records automatically trigger a Debug build followed by Export using the files already on disk, without saving partially loaded project data.

A progress dialog tracks the active step, and full logs are written to `~/Ludork/Log/<project-key>/Ludork.log` (keyed by the project's absolute path). Any build or export failure cancels opening and reports the log path. Source edits alone do not trigger automatic rebuilding on open. Deleting `EditorCache` triggers fresh preparation on the next open for C++ Source projects; Standalone projects require an explicit Export before Play.

If generated files exist but Blueprint fields or map Actor overrides fail schema validation, the editor offers **Rebuild and Export Project**. Confirm to rebuild, export, reload and validate once more; choose **Cancel** to exit the application. Errors persisting after that attempt appear in the standard invalid-data dialog without repeating the prompt. Invalid JSON alone does not trigger this recovery.

### Incremental Play, locks and window modes

After the first successful build and export, **Play** offers **Build and Play** or **Export and Play** when relevant files have changed; cancelling or failing either step aborts launch. Lua, UI JSON, game data and localisation edits require no native build.

- **Editing locks:** Construct, Export and Play preparation disable repeated actions, open document editing, Undo/Redo and editing shortcuts until completion. Natural exit, cancellation and failures restore editing; closing the project cancels active preparation.
- **Progress feedback:** An animated indicator appears on the toolbar during preparation, Construct, Export and Pack (hover for the active step) without switching away from the active tab; open **Console** manually to view logs. Saving and reference checks use the shared [editing progress dialog](<Start Page Workspace and Shortcuts.md#editing-and-saving>).
- **Individual Window:** Opens a separate game window while keeping the selected map or world visible in the editor. Editing locks apply while the game runs unless **Live Debug** was enabled before Play; Console, Performance Monitor and Help stay available.
- **Embedded mode (Windows only):** Displays the runtime inside the editor and locks the game viewport layout. Other platforms always use an individual window. An unavailable Windows embedded handle fails with `EmbeddedHandleUnavailable`.

## Windows project compiler

Windows C++ Source projects store their native compiler in [Main.proj](<Project Settings and Asset Layout.md#mainproj>). Click the arrow beside **Construct** and choose **Change project compiler**. The same selector appears in **New Project** and Windows **Pack Project** (including Source-to-Standalone conversion). Standalone projects neither require nor show compiler selection.

### Supported toolchains and detection

| Toolchain | Detection and requirements |
|---|---|
| **MSVC** | Requires Visual Studio 2022 C++ build tools (including Build Tools installations), not the IDE alone. |
| **MinGW** | Detected from Ludork user data (labelled with its source in parentheses) and saved user/system `PATH`. Requires no specific GCC version, C runtime, thread model or exception model. |
| **LLVM Clang** | Detected from saved user/system `PATH`. Uses `clang-cl.exe` targeting Windows x64 with the MSVC ABI (install separately; the editor provides no download action). See [Build the project](<../Native C++ Development/Build and Module Layout.md#build-the-project>) for required tools. |

Hover over a list entry to inspect its executable path; each compiler appears once. A valid project-configured MinGW or LLVM Clang stays selectable even if removed from `PATH`. Listing reads paths, versions and target architectures without initialising a build environment or compiling a test program. Project creation, compiler switches and builds validate the toolchain and its dependencies; MinGW and LLVM Clang must compile and run a C++20 program.

**New Project** and **Change project compiler** both offer **Get MinGW**, even when another compiler is installed. It downloads the fixed [WinLibs UCRT GCC 14.2.0, 64-bit, POSIX threads, SEH build](https://github.com/brechtsanders/winlibs_mingw/releases/download/14.2.0posix-19.1.1-12.0.0-ucrt-r2/winlibs-x86_64-posix-seh-gcc-14.2.0-mingw-w64ucrt-12.0.0-r2.7z) into `~/Ludork/Toolchains/WinLibs-14.2.0-ucrt-r2/mingw64/`, verifies SHA-256, extracts and validates it, and refreshes the list.

A valid installation shows **Installed**. Cancellation or failure removes only unfinished download and extraction files while preserving any existing installation for retry. Downloading neither changes an existing project's compiler nor modifies system or user `PATH`.

### Switching compilers and build outputs

Switching an existing project's compiler (including via **Pack Project** or between two installations of the same compiler) is unavailable while a build, Export or Pack is running and requires a second confirmation displaying both choices. Cancelling stops no processes, deletes no files and does not save. Initial new-project selection and re-selecting the active compiler skip cleanup confirmation.

1. After confirmation, the editor stops the project's game and all preview hosts, waits for process and file handles to release, and blocks restarts during cleanup.
2. It invalidates native build and preview records and deletes the Windows desktop CMake cache, intermediate native objects, dependency build outputs, launcher and Debug/Release binaries. Sources, assets, saves, existing distribution packages and other platform build directories stay intact.
3. Only successful cleanup writes the new compiler to `Main.proj`. A normal switch requires a fresh **Construct**; a switch confirmed via **Pack Project** continues building and packaging with the new compiler. Preview resumes once fresh build outputs are valid.

> **Warning:** If a locked file or error interrupts cleanup, the editor reports the path without terminating unrelated applications; close the holding application and retry. On cleanup or save failure, `Main.proj` keeps the previous compiler, but deleted build outputs are not restored and must be rebuilt. Closing the dialog after cleanup begins cannot bypass these checks or continue packaging.

All compilers share the same output paths: `build` (CMake), `bin/Debug` and `bin/Release` (executable and runtime libraries), and `build/launcher/<configuration>/Main.exe` (package launcher).

Compiler identity is part of the build state: when the configured compiler disagrees with an existing cache, the editor triggers the same confirmation and cleanup, whereas CLI builds fail rather than silently switching compilers or deleting files. See [Build the project](<../Native C++ Development/Build and Module Layout.md#build-the-project>).

## Server connections

For a C++ Source project, configure the local development server via **Game → Test Server Settings**, then rebuild Debug. **Pack Project → Enable Ludork Server** configures a separate Release connection and starts blank on every run (dev packages also use Release). Standalone projects hide both options. See [Simple Server](<../Simple Server/Deployment and Editor Settings.md>).

## Live Debug

Enable **Edit → Development Tools and Settings → Live Debug** before clicking **Play** to edit the running map directly from the editor. This preference defaults to off, cannot change during preparation or running, and requires **Individual Window** (switching to embedded mode clears and disables it). Preparation, Construct and Export keep standard editing locks.

While running, the editor tracks the player's current ordinary map or active composite-world child; manual map switching is disabled. World holes, loading maps, scenes without a live player and disconnected sessions are read-only.

**File Explorer** allows browsing and locating resources only (creating, renaming, moving, deleting and editing project files are disabled). See [Map and World Previews](<../PreviewHost/Map and World Previews.md#viewport-options>) for live viewport display rules.

| Area | Allowed in Live Debug | Locked / read-only |
|---|---|---|
| **Tiles** | Paint, erase, sample and fill on editable layers (same brush as [project map editing](<Tilesets Autotiles and Maps.md#layers-and-painting>)). | Layer structure, visibility, tileset assignment, map properties, world placements and hidden layers. |
| **Actors** | Select and delete live Actors (including the player, runtime-spawned Actors and child Actors); move top-level Actors (cancels active movement routes). | Adding, pasting or duplicating Actors; direct child-Actor movement (including **Actor Info** coordinates). |
| **Actor Info** | Inspect values and edit ordinary variables, including `Optional[T]` when `T` is supported (`nil` accepted; non-nil requires the declared type). | **Components**, parent class, Blueprint identity and map tags. |

- **Live synchronisation:** Actor positions, creation, destruction and inspected values track the running game. Deletion executes normal Actor destruction and lifecycle callbacks. Hidden Actors follow [map preview rules](<../PreviewHost/Map and World Previews.md#actor-and-layer-assistance>).
- **Child Actors:** **World Outliner** nests children under their current parents. Clicking or dragging a child on the map selects and moves its top-level owner and descendants. Select a child in **World Outliner** to inspect or edit ordinary variables, or delete it through its normal lifecycle.
- **Session scope:** Live edits have no Undo/Redo and never modify or mark project documents unsaved. Changes belong to the live map or Actor instance (including dormant world instances) and do not replay onto rebuilt instances, which use project definitions and standard gameplay persistence.

> **Note:** Game saves function normally during Live Debug and can persist gameplay state modified by live edits. Live Debug protects project source data, not save slots.

Stopping, natural exit or launch failure restores the project map view and pre-Play selection and discards temporary snapshots. The game in an **Individual Window** continues running while the editor holds focus.

## Reload Lua while running

To hot-reload Lua functions without restarting the game:

1. Save modified Lua source files in your external editor.
2. Enter `$r` in **Console** and press `Enter` or click **Send**.

Hot reload updates existing functions while preserving the scene, objects and runtime state. The game must be launched from the editor using loose Lua sources. The Console reports the outcome; structural data or module changes can require a restart. See [Lua hot reload](<../Lua and Blueprint Scripting/Lua Runtime and Modules.md#lua-hot-reload>).

## Packaging

Choose **File → Pack Project** and select a target from the list on the left.

| Host OS | Available targets |
|---|---|
| **Windows** | Windows, WebGL (browser; C++ Source), Standalone project (C++ Source) |
| **macOS** | macOS, iOS (C++ Source), HarmonyOS (C++ Source with DevEco Studio), Android (C++ Source with Android Studio), WebGL (browser; C++ Source), Standalone project (C++ Source) |

**WebGL (browser)** appears immediately above [Standalone project conversion](#convert-source-to-a-standalone-project). The shared release, server, encryption and archive options below apply to game-package targets.

The right pane displays platform-specific options first, followed by shared settings and — when signing is enabled — a signing form below a horizontal divider. Switching targets keeps inputs entered in the current session, while **Confirm** applies only the selected target's settings and opens the build log window (showing an animated indicator until completion or failure, then enabling **Close**).

### Prerequisites, identity and hooks

- **macOS Xcode check:** On macOS, C++ Source projects require a [full Xcode installation](<../Getting Started/Project Templates.md#template-reference>) for macOS, iOS and Standalone conversion. Pack Project checks once on open; if absent, those targets display **Get Xcode** instead of **Confirm** (opening Xcode on the Mac App Store without saving or packing). Close and reopen Pack Project after installation to recheck; switching targets or refocussing the window does not repeat the check. Existing Standalone projects, Android and HarmonyOS skip this check.
- **`APP_NAME` validation:** Game packages require a valid entry script and compatible template. `Scripts/Entry.lua` (initialised by [New Project](<../Getting Started/Project Templates.md#template-reference>)) must contain one top-level `local APP_NAME` as a non-empty static string literal with no reassignment, decoding per Lua rules into valid UTF-8 and a single directory name (not `.` or `..`, and without `/`, `\`, `:` or control characters).
  - Syntax errors (malformed strings, long comments or numeric literals) report file, line and column. Pack Project checks on open and focus; missing, invalid or computed values disable **Confirm** (hover for the reason).
- **Display name vs. identity:** Mobile and macOS display names use the raw `title.value` from `Data/Configs/System.json` (non-empty, no control characters; localisation keys are not resolved). `APP_NAME` supplies stable application/bundle identifiers and the sanitised `<game-name>` filename prefix. Changing title or version preserves installation identity; changing `APP_NAME` can require new signing or provisioning setup.
- **Export and hooks:** Each editor Pack runs one complete Export (including Export hooks and UI generation). Game packaging then runs before-Pack hooks (Standalone conversion skips them; Official Locale Tools uses its before-Pack hook solely to exclude `Data/Locale/Locale.xlsx`). Command-line run and pack consume existing UI and locale exports without running Export. Material programs generate during game initialisation and require no exported files or preflight.
- **Staging:** Game packaging operates on a staging copy without altering project sources. Preview registry rules are in [PreviewHost packaging rules](<../PreviewHost/Build Publish and Troubleshooting.md#editable-templates-and-final-packages>).
- **Mobile resource replacement:** Android, HarmonyOS and iOS replace prepared build resources only after conversion and validation succeed. Failure or cancellation beforehand keeps prior resources; failure during replacement that cannot restore them reports the retained recovery directory; cancellation or cleanup failure afterwards keeps the new resources (no rollback) and reports any leftover recovery or staging directory.

> **Tip:** If packaging reports access denied on an output path, close any running copy of the packaged game and File Explorer windows viewing the output folder, then retry. Language servers and antivirus scanners can also hold generated files open.

### Encryption, credentials and debug symbols

**Encrypt game data** controls script compilation plus shader, JSON and save encryption, displaying an indeterminate state when only a subset is checked.

- **Save encryption:** **Encrypt save contents** sets the C++ Source runtime default; Standalone projects configure `SAVE_AS_LDC` in `Scripts/Entry.lua`. See [Saves](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#saves>).
- **Data encryption:** Converts staged JSON to compressed, obfuscated binary `.ldc` while keeping source JSON editable. Rebuild packages containing the legacy compressed-JSON LDC format.
- **Runtime settings:** Shader encryption covers authored `Assets/Shaders`; data encryption covers packaged Data. Packaging writes `Data/RuntimeSettings.json` (`encryptShaders`, `encryptData`) into staging (processed with Data encryption and `.ldpak` without touching source files) to govern generated material shaders and metadata. When absent (as in development projects), both default to `false`; an invalid file fails preparation. Templates and Standalone conversions omit this file (see [Runtime generation](<Materials.md#runtime-generation>)).

> **Important:** Keep signing keys, certificates, keystores and provisioning profiles outside the project and repository. Paths must be absolute, readable and free of NUL, CR and LF characters; aliases and passwords cannot contain NUL or line breaks.
>
> Saved profiles store only paths and aliases in `~/Ludork/<platform>-signing.json` (see [User data directory](<Start Page Workspace and Shortcuts.md#user-data-directory>)); passwords live in the OS credential store, are redacted from logs, and are never written to project or generated build files.

**Debug symbols:** Windows packages omit PDB files, and Android and HarmonyOS packages strip symbols from native libraries. Retain the matching native build outputs for post-mortem debugging.

### Packaged and excluded files

Every target packages `Assets`, `Data` and `Scripts`. Windows, macOS, Android and HarmonyOS also copy `Licenses` and root `LICENSE.md`, `THIRD_PARTY_NOTICES.md` and `THIRD_PARTY_NOTICES_zh_CN.md` (iOS adds the same project files). Windows, macOS and FFmpeg-enabled WebGL packages also include `ThirdPartySource`.

The following entries are always excluded, and packaging fails if any of them (or any `.d.lua` file) remains after staging:

- `.DS_Store`, `.git`, `.vs`, `.idea`, `.vscode`, `.emmyrc.json`, `.gitignore` and editor animation caches (`*.anim.json`) at any depth
- Project-root `EditorCache`, `Cache`, `Intermediate`, `Log` and `Scripts/stub`
- [Map List order](<Tilesets Autotiles and Maps.md#map-list-and-composite-worlds>) files `Data/Maps/_info.json` and `Data/Maps/<world>/_info.json`
- `Assets/Shaders/MaterialGenerated`, build tools and preview components
- Files excluded by before-Pack hooks

Packaging also strips UI `editor` design data. UI asset keys stay extensionless and Controller references keep their logical Lua module paths across JSON, `.ldc` and `.ldpak`. Directories named `EditorCache` or `Cache` inside `Assets`, `Data` or `Scripts`, and `_info.json` files elsewhere in `Data`, are packaged as ordinary project content.

### WebGL

WebGL packaging requires a C++ Source project, Emscripten SDK **7.0.0**, CMake and Ninja. Point to the SDK via `EMSDK`, its compiler on `PATH`, or CLI `--emsdk <directory>`. The packer reports missing prerequisites without installing tools or replacing dependencies. Existing projects require Web-capable SFML and LuaSF sources (see [native build inputs](<../Native C++ Development/Build and Module Layout.md#build-the-project>)). Standalone projects cannot produce Web runtimes.

- **Output:** `dist/<game-name>-<full-version>-webgl` (a custom output directory replaces `dist`), containing `index.html`, `Main.js`, `Main.wasm`, `Main.data` and available licence materials. Build and validation finish before publication, preserving the previous package on failure or cancellation.
- **Included features:** Supports release/dev metadata, before-Pack hooks, Lua compilation, shader/data/save encryption, `.ldpak` and optional Ludork Server integration, with no signing settings. The shared resource finaliser prepares `Assets`, `Data` and `Scripts`, and Emscripten embeds that loose-or-packed layout inside `Main.data` rather than publishing separate download directories. FFmpeg-enabled projects support [local video playback](<../Lua and Blueprint Scripting/Default Gameplay/Resources Audio Video and Effects.md#video>).
- **Hosting and runtime constraints:**
  - Serve the full directory over HTTP or HTTPS (never open `index.html` via `file://`), serve WebAssembly as `application/wasm`, and keep generated filenames together.
  - Uses WebGL 2 on a single canvas with keyboard, mouse, touch and [supported browser gamepads and text entry](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Input and Services.md#web-input>).
  - Starts automatically after load (`SceneInit` → title screen). If autoplay audio is blocked, the game runs silently until the first normal click or tap unlocks audio.
  - Browsers govern fullscreen, background throttling and storage; losing the graphics context requires reloading the page.
  - Geometry shaders, custom image cursors (uses system cursors), native embedded WebView windows, editor Console connections and Live Debug are unavailable; runtime output goes to the browser console.
- **Command line:** Run `tools/pack_web.bat <project> [dist]` (Windows), `sh tools/pack_web.sh <project> [dist]` (macOS), or `ludork-pack web-pack`. Pass `--check` to validate prerequisites without building; CLI callers must run editor Export beforehand. See [browser user data](<Project Settings and Asset Layout.md#game-configuration>) and [server deployment](<../Simple Server/Deployment and Editor Settings.md#configure-a-packaged-client>).

### Convert Source to a Standalone project

With a C++ Source project open on Windows or macOS, select **Standalone project** in **Pack Project** to build a Release runtime and matching preview components for the host platform and output an editable project at `dist/<source-directory-name>-standalone`.

- **Export and content:** Saves and runs a complete Export (including Export hooks), skipping before-Pack hooks. Preserves editable `Assets`, `Data`, `Scripts`, UI/locale outputs, material graphs, `Data/MaterialCombinations.json`, UI design data, Lua declarations, localisation workbook, licences, third-party source, `.emmyrc.json` and VS Code Lua/EmmyLua settings.
  - Excludes native sources, build directories, Git, saves, logs and local caches; `EditorCache` holds published preview metadata only.
- **`Main.proj` updates:** Copies `Main.proj`, sets `Cpp=false` and `packaging.dev=false`, removes `compiler`, and preserves project version, FFmpeg flag, release version and other keys without modifying the source project's release/dev preferences (a confirmed Windows compiler switch updates the source project per [Windows project compiler](#windows-project-compiler)).
  - Release-version, dev, encryption, Lua compilation and `.ldpak` controls are hidden. Existing exports allow direct launch, but `EditorCache/ProjectExport.json` is neither copied nor fabricated, so the first editor Play can require Export.
- **Ludork Server:** **Enable Ludork Server** defaults to off. When enabled, the Release URL and key compile into the runtime and cannot be changed inside the resulting Standalone project ([server configuration](<../Simple Server/Deployment and Editor Settings.md#configure-a-packaged-client>)).
- **macOS signing:** Uses the same signing identity, certificate, password, environment and local credential storage as game packaging (ad-hoc when unconfigured). Signs runtime libraries, preview components and root `Main`, applies entitlements only to `Main`, and verifies the signed preview. The output stays an editable directory (not wrapped in an `.app` or directory-signed); notarisation is unavailable and saved notarisation settings are ignored.
- **Command line:** Run `tools/pack_standalone.sh` (macOS) or `tools/pack_standalone.bat` (Windows) with `<source-project> [standalone-directory]` (full output path) and optional `--check`; see [tool options](https://github.com/JasonLeon01/Ludork/blob/main/tools/README.md).
  - CLI checks existing UI exports (requiring editor Export if missing or outdated; no headless Export). Staging and validation finish before replacing the destination, preserving prior output on failure or cancellation; outputs cannot replace the source or its inputs, and linked paths are rejected.

### Release version and internal packages

Set **Release version** (`packaging.version`, default `"1.0.0"`) and — for C++ Source projects — **Internal test package (dev)** (`packaging.dev`, default `false`) in Pack Options. Confirming valid options saves both settings to `Main.proj`; cancelling writes nothing. Standalone projects always save `packaging.dev=false`, even when an older project had `true`.

The base version must be three non-negative decimal components (such as `1.0.0` or `0.1.0`), up to 127 bytes, with no `v` prefix, leading zeros, date suffix or prerelease suffix. Version metadata is not defined in `Scripts/Entry.lua`.

All game packaging commands accept `--version 1.0.0` and `--release`; C++ Source projects also accept `--dev`. Command-line flags override `packaging.dev` for that invocation without writing back to `Main.proj`. Standalone projects always package as releases, ignore legacy `packaging.dev=true` and reject `--dev`.

The packer captures one UTC+8 build timestamp at startup and reuses it across all stages. Formal packages use the base version as their full identifier, whereas dev packages append `YYYYMMDDHH` (for example, `1.0.0` at `2026-01-01T07:00:00+08:00` becomes `1.0.0.2026010107`). Dev mode does not alter Release optimisation, signing, encryption or installation identity.

| Platform field | Value in both modes | Example (`2026-01-01T07:00:00+08:00`) |
|---|---|---|
| Android/HarmonyOS `versionName`; Apple `CFBundleShortVersionString` | Base version | `1.0.0` |
| Android/HarmonyOS `versionCode` | `YYMMDDHH` integer | `26010107` |
| iOS/macOS `CFBundleVersion` | `YYMM.DD.HH` | `2601.1.7` |

- **Date and rebuild limits:** Build dates must fall within `2000–2099`, and platform limits are checked before replacing output. Rebuilding within the same hour produces the same build number; wait until the next hour when a store requires a higher build number. Output names use `<game-name>-<full-version>` before platform, architecture, variant and signing suffixes; repacking the same identifier replaces only that artefact.
- **`Data/BuildInfo.json`:** Written into staging with `version`, `fullVersion`, `dev` and `builtAt` (ISO 8601 with `+08:00`). Both dev and release game packages process it through Data encryption and `.ldpak`; it is never written back to source `Data`. Application release versions are independent of the top-level [project compatibility version](<Project Settings and Asset Layout.md#mainproj>) and [save schema versions](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#saves>).

### Loose and packed output

Resources default to loose `Assets`, `Data` and runtime `Scripts` directories on every platform (WebGL embeds this layout in `Main.data`). The `.ldpak` archive option is clear by default; enabling it packs each complete directory into root `Assets.ldpak`, `Data.ldpak` and `Scripts.ldpak` while preserving relative paths and Lua module names.

Encryption and Lua compilation run before archiving; `.ldpak` itself does not encrypt contents. Mobile packages enforce the same contract, and Android and HarmonyOS do not recompress `.ldpak`. Packaging rejects symbolic links, case collisions, illegal paths, output conflicts, corrupt archives, missing roots, mixed loose/packed layouts and legacy per-group archives.

### Windows and macOS

Windows outputs `dist/<game-name>-<full-version>/Main.exe` alongside its runtime files; macOS outputs `dist/<game-name>-<full-version>.app` using the System title as its display name. A custom output directory replaces `dist`. Neither the output directory nor its ancestors may be symbolic links or directory junctions.

Game packages built from Standalone projects keep a root launcher and place runtime binaries in `Binaries` (Windows) or `Contents/Frameworks` (macOS). FFmpeg notices and source-offer files are included where applicable (see [FFmpeg option](<../Getting Started/Project Templates.md#ffmpeg-option>)).

#### macOS signing and notarisation

**Sign Application** is clear by default on macOS:

- **Unchecked:** Signs with environment variables (`LUDORK_MACOS_SIGNING_IDENTITY`, `LUDORK_MACOS_SIGNING_CERTIFICATE`, `LUDORK_MACOS_NOTARY_*`), or applies an ad-hoc signature when unset (no distributor certificate or notarisation).
- **Checked:** Accepts a `.p12` certificate and password, an optional signing identity, and optional notarisation using either an Apple ID (with team ID and app-specific password) or an App Store Connect key (with key ID and issuer ID). Form values override environment variables for that run, and selecting a previously saved certificate restores its settings from `~/Ludork/macos-signing.json`. Notarisation signs with a real identity, submits to Apple, waits for approval and staples the ticket; ad-hoc identities are rejected.

Certificate signing and notarisation (in the editor, CI and CLI; never for ad-hoc signing) retry transient network or service errors — including temporarily unavailable tickets after acceptance — after `5`, `15` and `30` seconds (up to four attempts per operation, retrying only the failed target with identical options). Invalid credentials, certificates, signatures, rejected submissions and revoked or invalid tickets fail immediately.

| Operation | Timeout | Retry notes |
|---|---|---|
| Signing, ticket attachment/validation, notary queries | 2 minutes per attempt | Up to 4 attempts (`5 s`, `15 s`, `30 s` backoff). |
| Credential validation | 5 minutes per attempt | Up to 4 attempts. |
| Submission upload | 15 minutes per attempt | Once a submission ID exists, retries wait on that ID instead of re-uploading. |
| Apple notarisation processing wait | 15 minutes per wait (max 4 waits) | Failed waits or ticket steps report the submission ID for manual querying. |

### iOS

Targeting iOS `16.3` or newer on macOS requires a C++ Source project, a full Xcode installation with the iPhoneOS SDK, and completed Xcode first-launch setup.

- **Sign Application (clear by default):** Leave clear to let Xcode sign automatically with the signed-in Apple Development team (`LUDORK_IOS_DEVELOPMENT_TEAM` when set). Select it to sign manually with a team ID, a `.p12` certificate and password, and a `.mobileprovision` provisioning profile (certificate and profile are required together; mismatched team or app IDs are rejected). Form values override environment variables for that run, and selecting a saved certificate restores its profile from `~/Ludork/ios-signing.json`.
- **Output and device export:** By default, packaging builds `dist/<game-name>-<full-version>.ipa` with no iPhone connected. Select **Export to iPhone** to install and launch on exactly one physical iPhoneOS device (iPhone or iPad) reported available by `xcdevice` (simulators do not qualify). Unlock, trust and Developer Mode prompts appear on the device; untrusted developer profiles currently surface as a signing error.

### HarmonyOS

HarmonyOS packaging requires a C++ Source project, Apple Silicon macOS and DevEco Studio with the OpenHarmony native SDK. It builds a single `arm64-v8a` HAP targeting HarmonyOS `6.0.2` / API `22` for phones, tablets and 2-in-1 devices using OpenGL ES 3.

- **Sign HAP (clear by default):** Leave clear to output `dist/<game-name>-<full-version>-harmony-unsigned.hap`. Select it to provide a `.p12` keystore, `.cer` certificate, `.p7b` Profile, key alias and passwords (key password defaults to keystore password) to produce `-signed.hap` without a connected device.
  - Saved settings live in `~/Ludork/harmony-signing.json` (passwords in macOS Keychain) and restore on selecting the same keystore; failed runs keep previous output. See [tools/README.md](https://github.com/JasonLeon01/Ludork/blob/main/tools/README.md) for CLI options.
- **Profiles and clipboard permission:** Both Debug and Release Profiles are supported and must be valid, signed and match the bundle name. The HAP declares `ohos.permission.READ_PASTEBOARD` only when the verified Profile grants it via `acls.allowed-acls` or has the `system_basic` or `system_core` access level; unsigned builds and builds lacking that authorisation omit the permission.
- **Device export:** Select **Export to connected HarmonyOS device** to sign, install and launch on one connected device with Developer Mode enabled. HDC accepts the reported device types `default`, `phone`, `tablet` and `2in1`, verifies Profile authorisation for the device, and aborts if the device type cannot be read. Installation failures include the device diagnostic.

### Android

Android packaging requires a C++ Source project on macOS with Android Studio installed in `/Applications` or `~/Applications`. Install SDK Platform `36`, Build Tools `36.0.0`, a complete stable NDK `r27` or newer (the highest qualifying local NDK is chosen) and host CMake `3.28` or newer separately.

The APK targets API `36`, runs on Android `7.0` / API `24` or newer in either landscape orientation, and is built without installing or launching.

- **Sign APK (clear by default):** Leave clear to publish `dist/<game-name>-<full-version>-android-arm64-v8a-unsigned.apk`. Select it to choose a JKS or PKCS12 keystore (`.keystore`, `.p12`, `.pfx`, `.pkcs12`, auto-detected), enter its alias and passwords (key password defaults to the keystore password), and optionally save the path and alias in `~/Ludork/android-signing.json` (auto-restored on keystore selection). A signed run publishes only `dist/<game-name>-<full-version>-android-arm64-v8a-signed.apk`.

> **Warning:** Android requires updates to an installed application to be signed with the same certificate. Devices reject updates signed with a different key.
