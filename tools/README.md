# Game tools

All scripts switch to the repository root before doing work. Use `.bat` on Windows 10 or newer on x64 and matching `.sh` scripts on Apple Silicon macOS 13.3 or newer. macOS tools find CMake from `PATH` or `/Applications/CMake.app`.

| Tool | Purpose |
|---|---|
| `init` | Prepare the generator environment and native dependencies |
| `create_vsc_workspace` | Create a local VS Code workspace for the project and the branch worktrees listed by the generator |
| `setup_python` | Create `.venv` and install build-time Python requirements |
| `build_script_tools` | Build the ScriptTools runtime bundle under `.tools/ScriptTools`; compiler outputs stay under `.tools/build/ScriptTools` |
| `build_preview_host` | Build and publish a project's native preview snapshot |
| `init_cpp_dependencies` | Download dependencies for a C++ project folder; per-package scripts live under `tools/cpp_dependencies` |
| `run_editor` | Start the editor from the repository root |
| `animation_to_mp4` | Export source animation JSON files to H.264/AAC MP4, individually or recursively in a batch |
| `build_cpp` | Configure/build a C++ project, its preview and registry; regenerate Core bindings, stubs and metadata |
| `run_cpp` | Run a built native project with its source folder as working directory |
| `build_standalone` | Build a desktop runtime with a root launcher/host and native code under `Binaries` |
| `run_standalone` | Run a desktop Standalone project through its root launcher/host |
| `create_templates` | Recreate Cpp and Standalone template variants |
| `create_templates_plain` | Recreate only the non-FFmpeg Cpp and Standalone templates |
| `create_templates_ffmpeg` | Recreate only the FFmpeg-enabled Cpp and Standalone templates |
| `pack_project` | Produce the platform distribution layout, with optional macOS signing and notarisation |
| `pack_standalone` | Convert a C++ Source project into an editable desktop Standalone project, retaining exported resources and preview support |
| `pack_ios.sh` | Build an iOS 16.3-or-newer IPA from a C++ Source project, with optional manual signing |
| `pack_harmony.sh` | Build a HarmonyOS API 22 Mobile or 2in1 arm64-v8a HAP, with optional signing and device export |
| `pack_android.sh` | Build an Android arm64-v8a Release APK from a C++ Source project, unsigned by default with optional signing |
| `pack_server.py` | Build or validate the independent deployable Node.js Server archive |
| `pack_editor.bat` | Publish and validate the self-contained Windows 10-or-newer x64 editor package with official plug-ins |
| `pack_editor.sh` | Publish and validate the self-contained macOS Apple Silicon editor DMG, with ad-hoc signing by default and optional Developer ID signing and notarisation |
| `pack_editor_msi.bat` | Wrap a release Windows editor package into the installer MSI |
| `pack_editor_exe.bat` | Wrap the same release Windows editor package into the Inno Setup EXE |

Typical commands:

Use the editor's **Export** before running or packaging a project with the
command-line tools. These tools consume the exported UI and locale Lua files and material programs.

```sh
./tools/init.sh
./tools/run_editor.sh
./tools/build_cpp.sh Game Debug
./tools/run_cpp.sh Game Debug
./tools/pack_project.sh Game Game/dist
./tools/pack_harmony.sh --device-form mobile --graphics-api opengl-es Game
./tools/pack_android.sh Game
./tools/pack_editor.sh
```

```bat
tools\init.bat
tools\run_editor.bat
tools\build_cpp.bat Game Debug
tools\run_cpp.bat Game Debug
tools\pack_project.bat Game
tools\pack_editor.bat
```

For project workflows, see [Run, Debug and Package](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=02%2F11), [Project Templates](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=01%2F02) and [Native Build and Module Layout](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=04%2F01). The sections below cover command-line and repository maintenance details.

On macOS, the default editor build creates and ad-hoc signs `bin/<configuration>/net9.0/Ludork.app`. Both `dotnet run` and `tools/run_editor.sh` launch its executable, so system notifications also work during development. Allow notifications for **Ludork Development** when macOS first requests permission; this development identity has separate notification settings from the installed **Ludork** application. IDE launch configurations must run `Ludork.app/Contents/MacOS/Ludork`; launching `Ludork.dll` through `dotnet` does not provide the required macOS application identity. Builds with an explicit runtime identifier or custom output directory retain their normal output layout and do not create this development bundle.

## Website and documentation

Website sources live on `gh-pages`; use a separate worktree and follow [AGENTS.md](../AGENTS.md#documentation-workflow). `EDITOR_VERSION` in `versions.conf` selects `<major.minor>/docs` and `<major.minor>/images`. The branch's README covers the frontend, dependencies, development server and build hooks.

Run `tools\create_vsc_workspace.bat` on Windows, or `sh tools/create_vsc_workspace.sh` on macOS/Linux, to generate `<project-directory-name>.code-workspace` in the project root. Both entries run `tools/create_vsc_workspace.py`, locate the project from that script, and require Git and Python 3 on `PATH`. The Windows entry uses `python`, then the `py -3` launcher. The shell entry uses `python3`. Set `PYTHON` to choose another executable. No build environment initialization is required.

`WORKSPACE_BRANCHES` in the Python script lists the branches checked out beside the current project. It currently contains `gh-pages` and `Server`. Append a branch name there to include another checkout. Repeated names are checked out once. New worktrees use the sibling directory `Ludork-<branch>`, such as `Ludork-gh-pages` or `Ludork-Server`, regardless of the project directory's name. Slashes and Windows-invalid filename characters in the branch name become hyphens in the directory name; the Git branch name is unchanged.

The script reuses an existing registered worktree for each listed branch without renaming it, even outside the project parent directory. Otherwise, it uses the local branch first, then tracks `origin/<branch>`; if that remote reference is missing, it fetches just that branch from `origin`. For a single-branch clone, it adds the missing branch fetch mapping to `origin` so upstream tracking works, retaining existing mappings. An occupied destination, invalid branch name, duplicate worktree directory or invalid registered worktree stops the operation. It does not switch or update the current worktree's branch. When the current checkout is one of the listed branches, that folder supplies the branch and the workspace does not repeat it. The operation stops when every listed branch is already the current checkout.

Each successful run replaces the workspace file with the current project and one folder for each listed branch checked out elsewhere. Paths are relative where possible and absolute across drives. Existing workspace settings are overwritten; `*.code-workspace` is ignored by Git for developer-local use. Open the generated file in VS Code.

Run `build_docs.bat` or `sh build_docs.sh` in that worktree to regenerate the site. Its pre-commit hook builds and includes generated output, rejecting failed builds and unstaged inputs. Edit Markdown and navigation sources, not generated pages or bundles. Configure GitHub Pages as `gh-pages / (root)` and retain `.nojekyll`; no deployment workflow is needed.

Public routes are `/Ludork/`, `/Ludork/docs/v1.0/`, `/Ludork/about/` and `/Ludork/notices/`. Pages accept `lang=en_GB` or `lang=zh_CN`; documentation accepts `doc` or `path` with optional heading fragments. About and notices accept `version=1.0`; the editor adds `embedded=1`. Editor packages contain local licence texts and runtime notices, but no website sources or generated site assets.

`ScriptTools legal-resources editor <repository-root> <output-root>` writes the editor licence, READMEs and licence indexes after resources are copied. `legal-resources template-index` takes the same arguments and writes only the two indexes, linked to the template's root runtime notices. Neither command modifies source documents.

## Animation MP4 export

Rebuild ScriptTools after source changes with `tools/setup_python` and `tools/build_script_tools` using the platform extension. The exporter uses Pillow from the Python requirements, included in the built bundle. Install FFmpeg with `libx264` and AAC on `PATH`, place it at `.tools/ffmpeg/ffmpeg.exe` (Windows) or `.tools/ffmpeg/ffmpeg` (macOS), or pass `--ffmpeg <executable>`. Editor packaging does not bundle this executable.

```bat
tools\animation_to_mp4.bat
tools\animation_to_mp4.bat Game --input Game/Data/Animations/attack.json --output Game/EditorCache/attack.mp4
tools\animation_to_mp4.bat Game --size 512x512 --background "#202020" --overwrite
```

On macOS, use `sh tools/animation_to_mp4.sh` with the same arguments. The direct command is `ScriptTools animation-mp4 [project] [options]`; `project` defaults to `Game`.

| Option | Behaviour |
|---|---|
| `--input <path>` | One source JSON or a recursively searched directory; default `<project>/Data/Animations`. |
| `--output <path>` | Default `<project>/EditorCache/AnimationMp4`; preserves relative subdirectories in a batch. A single input also accepts an `.mp4` filename. |
| `--size WIDTHxHEIGHT` | Even-sized canvas, without rescaling; crops content outside it. The automatic canvas contains all sampled frames with the animation origin centred. |
| `--background "#RRGGBB"` | Opaque background; black by default. |
| `--mute` | Removes audio without changing the timeline duration. |
| `--overwrite` | Allows replacing existing outputs. |
| `--ffmpeg <executable>` | Selects the local encoder. |

Wrapper input/output paths are relative to the repository root. Inputs must be saved `type: animation` JSON and loose `/Game/Assets/...` files, not compressed caches or `.ldpak` packages. Export preserves frame rate, timeline order, linear transforms, horizontal flip and sound timing/trimming. It renders one playback through the last image or sound segment, rounded up to whole frames; sound-only tails show the background. Time tags are not video events.

Each output is published only after FFmpeg succeeds. Batch export continues after individual failures and exits nonzero if any file fails.

## Native build state

`build_cpp` records successful builds in `EditorCache/NativeBuild-<configuration>.json`. A pending marker preserves the previous record while a build runs; failure, cancellation or changed inputs leaves the build out of date. Only successful completion with unchanged inputs clears it.

| `ScriptTools native-build-state` command | Result |
|---|---|
| `check <project> <Debug\|Release>` | JSON `{"current":true,"detail":""}` or `current:false` with a reason. Both exit 0; invalid input or unreadable state exits 2. |
| `begin <project> <Debug\|Release>` | Starts the build record operation. |
| `complete <project> <Debug\|Release>` | Completes it; exits 1 if inputs changed during compilation. |

Checks compare first-party native sources, CMake/resources, the `Main.proj` FFmpeg switch, Windows icon and executable/library identity. Lua, UI, locale, generated files and third-party trees are excluded. Records are specific to configuration, platform and architecture; editor Play requires Debug. Run `build_cpp` after third-party or external CMake changes, or after a direct `cmake --build`, to publish the result for Play. Restoring unchanged source contents can reuse a successful record when no build is pending.

`ScriptTools project-state-worker <project>` provides read-only JSON Lines requests over stdin/stdout. Each request has `version:1`, integer `id`, and `operation`: `ready`, `native-check` (with `configuration:Debug` or `Release`), `ui-manifest`, or `material-manifest`. Responses echo `id` with `result` or `error`; diagnostics use stderr. It reads current files on every request and writes no build/export records. The editor serialises requests and restarts the worker after bundle changes or process exit, stopping it on cancellation or project close.

### IDE entry points

From a C++ Source project root, run `generate_vs2022.bat` to create `Main.sln`, or `generate_clion.bat` / `./generate_clion.sh` for CLion's **Ludork Play** target. These generators configure CMake. Set `LUDORK_TOOLS_DIR` to the installed editor's `tools` directory if autodetection fails. IDE launches use the Debug `Main` target with the project root as working directory, without editor save handling, Before Run hooks, embedded viewport or editor bridges. See [Workspace and Shortcuts](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=02%2F01) for editor integration.

## Templates and editor packages

| Command or option | Contract |
|---|---|
| `create_templates --variant plain\|ffmpeg` | Build one Source/Standalone pair; omission builds all four. `create_templates_plain` and `create_templates_ffmpeg` are shortcuts. |
| `create_templates --version X.Y.Z` | Override the editor base version for this invocation; generated `Main.proj` uses `v<major.minor>`. |
| `create_templates --native-cache <folder>` | Reuse native outputs by variant and Debug/Release configuration. Keep outside Game and template output; invalidate after native, binding, dependency or build-option changes. |
| `build_standalone --use-current-build` | Package matching `bin/<configuration>` and Windows `build/launcher/<configuration>` outputs. |
| `pack_editor --without-templates` | Package the editor, plug-ins and tools without templates; mutually exclusive with `--templates`. |
| `pack_editor --templates <folder>` | Copy an existing four-template set in development mode. Keep outside the recreated `obj/editor-package` staging directory. |
| `pack_editor --use-current-editor-build` | Restore NuGet and publish matching Release editor/contract outputs with `--no-build`; refresh Content and locale in clean staging. |
| `pack_editor.bat --launcher <file>` | Use a prepared launcher outside staging. |
| `pack_editor --version <base-version> --dev\|--release` | Override package metadata for this invocation; default channel is dev. |

Development packaging regenerates templates unless `--templates` or `--without-templates` is given. Release packaging always omits templates and rejects `--templates`. Both modes require Python 3.12, ScriptTools and the host Lua compiler, plus GNU Make on Windows; template-free builds do not require Game dependencies or the FFmpeg source archive. Standalone templates must contain matched previews; Source templates contain preview source. Source templates omit unused SFML test fixtures whose filenames conflict with macOS filesystem normalisation.

Templates exclude `Assets/Shaders/MaterialGenerated`; the first editor Export creates the generated material programs. Native caches supply runtime binaries, the Windows launcher and generated bindings, enums and preview data. Current Game content is still copied and validated. See [Project Templates](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=01%2F02) for retained files, export exclusions and release downloads.

`EDITOR_VERSION` is the editor base version; `GAME_VERSIONS` lists supported project versions and must include the current `v<major.minor>`. New projects use that current version. These are separate from a game's `packaging.version`; see [project compatibility](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=02%2F02#mainproj).

```bat
tools\pack_editor.bat --release
tools\pack_editor_msi.bat
tools\pack_editor_exe.bat
```

For a development package, use `tools\pack_editor.bat --version 1.0.1 --dev` without the installer steps.

```sh
./tools/pack_editor.sh --release
```

Packers freeze `obj/editor-package-metadata/BuildInfo.json` for assembly metadata, filenames, MSI, EXE and macOS bundles. Dev full versions append the UTC+8 `YYYYMMDDHH` build hour; binary version fields retain the base version. Reused editor outputs must match the snapshot's base/full version and channel. Dev packages copy the snapshot to the Windows root or macOS `Contents/Resources`; formal editor, template and Server assets omit it. The external snapshot is not uploaded. About and channel detection use embedded metadata; direct source runs default to dev.

Windows packages place a native `Ludork.exe` launcher at the root and the self-contained editor under `Binaries`. Root resources include `Locale`, `Licenses`, `tools/ScriptTools`, official plug-in sources and optional development `Templates`. Development packages include `Plugins` and `plugins.json` at the root for direct loading; release packages keep their installation payload under `tools/OfficialPlugins/{Plugins,plugins.json}`. Packaging uses repository plug-in sources and never copies user data from `~/Ludork`. The launcher forwards arguments, streams and exit status, using the installation root as the working directory. Packaging invokes `--compile-locale` through it, validates staging before replacing `dist`, and restores the previous package on replacement failure. MSI and EXE shortcuts and `.proj` associations target this launcher.

`pack_editor_msi.bat [output.msi]` and `pack_editor_exe.bat [output.exe]` require the same release `dist` and pinned WiX or Inno Setup toolset. Both accept `--build-info <file>`, defaulting to `obj\editor-package-metadata\BuildInfo.json`; the snapshot must match the embedded editor metadata. Default outputs are `obj/installer/output/Ludork-<version>-windows-x64.msi` and `.exe`. Definitions, the English agreement and helper sources stay under `tools/installer`; tool caches stay in `.tools` and intermediate files in `obj/installer/msi` or `obj/installer/exe`. Installer inputs and user data are rejected from editor payloads.

Both formats request administrator permission and install for all users under the system `Program Files\Ludork` by default, with a selectable directory. They register `.proj` as `Ludork.Project`, using the quoted root launcher and project filename. Windows retains protected per-user default-app choices; choose Ludork in Windows Default Apps when another app was explicitly selected. EXE includes its own uninstaller and an entry in Installed apps. Uninstall removes Ludork's own association only while its open command belongs to that installation and preserves associations another app took over. **Delete user data** is selected by default and removes only the verified initiating user's `~/Ludork`; clear it to retain data. Projects outside that directory and other users' data are preserved. Silent MSI uses `REMOVE_USER_DATA=0/1`; the EXE uninstaller uses `/REMOVEUSERDATA=0/1`, both defaulting to `1`. MSI cleanup runs only for explicit full removal, excluding repair and major-upgrade removal; service or unverifiable identities must use `REMOVE_USER_DATA=0`. EXE preserves data and reports manual cleanup when it cannot verify the initiating user, including cross-account UAC. See [User data directory](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=02/01#user-data-directory) for scope and failure behaviour. Same-format upgrades are supported; downgrades and another-format or legacy per-user installations require uninstalling the existing installation first.

The EXE **Install Official Plugins** option is selected by default. MSI also installs them by default; use `INSTALL_OFFICIAL_PLUGINS=0` to skip. Installing official plug-ins replaces the entire user plug-in directory and registry, including third-party and project plug-ins and their data. Close Ludork and preserve anything needed first. Skipping installation or repairing the editor does not reset plug-ins; uninstall follows the user-data choice above. Installation uses the invoking user's identity rather than another administrator's profile supplied during UAC; service-account installations must skip user plug-ins. Explicitly starting setup as another account instead targets that account. The `install-current-user` command saves operation and recovery details to `~/Ludork/Log/OfficialPluginInstall.log` when that directory is writable; failure to save this diagnostic log does not undo a completed installation.

`pack_editor.sh` requires Apple Silicon macOS, a logged-in Finder session, .NET 9 SDK, CMake and prepared tools. It writes `dist/Ludork-<version>-macos-arm64.dmg` for macOS 13.3+. Resources and tools live under `Contents/Resources`; executable/.NET dependencies under `Contents/MacOS`. The DMG includes `Ludork.app`, an Applications link and **Install Official Plugins**. The installer validates and replaces `~/Ludork/Plugins` and `~/Ludork/plugins.json`, including third-party plug-ins and `Plugins/.data`; failure attempts rollback and reports recovery details if rollback fails. To unregister file associations before removing the app:

```sh
/Applications/Ludork.app/Contents/Resources/tools/unregister_editor.sh
```

`editor-official-plugins prepare <source> <output-root>` copies the fixed official plug-ins and generates their registry. `editor-official-plugins validate <output-root>` checks exact directories, manifests, index and excluded user/build files. `editor-official-plugins install-current-user <payload-root>` validates the prepared payload, requires Ludork to be closed, and replaces the current user's plug-ins with rollback on failure. A backup cleanup failure is reported separately. Ordinary editor startup never installs or replaces official plug-ins.

For a repository checkout, explicitly prepare and install the official plug-ins after building ScriptTools:

```sh
.tools/ScriptTools/ScriptTools editor-official-plugins prepare Plugins obj/official-plugins
.tools/ScriptTools/ScriptTools editor-official-plugins install-current-user obj/official-plugins
```

On Windows, use `.tools\ScriptTools\ScriptTools.exe` instead. Windows development archives load their bundled plug-ins directly when launched through the root `Ludork.exe`; no user-directory installation is needed. To retry a failed installer plug-in operation, run `tools\ScriptTools\ScriptTools.exe editor-official-plugins install-current-user tools\OfficialPlugins` from the installed release directory. These commands replace all existing user plug-ins and their data; they are deliberate installation operations, not startup commands.

All editor modes on Windows and macOS, including repository runs, use the current user's `~/Ludork` directory for persistent data. Windows typically resolves this to `C:\Users\<user>\Ludork`. No editor operation needs write access to Program Files. The shared locations are:

| Data | User-directory path |
|---|---|
| Settings, recent projects and window layout | `Ludork.plist` on macOS; `Ludork.ini` on Windows |
| Plug-ins, registrations and retained data | `Plugins`, `plugins.json` |
| Downloaded templates, all four variants | `Templates/v<base-version>` |
| Signing profile indexes | `*-signing.json` |
| Local server test configuration | `.localserver/<project-directory-name>` |
| Editor console logs | `Log/<stable-project-key>` |
| Windows WebView2 profile | `WebView2` |

The macOS settings file is an XML property list. Existing INI settings on macOS are not imported automatically.

Template cache validation and offline reuse use that user directory. Bundled development templates remain read-only package inputs. Locale initialization reads packaged resources without rewriting them; `--compile-locale` is an explicit packaging command. Project files, build output and `EditorCache`, game runtime saves, system credentials and system-managed WebKit data retain their existing locations. Old paths are not automatically read or migrated: with Ludork closed, copy any settings, plug-ins and cache you need into the matching new locations before removing an old installation.

## Ludork Server

The independent `Server` branch contains the Node.js 24 backend, viewer and deployment scripts. `python tools/pack_server.py build --output <archive.tar.gz>` runs `npm ci` and `npm run build` in its existing worktree, then archives `dist/LudorkServer`; use `--source <checkout>` to select it explicitly. The resulting deployable archive is distributed as a separate CI artifact or Release asset. Editor packages contain no Server directory or archive, and editor packaging does not require the Server checkout or Node/npm. Extract the server archive to a separate deployment directory and run its deploy script, which installs only production npm dependencies. See [Simple Server](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=06%2F01) for deployment, editor settings and production environment variables; [Native Client API](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=06%2F02) and [HTTP Protocol](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=06%2F03) cover client use.

`ScriptTools server-config generate <directory> <configuration>` creates private `LudorkGenerated/ServerConfig.hpp`. Enabled builds define `LUDORK_SERVER_AVAILABLE`; disabled builds retain the public API but only log a diagnostic and return without starting operations or invoking callbacks. `server-config cleanup <project>` removes generated Release headers after packaging or cancellation. Templates explicitly clear credentials and disable integration. Do not edit generated headers.

## Pull requests and automated packages

PR validation selects additive checks from added, changed, deleted and renamed paths:

| Changed inputs | Required check |
|---|---|
| Engine/native build | Release plain and FFmpeg templates on Windows x64 and macOS ARM64. |
| Editor and its project/resource/generator dependencies | Avalonia builds on both platforms, without editor packaging. |
| Lua or EmmyLua configuration | Full Game diagnostics with pinned EmmyLua and `--warnings-as-errors`; matching native stubs come from the Windows plain-template build. |
| Windows installer definitions, helpers, packers and tool versions | Parse PowerShell/WiX XML, compile native actions and compile the real Inno definition with `/O-` on Windows. Full WiX linking and product-payload validation run during release packaging. |
| Shared build/CI inputs | Every affected category. |
| Every PR | GlobalFunctions declaration and module-scope `require` check. |

`PR Validation` requires all selected checks to succeed. Failed, cancelled or unexpectedly skipped checks block it; unselected checks may skip, and documentation-only changes need no builds. New commits cancel older runs for the same PR. In the existing `main` branch rule or ruleset, require the GitHub Actions `PR Validation` status while preserving other protections; workflow files do not configure branch protection.

macOS dependency downloads use at most four attempts, with a 15-second connection timeout, a five-minute limit per attempt and five seconds between retries. No new attempt starts after the 15-minute retry window. Timeouts, refused connections and curl's transient HTTP errors can retry; permanent HTTP errors and archive/hash validation failures stop immediately. The same policy applies to local shell dependency tools. macOS CI sets pip's connection retries to three and its socket timeout to 60 seconds. Artifact downloads retain the download action's built-in retries.

Install the local staged-file check once per clone:

```sh
git config --local core.hooksPath .githooks
```

The hook runs `python -m ScriptTools.global_functions_check --staged` against the index. Omit `--staged` to check the worktree. Python 3.12+ is required. Lua modules under `Scripts/GlobalFunctions` must not occupy native function-group/root-function paths or call `require` at module scope; the check includes native declarations hidden from Blueprint metadata.

| Workflow | Development output |
|---|---|
| [Export Package](../.github/workflows/export-package.yml) | Both platforms, four embedded templates per platform, independent template archives and a built Server package. |
| [Export Package Windows](../.github/workflows/export-package-windows.yml) / [macOS](../.github/workflows/export-package-macos.yml) | The same complete package for one platform. |
| [Export Editor](../.github/workflows/export-editor.yml) | Both platforms with `--without-templates`. |
| [Export Editor Windows](../.github/workflows/export-editor-windows.yml) / [macOS](../.github/workflows/export-editor-macos.yml) | The same template-free package for one platform. |

Combined workflows run at 02:00 and 14:00 UTC+8 (`0 6,18 * * *` UTC); all six allow manual dispatch. Pushes to `main` that add, modify, delete or rename files under `.github/**` or Python scripts matching `**/*.py` immediately trigger both Export Editor and Export Package. Python scripts at the repository root are included. These pushes build development packages; other branch pushes and PRs do not trigger these exports. Each mode serialises push-triggered, scheduled and manual temporary runs and has its own successful default-branch dual-platform baseline. Scheduled runs compare both the main commit and the resolved remote `Server` commit. Push-triggered and manual runs always build and update that baseline only on successful default-branch runs. Failed, cancelled, skipped, other-branch and tag runs do not update it; legacy complete-package runs named Export Editor do not count as editor-only successes.

All exports use clean runners without dependency/build caches. Tools, compiler, editor and Windows launcher are rebuilt; complete packages also rebuild both native template variants. Artifacts are passed only between jobs in the current run. Platform/mode concurrency groups are independent. Editor-only preparation builds ScriptTools, `luac` and Windows GNU Make without configuring Game or downloading its native dependencies.

Temporary packages use `--dev` and expire after seven days:

| Artifact | Layout |
|---|---|
| `Ludork-editor-<fullVersion>-windows-x64.7z` | Template-free Windows package; filename is also the artifact name. |
| `Ludork-<fullVersion>-windows-x64.7z` | Complete Windows package. |
| `Ludork-editor-macos-arm64-<sha>` | Template-free macOS DMG artifact. |
| `Ludork-macos-arm64-<sha>` | Complete macOS DMG artifact. |
| `LudorkServer-<40-character-server-SHA>.tar.gz` | Independent built Server package. |

Windows 7z files have no outer ZIP, contain the root launcher, use multithreaded LZMA2 level 5 and pass `7z t` before upload. Template-free consumers select a successful target-platform job with a unique unexpired exact-name artifact, continuing to earlier runs if unavailable. Combined Package consumers also require `Record successful package (<server-SHA>)`; an unchanged scheduled run may succeed without producing artifacts. Editor-only baselines use `Record successful editor (<server-SHA>)`.

All export runs resolve the remote `Server` tree, install locked dependencies with Node.js 24, build the backend and management website, and archive the deployable `LudorkServer/` directory. It contains compiled JavaScript, static frontend assets, Lua files, production dependency manifests, deployment scripts and notices; private project code, credentials, user data, TypeScript inputs and node_modules are excluded. Each combined run builds one Server artifact; single-platform runs build their own. The archive is uploaded independently of the editor packages. Extract with `tar -xzf <archive>` before deployment.

A `v*` tag must equal `v<EDITOR_VERSION>`. It runs Export Package with `--release` and creates or updates a **draft** Release with generated notes and exactly 12 assets: Windows MSI and EXE, macOS DMG, eight templates and `LudorkServer-<EDITOR_VERSION>.tar.gz`. Formal installers omit templates. Both Windows installers reuse the same release payload and BuildInfo snapshot, and each has a separate seven-day workflow artifact. Installer creation and upload are restricted to formal tag runs. Reruns remove obsolete Windows installer, macOS DMG and Server attachments from the draft. Published releases are not overwritten, and drafts are never published automatically. Only the upload job has `contents: write`.

Each Package platform uploads these four archives directly; Export Editor uploads none:

| Template | Windows x64 | macOS ARM64 |
|---|---|---|
| Source | `game-source.zip` | `game-source.tar.gz` |
| Source + FFmpeg | `game-source-ffmpeg.zip` | `game-source-ffmpeg.tar.gz` |
| Standalone | `game-standalone.zip` | `game-standalone.tar.gz` |
| Standalone + FFmpeg | `game-standalone-ffmpeg.zip` | `game-standalone-ffmpeg.tar.gz` |

Archive roots contain project files, hidden configuration, licences and matching preview data. Keep archive output outside the editor payload; macOS preserves executable permissions and relative library links. Public downloads use [Release Assets](https://github.com/JasonLeon01/Ludork/releases). Local checks do not establish hosted workflow success or branch-protection configuration.

### CI signing configuration

Both macOS package modes use the same secrets. Set `MACOS_SIGNING_CERTIFICATE` to a base64 `.p12`, with `MACOS_SIGNING_CERTIFICATE_PASSWORD` and optional `MACOS_SIGNING_IDENTITY`. Notarisation uses either `MACOS_NOTARY_APPLE_ID`, `MACOS_NOTARY_TEAM_ID`, `MACOS_NOTARY_PASSWORD`, or a base64 `.p8` in `MACOS_NOTARY_KEY` with `MACOS_NOTARY_KEY_ID` and `MACOS_NOTARY_KEY_ISSUER`. Without signing secrets the package uses ad-hoc signing.

## ScriptTools maintenance commands

Shell/batch scripts coordinate the ScriptTools runtime bundle. Build it explicitly after source changes; build and packaging commands consume the prepared bundle. Development entry points are `.tools/ScriptTools/ScriptTools` or `.tools\ScriptTools\ScriptTools.exe`; compiler output stays in `.tools/build/ScriptTools`. Installed editors use `tools/ScriptTools` and need no system Python. Keep the entire bundle together.

`runtime-versions.txt` records build/dependency versions; `runtime-files.json` records contents. `ScriptTools runtime-bundle validate <bundle-directory>` checks that manifest. Windows tool builds use deterministic MSVC compilation, `/Brepro`, disabled incremental linking and Nuitka `--reproducible=yes`; identical outputs require the same paths, sources, dependencies and toolchain. Editor-only and complete-package `luac` builds may differ; these settings do not make whole installers or signed macOS bundles byte-identical.

### Packaging metadata

`ScriptTools/packaging_constants.py` owns shared lists and exit codes; `packaging_names.py` owns static `APP_NAME` parsing and filenames; `packaging_metadata.py` owns release settings and the per-invocation timestamp. `resource_constants.py` owns resource groups, prefixes and extensions. Edit these sources and rebuild ScriptTools; generated C# constants are build outputs.

The following are subcommands of `ScriptTools packaging-constants`:

| Command | Purpose |
|---|---|
| `list <name> [--separator space] [--windows]` | Print a list, one entry per line by default; optionally use one line or Windows path separators. |
| `app-name <project-root> [--artifact]` | Print the static application name or its safe filename form. |
| `check-package-metadata <project-root> [--version <version>] [--dev\|--release]` | Validate identity, System title and release settings; sample app name exits 24, invalid/missing metadata exits 23. |
| `release-version --version <version> [--dev\|--release]` | Print version-preview JSON without a project. |
| `release-build-info <directory> --version <version> [--dev\|--release]` | Write `BuildInfo.json` and print its full version. |
| `build-info <file> --field <name>` | Read a validated field, including `fullVersion`, `dev` or `appleBuildVersion`. |
| `resolve-metadata <project-root> <output-json> [overrides]` | Freeze metadata once per packaging invocation. |
| `package-name --metadata <snapshot-json>` | Print the versioned artifact name. |
| `prepare-output <project-root> <dist-root> --metadata <snapshot-json>` | Prepare `<dist-root>/<game>-<full-version>`, preserving siblings; reject links/ancestors or protected-content overlap. |
| `write-build-info <runtime-root> --metadata <snapshot-json>` | Write staged `Data/BuildInfo.json` before finalisation. |
| `csharp <output.cs>` | Generate `Ludork.Services.ProjectToolConstants`. |

List names are `editor-cache-directory`, `package-cache-directories`, `resource-groups`, `runtime-legal-files`, `template-names`, `cpp-template-names`, `standalone-template-names`, `plain-template-names`, `ffmpeg-template-names` and `native-lua-files`. Reuse one metadata snapshot throughout a pack; BuildInfo readers reject fields inconsistent with their version/time.

Additional ScriptTools commands:

| Command | Purpose |
|---|---|
| `editor-build-metadata csharp <versions.conf> <output.cs> [--build-info <snapshot>]` | Generate assembly metadata; without a snapshot, use development metadata. |
| `editor-build-metadata validate-payload <root> --build-info <snapshot> [--content-root <directory>]` | Validate dev/release BuildInfo placement. |
| `template-archive <templates-root> <output-directory> --platform windows\|macos` | Archive all four templates. |
| `configure-project-template <Main.proj> <cpp:true\|false> <ffmpeg:true\|false> --versions-config <versions.conf> [--version X.Y.Z]` | Configure template version metadata. |
| `engine-constants <EngineState.hpp> <output.cs>` | Generate editor constants during .NET builds; C++ owns the cell size. |
| `runtime-constants cpp <project-root> <output-directory>` | Generate neutral headers under `LudorkGenerated`. |

`runtime_formats.py` owns archive/encrypted-format values. Runtime constants also read the single decimal `LUDORK_MAX_SHADER_LIGHTS` definition (1–256) in `Assets/Shaders/Global/UnoccludedLightPass.frag`. CMake generates headers for game, preview and no-Lua builds under Intermediate, without rewriting identical output. Python-definition changes require rebuilding ScriptTools and native code; shader-limit changes require a native rebuild before packaging.

### UI generation and previews

See [UI Authoring Workflow](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=03%2F04%2F01) for View/Controller ownership and [PreviewHost Build and Publication](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=07%2F05) for Source/Standalone lifecycle and package exclusions.

| ScriptTools command | Purpose |
|---|---|
| `ui-assets generate <project-root> [--check]` | Generate Lua Views and window declarations; `--check` reports missing, stale or obsolete output without writing. |
| `ui-assets manifest <project-root>` | Read-only JSON of input paths/SHA-256 hashes and expected/existing output paths, including window modules and Controller declarations. |
| `ui-assets validate <project-root> [--structure-only]` | Validate assets; structural mode needs no preview registry. |
| `ui-adapter-check <project-root>` | Check native adapter consistency. |
| `preview validate <project-root>` | Check published preview artifacts. |
| `preview ensure <project-root>` | Recover missing or mismatched Standalone preview JSON. |
| `preview registry <project-root>` | Ensure and print the project registry path. |
| `preview begin-build <project-root> <bin-directory> --configuration <Debug\|Release>` | Stop project preview connections and mark a build pending. |
| `preview publish <project-root> <bin-directory> --configuration <Debug\|Release>` | Publish matched metadata after linking and dependency preparation. |
| `preview copy <source-project> <target-project>` | Copy matched binaries to `Binaries` and preview JSON to `EditorCache`. |
| `preview copy --runtime-directory bin/<configuration> <source> <target>` | Reuse copied native-cache outputs without another `Binaries` copy. |

UI generation reads assets and declarations without executing Lua or requiring a Host. Window modules need direct `require` imports and one final `return Ui.DefineWindow(...)`; the optional native base defaults to `Engine.Canvas`. Conflicts are checked before writes; unchanged files retain timestamps and only obsolete generator-marked files are removed. Neither `generate` nor `manifest` runs plug-in hooks or publishes a complete editor Export record. Low-level builds/run/pack commands consume existing UI, locale and material exports; mobile `--check` is preflight only.

System descriptors belong to the project's `Engine/Source/Core/include/UI/UiControlAdapterDescriptors.hpp` and are exported by `PreviewHost --describe`; tools read project JSON rather than an embedded control table. Publication combines `--describe` with `--build-info`, writing only `EditorCache/Preview.json` and `EditorCache/UiPreview.registry.json`. Manifest v3 records a safe project-relative runtime directory, binary names, configuration and registry identity. The build ID hashes declared binaries and exact UTF-8 registry bytes, including the final LF under the fixed registry filename. Identical content is not rewritten.

Preview copy preserves unrelated cache content and identical shared libraries, rejecting dependency mismatches. Standalone recovery replaces only the two JSON files and is blocked by `Preview.building`; Source metadata requires a rebuild. Staging validation passes `--registry <source-project>/EditorCache/UiPreview.registry.json` explicitly. Native caches need current publication metadata before reuse; no old-path fallback is provided.

On macOS, preview binaries keep `@loader_path` and relative library links; publication checks dependencies/signatures without relocating shared libraries. Mobile packers prepare a local desktop registry and supply it to resource validation and cross-CMake; no mobile Host is executed or desktop preview included in the device package. Descriptors must be platform-independent.

Validate host-to-implementation dependencies with:

```sh
.tools/ScriptTools/ScriptTools impl-boundary-check Game
```

```bat
.tools\ScriptTools\ScriptTools.exe impl-boundary-check Game
```

The checker discovers same-name C++ host/directory and Lua host/module pairs, excluding Lua children with their own mirrored declarations. It reports reverse dependencies, host definitions in implementation folders, Lua partial-class/mixin reuse and Standard ClassRuntime layer violations. Private-header checks cover both Engine and Application, including parent headers reached through include roots. First-party Engine C++ must not embed `Source.*` project Lua module paths or refer to those types in binding annotations; this check covers static strings and does not trace dynamically constructed paths. CMake exposes `ImplBoundaryValidate`.

### Material export

Editor **Export** compiles saved material graphs through the native PreviewHost after plug-in hooks, generating `Assets/Shaders/MaterialGenerated`. Ordinary Save and command-line build/run/pack tools do not generate material programs. Run and packaging preflight reject missing, modified or obsolete exports.

| ScriptTools command | Purpose |
|---|---|
| `material-assets manifest <project-root>` | Read-only input SHA-256 hashes, material paths, required `programs: [{ material, layer }]`, expected outputs and the union of expected/existing output paths. |
| `material-assets validate <project-root>` | Check source hashes, the export format and all generated file hashes without running a compiler or writing files. |
| `material-assets copy-inputs <project-root> <destination>` | Copy the optional root `MaterialExport.json` into an existing destination, or remove its stale destination copy when absent from the source. Used by Standalone conversion and macOS/mobile staging. |

Every material, including the built-in default, has a single-material program. Layered programs are the sorted, deduplicated union of actual tile/autotile usage in all ordinary maps under `Data/Maps` (including world-map children) and additional combinations from the optional root `MaterialExport.json`. Map visibility and the starting map do not limit scanning. Manage additional ordered pairs through [Game → Material Combinations](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=02%2F14#material-combinations), not by hand; scripts are not scanned for dynamic combinations. The file uses `additionalCombinations: [{ material, layer }]`, with full material paths and an empty base for the default. A missing file or an empty array contributes no extra combinations.

`project-state-worker` exposes the same manifest as `material-manifest`. Inputs include all material, map, tileset and autotile JSON files, `MaterialExport.json` (or a `"missing"` marker), and `EditorCache/Preview.json`. Content/configuration input hashes normalise CRLF to LF while preserving all other bytes; additions, deletions and other changes invalidate the export. The preview input is hashed without normalisation and invalidates local editor checks after compiler publication changes. The full input snapshot remains in `EditorCache/ProjectExport.json` and is checked during export. The shared export record is `Assets/Shaders/MaterialGenerated/export.json`, with `formatVersion: 1`, `generator: "Ludork Material Export"`, all project-relative input hashes except the preview manifest, and output hashes excluding the record itself. CLI validation compares the same content/configuration inputs and generated files without requiring the export machine's preview build identity, allowing Debug exports to be packaged with Release or another target platform. Input mismatch errors list the affected project-relative paths; see [Material export](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=02%2F14#export-for-runtime) for line-ending rules and recreating existing CRLF-based records.

Before hashing and installing native compiler JSON outputs, the editor recursively sorts object keys with ordinal comparison, preserves array order and numeric precision, and writes compact UTF-8 without a BOM with one final LF. GLSL bytes are preserved, and generated-file hashes always cover exact output bytes. The shared `export.json` record also uses LF on every platform. Identical export inputs and compiler results therefore produce byte-identical generated directories, including `export.json`, across Source/Standalone and Debug/Release exports; unchanged files retain their timestamps. The record is used by editor exports and tool validation; the game runtime never reads it. Its output list identifies owned files when the editor removes obsolete generated outputs. Packaging stages `MaterialExport.json` for validation, then removes it and the export record before encryption and archiving. Source projects and templates retain the optional combination file, and the source project's record remains available for later checks; game packages contain neither authoring/validation file nor the record's encrypted `export.ldc` form.

Generated programs live under `Materials/<path relative to Data/Materials, without the final .json>`; the empty material uses `Builtin/Default`. Each base directory contains `resource.json`, a `single` program and `layers/<layer path relative to Data/Materials, without the final .json>` programs. Every program has `vertex.vert`, `color.frag`, `surface.frag`, `transmission.frag`, `source.frag` and `program.json`. The editor owns transactional installation and obsolete-file cleanup; the native compiler owns GLSL and metadata. Templates omit all generated materials, while game packages retain their shaders and runtime metadata. `--encrypt-shaders` covers `Assets/Shaders`, including the generated shaders. `--encrypt-data` covers ordinary Data JSON and generated JSON metadata under `Assets/Shaders/MaterialGenerated`; both use the same encrypted data format. `--use-ldpak` stores the generated runtime files in `Assets.ldpak`. Material names that would make a base directory, program directory or output file collide after removing the final `.json` extension are rejected.

## Game packaging options

For identity, release versions, output layouts, encryption and platform prerequisites, see [Run, Debug and Package](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=02%2F11). Direct commands consume existing UI/locale/material exports; run editor **Export** first or provide a deliberate equivalent export step.

Desktop game preferences and saves use `~/<APP_NAME>/` in editor Play, IDE launches, direct runs and packaged games: `Main.plist` on macOS, `Main.ini` on Windows and Linux, and `Save/` on all three. Projects with the same `APP_NAME` share this data for the current user. `APP_NAME` remains the static declaration in `Scripts/Entry.lua`; changing it selects a different user directory without migrating files. iOS retains `~/Library/Application Support/<APP_NAME>/`, and Android and HarmonyOS retain their app-private user-data roots. The runtime launches an explicit command-line script or `Scripts/Entry.lua`; it does not read a project-local configuration to choose the entry. Templates and game packages continue to exclude local settings and saves. See [Game configuration](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=02%2F02#game-configuration) for the path APIs and configuration behaviour.

| Option | Behaviour |
|---|---|
| `--version <base-version>`, `--release` | Override project release settings for this invocation. |
| `--dev` | Append the UTC+8 build hour; C++ Source only, still a Release native build. |
| `--compile-lua` | Compile staged Lua with `luac -s` and rename to `.luac`. |
| `--encrypt-saves` | Build a C++ Source runtime with `LUDORK_SAVE_AS_LDC=ON`; rejected for Standalone. |
| `--use-ldpak` | Archive complete staged Assets/Data/pruned Scripts after encryption. |

`ScriptTools validate-ldpak-source <runtime-root>` runs source preflight. Standalone save defaults use `SAVE_AS_LDC` before any `require` in Entry; see [Saves](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=03%2F05%2F06#saves). Retain matching unstripped native outputs and debug symbols for release diagnostics.

Final distribution symbol handling is platform-specific:

| Platform | Packaging step |
|---|---|
| Windows | Exclude separate PDB files; do not run a generic strip tool on EXE/DLL files. |
| macOS | Apply `strip -x` to staged Main and real dynamic libraries, skipping symlinks, then sign. Preserve source `bin`, templates and shared preview libraries. |
| iOS | Xcode strips before signing with `DEPLOYMENT_POSTPROCESSING=YES`, `STRIP_INSTALLED_PRODUCT=YES`, `STRIP_STYLE=non-global`; input static libraries remain intact. |
| Android | Apply NDK `llvm-strip --strip-unneeded` to staged `libludork.so` before Gradle packaging. |
| HarmonyOS | Hvigor strips Release native libraries before HAP signing; do not modify signed HAPs. |

### HarmonyOS and Android CLI

Both targets require Apple Silicon macOS and C++ Source projects. SDK prerequisites, device limits and output names are in [platform packaging](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=02%2F11#harmonyos).

```sh
./tools/pack_harmony.sh --device-form mobile --graphics-api opengl-es Game
./tools/pack_harmony.sh --device-form 2in1 --graphics-api opengl Game
./tools/pack_harmony.sh --device-form 2in1 --graphics-api opengl-es Game
./tools/pack_android.sh --version 1.0.0 --release Game
```

HarmonyOS defaults to OpenGL ES for Mobile and OpenGL for 2in1; Mobile rejects OpenGL. It targets API 22 and delegates to `ScriptTools harmony-pack`. Add `--sign --keystore <absolute-p12-path> --certificate <absolute-cer-path> --profile <absolute-p7b-path> --key-alias <alias>` for a signed HAP. Provide exactly two UTF-8 newline-delimited passwords on stdin: keystore password, then key password (repeat the value if equal).

```sh
printf '%s\n%s\n' "$HOS_STORE_PASSWORD" "$HOS_KEY_PASSWORD" |
  ./tools/pack_harmony.sh --device-form mobile --graphics-api opengl-es \
    --sign --keystore "$HOS_KEYSTORE" --certificate "$HOS_CERTIFICATE" \
    --profile "$HOS_PROFILE" --key-alias "$HOS_KEY_ALIAS" /absolute/path/to/game
```

Hvigor first builds unsigned. Signing invokes `hap-sign-tool.jar sign-app` with `localSign`, `SHA256withECDSA` and API 22, verifies signature/Profile/bundle/version/libraries, then atomically publishes the signed HAP. Failures retain existing successful output; temporary signing work is cleaned and credentials are not persisted. `--check` validates without building or publishing; with `--sign`, it consumes the same passwords and checks tools, key and valid matching Debug/Release Profile. Add `--export-to-device` with signing to install and launch; `--check --export-to-device --sign` checks device match/authorisation without installing.

Android delegates to `ScriptTools android-pack`. SDK lookup is `ANDROID_SDK_ROOT`, then `ANDROID_HOME`, then `~/Library/Android/sdk`; it selects the highest complete stable NDK r27+. `LUDORK_CMAKE` selects a host CMake executable (3.28+, Unix Makefiles and `/usr/bin/make` required). It does not use SDK CMake, Ninja, SDK Manager, emulators or adb. The host and Gradle wrapper come from `Game/Engine/PlatformHosts/Android`; preserve wrapper JAR/configuration/notices and macOS executable permissions. The wrapper selects Gradle 9.5.0 and AGP 9.3.0; macOS templates omit `gradlew.bat`.

Android signing uses `--sign --keystore <absolute-path> --key-alias <alias>` and the same two-password stdin protocol. `--check` validates without publishing. Signed output is verified before publication, credentials are not persisted, and the command never installs or launches the APK. Reuse the same signing key for application updates.

## Editable Standalone projects

Use `pack_standalone` for an exported C++ Source project. It builds the current desktop platform in Release through `build_standalone`, then publishes an editable project with its launcher, runtime libraries and matching preview. It does not create a game distribution or regenerate Export outputs.

```sh
sh tools/pack_standalone.sh --check /path/to/SourceProject
sh tools/pack_standalone.sh /path/to/SourceProject /path/to/StandaloneProject
```

On Windows, use `tools\pack_standalone.bat <source-project> [standalone-directory]`. The optional destination is the exact project directory; its default is `<source-project>/dist/<source-directory-name>-standalone`. `--check` validates source configuration, existing UI/material exports, output paths and applicable signing materials without building or publishing. Both entry points call `ScriptTools standalone-pack --tools-directory <tools-directory>`; installed editor packages include the same tools.

The output retains editable Assets, Data and Scripts, existing UI/locale/material exports, Lua declarations, the locale workbook, UI design data, optional `MaterialExport.json`, licences and applicable third-party source material. It retains `.emmyrc.json` when present and filters optional VS Code settings to Lua settings, EmmyLua attach configurations and the EmmyLua extension. `Main.proj` retains its settings except `Cpp=false` and `packaging.dev=false`. Native source/build trees, animation caches, local saves/logs and Git are excluded. Only preview publication metadata is copied into `EditorCache`; no `ProjectExport.json` is copied or fabricated. Existing exported files can run directly, while editor Play may require a new Export.

Editor **Pack Project → Standalone project** saves and fully exports first, then calls this tool without before-Pack hooks. Conversion does not accept version/channel, encryption, Lua compilation or `.ldpak` options, and does not rewrite the source project's packaging preferences. CLI callers must export beforehand; missing or outdated UI/material outputs fail validation. `build_standalone` remains the lower-level runtime assembler used by templates and game packaging.

macOS always signs the native code. It accepts `--signing-identity`, `--certificate`, `--entitlements` and `--ignore-environment` with the same environment/profile precedence and certificate-password stdin protocol as game packaging. Without an identity it uses ad-hoc signatures. It does not create an `.app`, seal the project directory or allow notarisation options. Entitlements apply only to root `Main`; signatures and the refreshed preview build ID are verified before publication. `ScriptTools macos-sign --standalone <project-directory>` provides the same signing operation for an existing project.

Server integration is disabled by default. Explicit `LUDORK_SERVER_ENABLED=1`, `LUDORK_SERVER_URL` and `LUDORK_SERVER_KEY` compile the Release connection into the runtime; generated server configuration is cleaned up after the build. Changing that connection later requires another Source conversion. No signing or server secrets are written into project configuration.

Assembly and validation use a staging directory. Failed builds, signing or validation leave the previous successful output intact. Output paths cannot replace the source project or its ancestors, and outputs inside the source must be beneath `dist`. Output ancestors must not be symbolic links or junctions. Close running copies before replacing an existing output.

## macOS signing and notarisation

`pack_project.sh` and `pack_editor.sh` use `ScriptTools macos-sign`. With no identity they apply ad-hoc signatures. With a real identity they sign nested code before sealing the bundle, with hardened runtime and secure timestamps. Entitlements apply to the main executable. `Contents/MacOS` non-Mach-O signatures use extended attributes that must survive distribution. Editor packaging verifies the complete app before/after DMG creation and on the mounted final image.

| Command line | Environment variable |
|---|---|
| `--signing-identity NAME` | `LUDORK_MACOS_SIGNING_IDENTITY` |
| `--certificate PATH.p12` | `LUDORK_MACOS_SIGNING_CERTIFICATE` |
| `--entitlements PATH.plist` | `LUDORK_MACOS_SIGNING_ENTITLEMENTS` |
| `--notary-apple-id EMAIL` | `LUDORK_MACOS_NOTARY_APPLE_ID` |
| `--notary-team-id TEAMID` | `LUDORK_MACOS_NOTARY_TEAM_ID` |
| `--notary-key PATH.p8` | `LUDORK_MACOS_NOTARY_KEY` |
| `--notary-key-id ID` | `LUDORK_MACOS_NOTARY_KEY_ID` |
| `--notary-key-issuer UUID` | `LUDORK_MACOS_NOTARY_KEY_ISSUER` |

Environment values override CLI options; `--ignore-environment` disables that lookup. `--notarize` is opt-in with no environment equivalent. A `.p12` is imported into a temporary keychain using the first stdin password. `--signing-identity` selects among identities and accepts a SHA-1 fingerprint. Apple's `security import` requires that password in its process arguments; other password handling stays on stdin.

Notarisation requires a real identity and either Apple ID/team plus the app-specific password as the second stdin password, or an App Store Connect key/ID/issuer. It submits the DMG or a temporary app ZIP with `notarytool`, waits and staples the ticket. `--check` validates materials and performs a notarisation round trip without building. Failed submissions report the notary log when available while retaining the original failure status and submission ID.

Signing, notarisation and ticket operations retry temporary network/service failures with bounded command timeouts. The same submission ID is retained when waiting for notarisation. See [macOS packaging](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=02%2F11#windows-and-macos) for retry counts, time limits and failure handling. The DMG packer detaches the whole image device before conversion/cleanup; bounded forced retries preserve the image and work directory if detachment remains unresolved.

Packaging uses these additional signer options:

| Option | Packaging use |
|---|---|
| `--runtime-bundle <directory>` | Validate the original manifest, sign nested binaries, refresh hashes/sizes, then seal the app. Repeatable for resource bundles; not passed for a DMG. |
| `--preview-template <directory>` | Validate a bundled Standalone preview, sign its binaries, refresh its build ID and revalidate before sealing the editor. |
| `--templates <template-root>` | Sign independent templates and refresh Standalone previews before archiving. These archives are not separately notarised. |
| `--standalone <project-directory>` | Sign one editable Standalone project's native code and refresh/verify its preview, without an app bundle or notarisation. |

[Query macOS Notarization](../.github/workflows/macos-notary-status.yml) reuses `MACOS_NOTARY_*` secrets without a certificate. Leave `submission_id` empty to list submissions, or supply a UUID for status and completed logs. It does not submit, cancel or wait; workflow success confirms the query, not package acceptance.

## iOS signing

Without a certificate, `pack_ios.sh` uses the signed-in Xcode account and automatic signing. Supplying a certificate and provisioning profile selects manual signing, imports a temporary keychain and installs the profile for the build, removing it afterwards only if newly installed. The certificate password comes from stdin. Team/application mismatches are rejected.

| Command line | Environment variable |
|---|---|
| `--team-id TEAMID` | `LUDORK_IOS_DEVELOPMENT_TEAM` |
| `--certificate PATH.p12` | `LUDORK_IOS_SIGNING_CERTIFICATE` |
| `--provisioning-profile PATH.mobileprovision` | `LUDORK_IOS_PROVISIONING_PROFILE` |
| `--signing-identity NAME` | `LUDORK_IOS_SIGNING_IDENTITY` |

Environment values override CLI options; use `--ignore-environment` for CLI-only settings. Manual signing requires the team, certificate and profile together. `--check` validates without building. See [iOS packaging](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=02%2F11#ios) for SDK/device requirements.
