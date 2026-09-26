# Game tools

All scripts switch to the repository root before doing work. Use `.bat` on Windows 10 or newer on x64 and matching `.sh` scripts on Apple Silicon macOS 13.3 or newer. macOS tools find CMake from `PATH` or `/Applications/CMake.app`.

| Tool | Purpose |
|---|---|
| `init` | Prepare the generator environment and native dependencies |
| `setup_python` | Create `.venv` and install build-time Python requirements |
| `build_script_tools` | Build the ScriptTools runtime bundle under `.tools/ScriptTools`; compiler outputs stay under `.tools/build/ScriptTools` |
| `build_ui_preview_host` | Build and publish a project's native preview snapshot |
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
| `pack_ios.sh` | Build an iOS 15.0-or-newer IPA from a C++ Source project, with optional manual signing |
| `pack_harmony.sh` | Build a HarmonyOS API 22 Mobile or 2in1 arm64-v8a HAP, with optional device export |
| `pack_android.sh` | Build an Android arm64-v8a Release APK from a C++ Source project, unsigned by default with optional signing |
| `pack_editor.bat` | Publish and validate the self-contained Windows 10-or-newer x64 editor package with official plug-ins |
| `pack_editor.sh` | Publish and validate the self-contained macOS Apple Silicon editor DMG, with optional signing and notarisation |
| `pack_editor_msi.bat` | Wrap a release Windows editor package into the installer MSI |

Typical commands:

Use the editor's **Export** before running or packaging a project with the
command-line tools. These tools consume the exported UI and locale Lua files.

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

### Website and documentation

The website frontend lives in `docs/__default__` and uses React, MUI, and Vite.
Install its locked dependencies with `npm ci` from that directory, then run
`npm run dev` for local development or `npm run preview` after building. Both
servers expose the Markdown and images directly from the parent `docs` directory.

From the repository root, run `sh docs/build_docs.sh` on macOS or
`docs\build_docs.bat` on Windows to build the website and refresh its GitHub Pages
files. Node.js must satisfy the frontend's `package.json` engine requirement.
The scripts check all required outputs before replacing generated pages and
assets. Do not edit the generated HTML or asset bundles by hand.

GitHub Pages serves the repository's `docs` directory at `/Ludork/`. The homepage,
documentation, About, and third-party notices have separate HTML entries at
`/Ludork/`, `/Ludork/docs/`, `/Ludork/about/`, and `/Ludork/notices/`; Download links
directly to GitHub Releases. The notices page is reached from the shared footer
and keeps its page identity when switching language.
All pages use `?lang=en_GB` or `?lang=zh_CN`. Document selections use `doc` or
`path` under `/Ludork/docs/`, with an optional heading fragment. Root-page document
queries are not documentation routes. Keep `.nojekyll` in the published output.

Keep public Markdown in `docs/en_GB` and `docs/zh_CN`, images in `docs/_images`,
and shared About text in `docs/About_*.md`. The editor notices are sourced from
`docs/THIRD_PARTY_NOTICES.md` and `docs/THIRD_PARTY_NOTICES_zh_CN.md`. Frontend
translations live in the typed `ludorkSiteMessages.ts` module. The editor packages
the two documentation language trees and their images; About and notice sources
retain their `docs/` paths through MSBuild and the platform packaging scripts.
Website HTML, JavaScript, and build sources are not included in editor packages.

The homepage uses `src/Ludork/assets/hero/home-hero.png` as its replaceable main
image. Its acknowledgements list is defined in `ludorkDependencies.ts`; platform
and dependency cards share the horizontal logo scroller, with manual scrolling
when reduced motion is requested. Icon sources are recorded in
`src/Ludork/assets/credits.md`.

`ScriptTools legal-resources editor <repository-root> <output-root>` writes the
editor's root licence and READMEs, notices under `docs/`, and licence indexes,
preserving their source-relative Markdown links. MSBuild and editor packaging invoke
it after copying resources. `legal-resources template-index` takes the same two
paths and writes only the two licence indexes so they point to the template's
own root runtime notices. Both commands leave the source documents unchanged;
the `Game` notices and runtime packaging filenames remain independent of the
editor notices.

### Animation MP4 export

After changing ScriptTools, run `tools/setup_python` and `tools/build_script_tools`
using the matching platform extension. The exporter uses Pillow from the Python
requirements; the built ScriptTools executable includes it. Provide an FFmpeg
executable with `libx264` and AAC on `PATH`, place it at
`.tools/ffmpeg/ffmpeg.exe` (Windows) or `.tools/ffmpeg/ffmpeg` (macOS), or pass
`--ffmpeg <executable>`. FFmpeg is a separate local prerequisite and is not bundled
by editor packaging.

```bat
tools\animation_to_mp4.bat
tools\animation_to_mp4.bat Game --input Game/Data/Animations/attack.json --output Game/EditorCache/attack.mp4
tools\animation_to_mp4.bat Game --size 512x512 --background "#202020" --overwrite
```

On macOS, use `sh tools/animation_to_mp4.sh` with the same arguments. The direct
command is `ScriptTools animation-mp4 [project] [options]`; the project defaults
to `Game`. `--input` accepts one source JSON or a directory searched recursively,
defaulting to `<project>/Data/Animations`. `--output` defaults to
`<project>/EditorCache/AnimationMp4`; directory exports preserve relative subdirectories.
Explicit input/output paths are relative to the repository root when using the
wrappers. A single input also accepts an output `.mp4` filename.

The exporter reads saved `type: animation` data and loose `/Game/Assets/...`
files, retaining the source frame rate, timeline order, linear transforms,
horizontal flip and sound timing/trimming. It exports one playback through the
last image or sound segment, rounded up to whole frames; the remaining sound-only
portion shows the background. The automatic even-sized canvas contains all
sampled frames with the animation origin at its centre. `--size WIDTHxHEIGHT`
sets an even-sized canvas without rescaling; content outside it is cropped.
`--background "#RRGGBB"` selects the opaque background (black by default), and
`--mute` removes audio while retaining the timeline duration. Time tags are not
encoded as video events. Compressed caches and `.ldpak` packages are not inputs.

Existing outputs require `--overwrite`. Each file is encoded to a temporary
location and published only after FFmpeg succeeds. Batch export continues after
individual failures and returns a nonzero exit code if any file failed.

The editor **Construct** button runs `build_cpp` for C++ Source projects.
**Export** generates UI Lua files and runs project export hooks, including
Official Locale Tools. General Data and game variable Lua files still update
when their data is saved. **Play** requires a successful Export and, for C++
Source projects, a successful Debug Construct. Its disabled hint and pre-run
checks consider Construct first, then Export. After the first success, changed
native inputs prompt to construct and play; changed export inputs or outputs
prompt to export and play. Editor **Pack** runs Export before packaging and
stops if export fails. The editor workflow is in
[Running, Testing and Packaging](<../docs/en_GB/02.Editor User Guide/08.Run Debug and Package.md>).

The project export record is local state in `EditorCache/ProjectExport.json`. Templates
exclude that record, generated UI Views and declarations, and generated locale
catalogues. They retain handwritten Lua, General Data and variable exports, the
locale workbook and `Source/Locale/Core.lua`, and native binding stubs/metadata.

Both repository and installed-editor `build_cpp` scripts record the latest
successful native build with `ScriptTools native-build-state`, in
`EditorCache/NativeBuild-<configuration>.json`. Starting another build preserves that
record and writes a pending marker. A failed or cancelled build, or inputs
changed during compilation, leaves the marker in place and makes `check` report
that compilation is required. Only a successful build with unchanged inputs
replaces the successful record and clears the marker. A first build that fails
does not create a successful record.

`native-build-state check <project> <Debug|Release>` prints
`{"current":true,"detail":""}` for a current build, or `current:false` with a
reason. Both are normal checks with exit code 0; invalid input or an unreadable
state returns 2. `begin` and `complete` are the build scripts' paired operations;
`complete` returns 1 if inputs changed during compilation.

The editor prewarms one read-only `ScriptTools project-state-worker <project>`
process per open project. Requests and responses use JSON Lines over stdin and
stdout. Each request has `version:1`, an integer `id`, and an `operation`:
`ready`, `native-check` (with `configuration:Debug` or `Release`), or
`ui-manifest`. Responses echo `id` with either `result` or `error`. Diagnostics
use stderr. Every check reads current content; the process does not cache check
results or write build/export records. The editor serialises requests, restarts
the worker when the ScriptTools bundle changes or the process exits, and stops it on cancellation or
project close. Play keeps its initial and final Construct → Export checks.

The check compares the paths and contents of first-party native source/header,
CMake and resource files, the `Main.proj` FFmpeg switch, and the compiled Windows
icon. It also checks the identity of the executable and dynamic libraries.
Lua, UI, locale, generated files and third-party trees do not make Play require
compilation. When no build is pending, restore the previous source contents to
reuse an unchanged successful build. Build records are specific to configuration,
platform and architecture; the editor requires Debug. Use `build_cpp` after
changing third-party dependencies or external CMake cache options. Direct
`cmake --build` does not publish this record; a subsequent incremental
`build_cpp` makes its result available to editor Play.

`create_templates` accepts `--variant plain` or `--variant ffmpeg` to build one
source/Standalone pair; the matching `create_templates_plain` and
`create_templates_ffmpeg` scripts are thin entry points for automation. Without
`--variant`, it rebuilds all four templates.

Both editor packers accept `--without-templates` to produce an editor and its
build/package tools without a `Templates` directory. It is mutually exclusive with
`--templates <folder>`; omitting both keeps normal template generation. This mode
still requires ScriptTools, the host Lua compiler and the Windows GNU Make tool,
but does not require the Game native dependency tree or FFmpeg source archive.

Windows and macOS automation may pass `--templates <folder>` to copy an already
generated set of four editor templates. Each Standalone template must contain
its own matched preview snapshot; C++ templates contain preview source only.
The normal command regenerates the templates before packaging the editor.
The prepared template folder must remain outside `obj/editor-package`, which is
recreated during packaging.

`create_templates --native-cache <folder>` stores native outputs separately by
`plain`/`ffmpeg` and `Debug`/`Release`. A matching entry supplies runtime binaries,
the Windows game launcher, and the seven generated Lua stub/metadata files;
templates still copy current Game project content and run the packaging validations.
Source templates omit SFML's upstream test fixtures, which are not used by the
game build and have filenames that HFS+ normalizes during macOS DMG creation.
Keep this folder outside both Game and the template output. The caller must
invalidate it when native sources, bindings, dependencies or build options
change. `build_standalone --use-current-build` packages the matching existing
`bin/<configuration>` output and Windows `build/launcher/<configuration>`.

`pack_editor --use-current-editor-build` restores NuGet dependencies and runs
`dotnet publish --no-build` against matching Release outputs for the editor and
its two contract projects. It still refreshes Content and compiles current locale
data in a clean staging directory. Windows also accepts `--launcher <file>` for
a prepared editor launcher outside that staging directory.

`EDITOR_VERSION` in `versions.conf` is the editor's own base version. `Ludork.csproj`
derives its `<Version>` from that entry, so the assembly, Windows launcher resource,
macOS bundle metadata and installer all report the same number. Both editor packers
accept `--version <base-version>` and mutually exclusive `--dev` / `--release`,
defaulting to `--dev`; an override applies to that invocation only and does not
change `versions.conf`. They follow the same identifier rules as the game packers:
a formal package keeps the base version, and a dev package appends the UTC+8
`YYYYMMDDHH` build hour, as in `1.0.0.2026010107`.

```bat
tools\pack_editor.bat --release
tools\pack_editor.bat --version 1.1.0 --dev
```

```sh
./tools/pack_editor.sh --release
```

Each package receives `BuildInfo.json` at its content root, which is the package
root on Windows and `Contents/Resources` inside the macOS app bundle, with the same
`version`, `fullVersion`, `dev` and `builtAt` fields as a game package. The About
dialog shows `fullVersion`. Binary version fields hold the base version, because
Windows resources and .NET assembly versions reject components above 65535; the full
version names the DMG, its volume and the packaging log line.

### Pull requests and automated packages

PR validation selects checks from changed paths, including added, deleted and
renamed files. Engine and native build inputs require Release templates for
Windows x64 and macOS ARM64, both plain and FFmpeg. Editor sources and their
project, resource and generator dependencies require Avalonia builds on both
platforms without packaging the editor. Scripts and EmmyLua configuration changes
require full `Game` workspace diagnostics from a pinned EmmyLua version, with
`--warnings-as-errors`. This check first obtains matching generated native stubs
from the Windows plain template build, reusing that build when Engine validation
also needs it, and generates the remaining workspace declarations before checking.
Shared build and CI inputs select every affected check; the categories are additive.
Every PR also checks the Lua modules in `Scripts/GlobalFunctions` against the
native `GlobalFunctions` binding declarations. A Lua module must not occupy a
native function-group or root-function path, and modules in this directory must
resolve their `require` dependencies inside functions rather than at module scope.
The check reads C++ annotations, including bindings hidden from Blueprint metadata.

Install the repository's staged-file check once per clone:

```sh
git config --local core.hooksPath .githooks
```

The pre-commit hook runs `python -m ScriptTools.global_functions_check --staged`
against the exact Git index. Run `python -m ScriptTools.global_functions_check`
to inspect the current worktree without staging files. Python 3.12 or newer is required.

The always-running `PR Validation` check succeeds only when every selected check
succeeds and the GlobalFunctions check passes. A selected check that fails, is
cancelled or unexpectedly skips blocks it; unselected checks may skip, and
documentation-only PRs pass without builds.
New commits cancel older validation runs for the same PR. In GitHub repository
settings, edit the existing rule or ruleset targeting `main`, require status
checks before merging, and add `PR Validation` from GitHub Actions while preserving
the other protections. Workflow files alone do not enable this merge requirement;
changing branch protection requires repository administration permission.

[Export Package](../.github/workflows/export-package.yml) packages the default branch
at 02:00 and 14:00 UTC+8 (`0 6,18 * * *` UTC), or a selected ref on manual dispatch.
Ordinary pushes and PRs do not create complete editor packages. Temporary package
runs are serialized; after acquiring that slot, a scheduled run skips when its
commit equals the latest successful dual-platform temporary package of the
default branch. Manual runs always build, including the same commit. Successful
manual default-branch packages update that baseline; skipped, failed, cancelled,
other-branch and tag runs do not. Scheduled and manual runs package both platforms
with `--dev`, so their artifacts carry the dated full version. Temporary artifacts
retain their names and expire after seven days. The macOS artifact is named
`Ludork-macos-arm64-<sha>` because the DMG is signed and notarised whenever the
`MACOS_SIGNING_CERTIFICATE` secret is configured, and stays unsigned otherwise.
Configure `MACOS_SIGNING_CERTIFICATE` as a base64-encoded `.p12` with its
`MACOS_SIGNING_CERTIFICATE_PASSWORD` and optionally `MACOS_SIGNING_IDENTITY`, then
either `MACOS_NOTARY_APPLE_ID`, `MACOS_NOTARY_TEAM_ID` and `MACOS_NOTARY_PASSWORD`,
or `MACOS_NOTARY_KEY` as a base64-encoded `.p8` with `MACOS_NOTARY_KEY_ID` and
`MACOS_NOTARY_KEY_ISSUER`. Missing secrets keep the unsigned package.

[Export Package Windows](../.github/workflows/export-package-windows.yml) and
[Export Package macOS](../.github/workflows/export-package-macos.yml) manually
produce the same complete package for one platform, including all four templates.

For tests that do not need project templates, run
[Export Editor](../.github/workflows/export-editor.yml) for both platforms,
[Export Editor Windows](../.github/workflows/export-editor-windows.yml), or
[Export Editor macOS](../.github/workflows/export-editor-macos.yml). These
entries pass `--without-templates`: no template generation, native template builds,
template downloads or `Templates` directory. They retain the editor, official
plug-ins and build/package tools for existing projects. Tool preparation builds
ScriptTools and a standalone `luac`, plus GNU Make on Windows, without configuring
Game or downloading its SFML/LuaSF/FFmpeg dependencies. Creating a new project
requires templates from a complete package. Both macOS modes use the same signing
and notarisation secrets and DMG verification.

The template-free artifacts are `Ludork-editor-windows-x64-<sha>` and
`Ludork-editor-macos-arm64-<sha>`; complete packages retain `Ludork-windows-x64-<sha>`
and `Ludork-macos-arm64-<sha>`. All temporary artifacts expire after seven days.
Consumers of template-free editors inspect the combined or matching single-platform
Export Editor runs on `main`, require a successful target platform job and a unique,
unexpired exact-name artifact, and continue to earlier runs when unavailable.

The combined `Export Editor` also runs at 02:00 and 14:00 UTC+8, using the same
cron as `Export Package`. Each mode deduplicates scheduled runs against its own
latest successful default-branch dual-platform run, recorded by `Record successful
editor` or `Record successful package`; legacy complete-package runs under the old
Export Editor name do not count as editor-only successes. Manual runs always build.

The six entries reuse each platform's packaging steps and build on clean runners
without restoring or saving caches. Every export builds ScriptTools, the Lua
compiler, the editor and the Windows launcher anew; complete packages also
rebuild both native template variants. pip and compiler caches are disabled.
Artifacts pass the freshly prepared tools and templates between jobs of the
current workflow run only; packaging never selects a previous editor build or
native cache. Concurrency groups are
separate for each platform and package mode, so full packages do not hold up
scheduled editor-only builds. Scheduled and manual runs use `--dev`. Only a `v*`
tag push to `Export Package` uses `--release` and creates or updates a draft Release
with complete Windows MSI and macOS DMG packages, both including templates.

Consumers of the combined `Export Package` workflow must select a run whose `Record successful package` job
succeeded and whose requested artifact is still available and unexpired. Selecting
only the latest successful workflow is insufficient: a scheduled run skipped for
an unchanged commit also succeeds, but produces no package artifacts.

Pushing a `v*` tag packages both platforms with `--release`, builds the Windows MSI
and then creates a draft GitHub Release with generated release notes, that MSI and
the macOS DMG. The tag only selects the packaging mode; the version always comes
from `EDITOR_VERSION`, and the tag name is never parsed. A rerun updates the
existing draft; an already published release is not overwritten. No release
is automatically published, and only the release-upload job has `contents: write`.

Complete packages include the selected commit's Game templates; both modes
include its editor, tools, plug-ins, locale and docs. Local syntax and build checks do not
replace successful hosted Actions runs or confirmation of the `main` merge rule.

Both editor packaging scripts use the shared ScriptTools command
`editor-official-plugins prepare <source> <output-root>` to clean-copy the fixed
official plug-ins and generate their registry. The matching
`editor-official-plugins validate <output-root>` command verifies the exact
directory set, manifests, generated index and excluded build or user-state
artifacts.

`pack_editor.bat` builds a native Windows launcher with a static CRT and publishes
the self-contained editor under `Binaries`. Its output has this layout:

```text
dist/
├── Ludork.exe                 # Native launcher
├── BuildInfo.json
├── Binaries/
│   ├── Ludork.exe             # Actual editor
│   ├── Ludork.dll
│   ├── Ludork.deps.json
│   ├── Ludork.runtimeconfig.json
│   └── …                      # DLLs and .NET runtime files
├── Locale/
├── Templates/
├── tools/
│   └── ScriptTools/
│       ├── ScriptTools.exe
│       ├── runtime-versions.txt
│       ├── runtime-files.json
│       └── …                  # Bundled runtime dependencies
├── Plugins/
├── plugins.json
├── docs/
├── Licenses/
└── …                          # Readme and licence files
```

The installation root is the directory containing the launcher. It starts
`Binaries\Ludork.exe` with that root as the working directory and forwards
arguments, standard streams and the editor's exit code. Packaging invokes
`--compile-locale` through the launcher, validates the complete layout before
replacing `dist`, and restores the previous package if replacement fails. MSI
shortcuts and `.proj` associations also target the root launcher.

`OfficialBlueprintAI`, `OfficialLocaleTools`, `OfficialRandomMap`, and
`OfficialResourceCleanup` are placed
below the root `Plugins` directory, with `plugins.json` generated beside it from
their manifests. The published editor resolves its resources, plug-ins and
configuration against the installation root even when started directly from
`Binaries`. Runtime settings use root `Ludork.ini`; writable plug-in data uses
`Plugins/.data`. Repository development builds use `Plugins` and
`plugins.json` with the development marker. Desktop game Standalone packages retain their
own packaging layout.

`pack_editor_msi.bat` wraps the current `dist` package into the Windows Installer
database with the pinned WiX toolset. It reads `dist\BuildInfo.json`, refuses a dev
package and requires the packaged version to match `EDITOR_VERSION`, so run
`tools\pack_editor.bat --release` first. Without an argument it writes
`Ludork-<version>-windows-x64.msi` to the repository root; an argument selects a
different `.msi` path.

`pack_editor.sh` requires macOS on Apple Silicon with a logged-in Finder session,
the .NET 9 SDK, CMake, ScriptTools and the host Lua compiler. Including templates
also requires initialized Game project dependencies. It produces
`dist/Ludork-<version>-macos-arm64.dmg` for macOS 13.3
or newer. The mounted
volume visibly contains `Ludork.app`, an `Applications` link, and **Install
Official Plugins**; Finder hides the installer's `.command` extension. The user
first drags the app to Applications, then double-clicks the installer and
confirms the replacement. The official plug-in payload and generated registry
are hidden support items outside the app bundle. The installer validates that
payload and transactionally replaces `~/Ludork/Plugins` and
`~/Ludork/plugins.json`; it does not merge or retain a backup after success, so
existing third-party plug-in source, registrations, and `Plugins/.data` are
removed. An ordinary failure rolls back to the previous installation; if
rollback itself cannot finish, the installer reports recovery details instead
of claiming that the old installation is intact. The app registers Ludork
`.proj` files with Finder and includes their document icon.
Editor content, templates, documentation, and runtime tools are stored under
`Contents/Resources`; `Contents/MacOS` only contains the executable and its .NET
runtime dependencies. The app and installer are not signed or notarised.

The macOS association is owned by the app bundle and becomes unusable when the
app is removed. An installer or manual uninstall can explicitly remove the
LaunchServices entry before deleting the app:

```sh
/Applications/Ludork.app/Contents/Resources/tools/unregister_editor.sh
```

Shell files only coordinate commands. Metadata generation, project inspection,
version checks, packaging helpers, and Lua bytecode compilation are implemented
by the ScriptTools runtime bundle. Nuitka's standalone directory build publishes
only runtime files under `.tools/ScriptTools`, with the development entry point
`.tools/ScriptTools/ScriptTools` on macOS or
`.tools\ScriptTools\ScriptTools.exe` on Windows. Compiler outputs and caches
remain under `.tools/build/ScriptTools`.

Editor packaging copies the complete runtime directory to `tools/ScriptTools`
inside the installation's resource root. The installed entry point is
`tools/ScriptTools/ScriptTools` or `tools\ScriptTools\ScriptTools.exe`; keep its
neighbouring libraries and data files together when copying or moving it.
`runtime-versions.txt` records the build mode and dependency versions, while
`runtime-files.json` records bundle contents for
`ScriptTools runtime-bundle validate <bundle-directory>`. Package validation
runs this command through the installed entry point. On macOS it also verifies
the signatures of every Mach-O executable and library in the copied bundle.
The macOS editor signer receives that copied directory through
`macos-sign --runtime-bundle <directory>`. It validates the original manifest
before signing, regenerates it after signing the runtime's binaries, and seals
the enclosing app afterwards. This keeps the recorded sizes and hashes aligned
with the signed files without accepting an incomplete input bundle. The option
may be repeated for runtime directories inside the application's resources;
it is not passed when signing the DMG.
Complete editor packages also pass each Standalone template through
`macos-sign --ui-preview-template <directory>`. The signer validates each preview
before signing nested binaries, refreshes only its build ID against the signed
runtime, validates the resulting snapshot, then seals the editor app and checks
the signed Host description. Editor-only packages do not pass template paths.

Python is needed only when `init` or `build_script_tools` compiles ScriptTools.
Development build and packaging commands consume the prepared runtime bundle;
rerun `init` or `build_script_tools` explicitly after changing its sources.
An installed editor uses its bundled runtime and needs no system Python.

`ScriptTools/packaging_constants.py` owns the shared packaging exit codes,
editor-cache directory, licence lists, template names, generated native Lua
files, and common mobile project/dependency-cache
inputs. `ScriptTools/packaging_names.py` reads the static `APP_NAME` string in
`Scripts/Entry.lua` and owns package filename rules. `ScriptTools/packaging_metadata.py`
resolves project release settings, the System title and one UTC+8 build timestamp
for all platform metadata and artifacts. Its release-version half carries the
version rules, derived platform build numbers and `BuildInfo.json` handling that
editor packaging reuses without a project. The `packaging-constants`
command and C# constant generation live in `ScriptTools/packaging_cli.py`, which
consumes these shared modules; the shared constants have no dependency on the command or naming logic.
`ScriptTools/resource_constants.py` owns resource groups, logical path prefixes,
entry names and file extensions. Platform SDK, signing, bundle identifiers and
platform archive formats remain in their platform packers. Mobile dependency-cache
lookup keeps each platform's existing priority; the desktop native build retains its larger
dependency list.

Shell and batch tools read fixed lists through
`ScriptTools packaging-constants list <name>`. Supported names are
`editor-cache-directory`, `package-cache-directories`, `resource-groups`,
`runtime-legal-files`, `template-names`, `cpp-template-names`,
`standalone-template-names`, `plain-template-names`, `ffmpeg-template-names` and
`native-lua-files`. Output is one entry per line; `--separator space` emits a
single line, and `--windows` changes path separators for batch consumers.
`ScriptTools packaging-constants app-name <project-root>` prints the validated
`APP_NAME`; add `--artifact` to print its safe filename form. Desktop and mobile
packers share this reader and require exactly one top-level `local APP_NAME`
declaration with a non-empty static Lua string literal. Expressions that need
Lua execution are rejected.

`ScriptTools packaging-constants check-package-metadata <project-root>` validates
application identity, System title and release settings; it accepts `--version`
and mutually exclusive `--dev` / `--release` overrides. It rejects the unchanged
sample application name with exit code 24 and missing, unreadable or invalid
project metadata with code 23. `release-version
--version <version> [--dev|--release]` prints the release preview as JSON without
loading a project.

Editor packaging uses the same release rules without a project. `release-build-info
<directory> --version <version> [--dev|--release]` writes `<directory>/BuildInfo.json`
and prints the full version, and `build-info <file> --field <name>` prints one
resolved field of an existing `BuildInfo.json`, including `fullVersion`, `dev` and
`appleBuildVersion`. Both reject a file whose fields disagree with its own version
and build time.

Packers call `resolve-metadata <project-root> <output-json> [overrides]` once to
freeze the metadata for that invocation. `package-name --metadata <snapshot-json>`
prints the versioned safe filename. `prepare-output <project-root> <dist-root>
--metadata <snapshot-json>` prepares and prints the Windows package directory
`<dist-root>/<game>-<full-version>`, replacing only that child and preserving
siblings. It rejects links in the output directory or any ancestor and paths
that overlap protected project content. `write-build-info <runtime-root>
--metadata <snapshot-json>` writes staged `Data/BuildInfo.json` before data
finalisation. These commands all belong to `ScriptTools packaging-constants`;
reuse the same snapshot through the entire invocation rather than recapturing time.

`ScriptTools packaging-constants csharp <output.cs>` generates
`Ludork.Services.ProjectToolConstants` for the editor build. Generated constants
are build outputs; edit the Python source and rebuild ScriptTools before
building the editor or running packaging tools.

`dotnet build` and `dotnet publish` generate `obj/.../EngineConstants.g.cs` with `ScriptTools engine-constants <EngineState.hpp> <output.cs>`. The C++ declaration is authoritative for the editor cell size; rebuild ScriptTools after changing the generator.

`ScriptTools runtime-constants cpp <project-root> <output-directory>` generates
C++ headers under `<output-directory>/LudorkGenerated`. `runtime_formats.py`
owns LDPK and encrypted data/shader format values and field layouts;
`resource_constants.py` owns shared resource names. The generator also reads the
single `LUDORK_MAX_SHADER_LIGHTS` decimal definition from the project's
`Assets/Shaders/Global/UnoccludedLightPass.frag`, requiring a value from 1 to 256.
CMake runs the generator before its consumers in game, preview and no-Lua
builds. Generated headers stay under `Intermediate`, are recreated when missing,
and are not rewritten when their content is unchanged. Rebuild ScriptTools
after editing the Python definitions, then rebuild the native project. Changing
the shader limit also requires a native rebuild before packaging. Source
projects use the installed tools bundle; runtime binaries do not read tool
sources or generated headers.

System UI descriptors are owned by each project's
`Engine/Source/Core/include/UI/UiControlAdapterDescriptors.hpp`. The compiled
project Host exports them with `UiPreviewHost --describe`. C# and global
ScriptTools load the resulting project JSON; neither embeds a generated control
table, and adding controls with supported property types does not require
rebuilding those tools.

`ScriptTools ui-assets generate <project-root>` generates all Lua Views under
`Scripts/Internal/UI` and their typed declarations under `Scripts/stub/Internal/UI`
from `Data/UI/Assets`. It also generates public window declarations under
`Scripts/stub/Internal/UIWindows`, mirroring window modules below `Scripts/Source`.
Each window module returns `Ui.DefineWindow(ViewClass, Controller, nativeBase?)`;
its ordinary mirrored stub declares only the private Controller and business
types, with bare `---@meta` and no return. The generated declaration uses the
window's real module name and derives `new`, `FromView`, methods and constants
from that Controller contract. An empty Controller needs no handwritten stub.
Handwritten foundations live in `Internal.UIBase`; independent row and Scene
Controllers use `Ui.Define(ViewClass, definition, base?)`.

Generation reads asset structure, canonical `controlId` values and window
declarations without executing Lua or requiring a Preview Host or registry.
Declare window modules with direct `require` imports and one final
`return Ui.DefineWindow(...)`; the optional native base defaults to
`Engine.Canvas`. `--check` reports missing, stale or obsolete output and exits
nonzero without writing. All inputs and output conflicts are checked before
updating either Views or window declarations.

`ScriptTools ui-assets manifest <project-root>` is a read-only description used
by the editor's Export record. It emits JSON with project-relative input paths
and their SHA-256 hashes, plus the expected and existing generated output paths.
It includes window modules and their handwritten Controller declarations,
including missing declarations. It neither runs plug-in hooks nor records a
successful project Export.

The editor's **Export** generates these files after saving project data.
Ordinary saves, native builds and direct run or pack commands do not generate
UI Lua. Export also invokes project export hooks and records the inputs and
outputs only after they succeed. Editor Play checks whether export is current;
editor Pack runs Export before invoking its pack hooks and platform tools.
Generation errors stop Export and any Play or Pack that depends on it.

Low-level `build_cpp`, `build_standalone`, `run_cpp` and desktop/mobile pack
entry points consume existing exports and retain their existing UI validation.
For command-line workflows, export in the editor first, or deliberately run
`ui-assets generate` and prepare locale catalogues before running or packaging.
Mobile `--check` performs preflight only. The standalone UI generator does not
run editor plug-ins or publish the editor's project export record.

Unchanged generated files retain their contents and timestamps; only obsolete
files bearing the generation marker are removed from the generated trees.
Handwritten file conflicts stop generation. `create_templates` excludes
`Scripts/Internal/UI`, `Scripts/stub/Internal/UI` and `Scripts/stub/Internal/UIWindows`,
even when these folders exist in Game or a native cache is reused. Templates
retain handwritten declarations and native binding stubs; game packaging
removes all of `Scripts/stub` before optional Lua compilation.

Desktop `build_cpp` builds the project preview before full UI validation.
`build_ui_preview_host <project-folder> <Debug|Release>` builds that target
without launching the game. Both repository and installed-editor tools support
this project-based workflow. A source project without preview artifacts can
still save/build after descriptor-independent structural checks:

```text
ScriptTools ui-assets validate <project-root> --structure-only
```

After compilation, use these read-only checks:

```text
ScriptTools ui-preview validate <project-root>
ScriptTools ui-adapter-check <project-root>
```

`ScriptTools ui-assets validate <project-root>` performs full asset validation.
For Standalone, it first restores missing preview JSON when necessary, as do
`ui-preview ensure <project-root>` and `ui-preview registry <project-root>`.
Restoration reads the existing `Binaries/UiPreviewHost`; damaged or mismatched
metadata and missing binaries remain errors. Source projects require a build
when their selected preview metadata is unavailable.

The build runs `ScriptTools ui-preview publish <project-root> <bin-directory>
--configuration <Debug|Release>` after linking and dependency preparation. The
Host's `--build-info` embeds its exact dependency filenames, platform, architecture
and configuration; `--describe` exports the native UI descriptors. Publication
reads both outputs and writes only `EditorCache/UiPreview.json` and
`EditorCache/UiPreview.registry.json`, without copying the source runtime to `Binaries`.
Manifest v3 records a safe project-relative `runtimeDirectory`, binary filenames,
configuration and registry identity. The build ID hashes the declared binaries
and the exact UTF-8 registry bytes, including the final LF, under the fixed
`UiPreview.registry.json` name. Unrelated game files are preserved and identical
content does not rewrite the JSON.

Before source compilation, `UiPreviewPrepare` calls `ui-preview begin-build`, stops
existing project preview connections and creates `EditorCache/UiPreview.building`.
Compilation, linking, description or publication failure leaves preview unavailable;
the next successful publication clears the marker. Full UI asset validation runs
after publication, so asset errors fail the build while preserving the new preview
for editing. The building marker also blocks metadata recovery.

`ui-preview copy <source-project> <target-project>` installs the matched binaries
in the target's `Binaries` and both JSON files in its `EditorCache`, rewriting
`runtimeDirectory` to `Binaries`. It retains identical shared game libraries and
unrelated target EditorCache content, and rejects mismatched dependency versions.
Template native-cache transfers use
`ui-preview copy --runtime-directory bin/<configuration> <source> <target>` after
copying the game build outputs, reusing that bin directory without an extra
`Binaries` copy. `ui-preview registry <project-root>` prints the ensured registry
path. Staging validation receives
`--registry <source-project>/EditorCache/UiPreview.registry.json` explicitly rather than
looking for development data in the final game package.

Validate convention-based C++ and Lua host-to-implementation boundaries, including the Standard ClassRuntime layer order, with:

```sh
.tools/ScriptTools/ScriptTools impl-boundary-check Game
```

```bat
.tools\ScriptTools\ScriptTools.exe impl-boundary-check Game
```

The command discovers C++ host/same-name-directory pairs from the source tree. For Lua it discovers the equivalent host/module directory pairs and excludes child modules that have their own mirrored `.d.lua` contract. It reports source locations for reverse dependencies, host member definitions in implementation folders, and Lua partial-class or mixin reuse. CMake exposes the same check through the `ImplBoundaryValidate` target.

Low-level build and pack scripts do not export `Data/Locale/Locale.xlsx`.
Official Locale Tools exports through the editor's project export hook. Its
pack hook excludes the workbook from game packages. Use **Export** or **Pack**
in the editor, or provide an equivalent deliberate export step when automating
outside it. Template creation keeps the workbook and handwritten locale Core
while excluding generated language catalogues.

`pack_harmony.sh` produces an arm64-v8a HAP for HarmonyOS 6.0.2 / API 22 or newer. It requires Apple Silicon macOS, a C++ Source project, and DevEco Studio with the OpenHarmony native SDK. Its form/backend matrix is fixed: Mobile uses OpenGL ES, while 2in1 uses OpenGL by default and can instead use OpenGL ES. The editor passes both choices explicitly; direct commands use `--device-form mobile|2in1` and `--graphics-api opengl|opengl-es`. Omitting the graphics option selects OpenGL ES for Mobile and OpenGL for 2in1; explicitly selecting OpenGL for Mobile is rejected.

```sh
./tools/pack_harmony.sh --device-form mobile --graphics-api opengl-es Game
./tools/pack_harmony.sh --device-form 2in1 --graphics-api opengl Game
./tools/pack_harmony.sh --device-form 2in1 --graphics-api opengl-es Game
```

The three unsigned outputs are `dist/<game>-<full-version>-harmony-mobile-unsigned.hap`, `dist/<game>-<full-version>-harmony-2in1-opengl-unsigned.hap` and `dist/<game>-<full-version>-harmony-2in1-opengl-es-unsigned.hap`. Add `--export-to-device` to build the corresponding `-signed.hap`, install it and launch it. Mobile export accepts a connected target whose reported device type is `default`, `phone` or `tablet`; 2in1 export accepts only `2in1`. Exactly one connected device must match the requested form, while devices of the other form may remain connected. `--check` validates the same selected form/backend and, when combined with `--export-to-device`, the matching-device requirement without building or publishing a HAP.

Every variant sets the HAP target and compatible SDK to `6.0.2(22)` and passes `OHOS_COMPATIBLE_SDK_VERSION=22` to the native build, producing the versioned compiler target `aarch64-linux-ohos22.0.0`. The Mobile CMake contract is `SFML_HARMONY_DEVICE_FORM=MOBILE` with `SFML_OPENGL_ES=ON`; the two 2in1 contracts use `SFML_HARMONY_DEVICE_FORM=2IN1` with `SFML_OPENGL_ES=OFF` for OpenGL or `ON` for OpenGL ES. FFmpeg-enabled builds use that same versioned target for compilation and linking.

The 2in1 OpenGL HAP requires the target image to provide HarmonyOS desktop OpenGL through `libGLv4.so` and the platform capability query. Some API 24 PC emulator images omit that runtime even though the compile SDK contains its import library. Such an image cannot load the OpenGL native module; the app reports the missing runtime and never silently falls back to OpenGL ES. Export the separate OpenGL ES variant for that image, or use a 2in1 device/image that provides desktop OpenGL to validate the OpenGL variant.

`pack_android.sh` produces an arm64-v8a Release APK for Android 7.0 / API 24 or newer. It requires Apple Silicon macOS, Android Studio at one of its two standard application locations, SDK Platform 36, Build Tools 36.0.0, a complete stable NDK r27 or newer under the locally installed SDK, system CMake 3.28 or newer with Unix Makefiles support, and `/usr/bin/make`. The SDK is resolved from `ANDROID_SDK_ROOT`, then `ANDROID_HOME`, then `~/Library/Android/sdk`. The packer selects the highest complete stable NDK under that SDK's `ndk` directory; projects and editor packages never carry an SDK or NDK. Set `LUDORK_CMAKE` only when selecting a particular system CMake executable. The tool does not use an SDK-bundled CMake, Ninja, SDK Manager, an emulator, AVD or adb. It runs `ScriptTools android-pack`, packages the prebuilt `libludork.so` with Gradle and, by default, writes `dist/<game>-<full-version>-android-arm64-v8a-unsigned.apk` without installing or launching it.

The Gradle wrapper lives in `Game/Engine/PlatformHosts/Android` alongside the Android host template and is included in both C++ Source template variants. Packaging copies and validates that template without reading third-party examples. `gradle/wrapper/gradle-wrapper.properties` selects Gradle 9.5.0; Android Gradle Plugin remains 9.3.0. The wrapper’s licence and source notice travel with it under `gradle/wrapper`. macOS packages retain `gradlew`, the wrapper JAR, configuration and notices; they omit the Windows-only `gradlew.bat`.

Optional signing uses `--sign --keystore <absolute-path> --key-alias <alias>`. Supply exactly two UTF-8, newline-delimited passwords on standard input, using the same value twice when they match; never place them in command-line arguments. With `--check`, the same protocol validates the environment and credentials without publishing. A successful run signs and verifies the APK, then publishes only `dist/<game>-<full-version>-android-arm64-v8a-signed.apk`; the command does not persist credentials. Reuse the same signing key for later application updates. A signed package is not installed or launched.

### macOS signing and notarisation

macOS packaging always signs. Without any signing information it applies ad-hoc
signatures exactly as before; with a signing identity it signs every Mach-O file
in the bundle from the inside out, then seals the bundle. `pack_project.sh`
exposes the same options through `ScriptTools macos-sign`, which `pack_editor.sh`
also uses for the editor application and disk image. Files in a bundle's
`Contents/MacOS`, including .NET assemblies and configuration files, are signed
before the bundle; its main executable receives its signature and entitlements
with the bundle. Non-Mach-O signatures use extended attributes, which must survive
distribution. Editor packaging verifies the complete app before and after DMG
creation, including the app mounted from the final disk image.

The editor DMG packer detaches the whole device associated with its image file
and confirms that the image is no longer attached before conversion or cleanup.
An unmounted volume alone is not sufficient. Detach failures receive bounded
forced retries; an unresolved attachment preserves the image and work directory.

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

The environment variable wins over the command-line option when both are set, so
a machine can carry local signing while CI passes the same values from secrets.
`--notarize` is opt-in per run and has no environment variable. Add
`--ignore-environment` to use only the command-line options.

A `--certificate` is imported into a temporary keychain that is removed afterwards;
its password is the first password read from standard input. `--signing-identity`
selects one identity when the certificate or keychain holds several, and accepts a
SHA-1 fingerprint. A real identity adds the hardened runtime and a secure timestamp
to all nested code, including helper tools and template executables under
`Contents/Resources`; their location does not exempt them from notarisation
requirements. `--entitlements` applies to the
application's main executable. Apple does not offer a non-interactive alternative
to passing the `.p12` password on the `security import` command line, so that one
process argument is unavoidable; every other step keeps passwords on standard input.

When `codesign` reports that the timestamp service is unavailable, real-identity
signing retries only the failed target after 5, 15 and 30 seconds, for at most four
attempts. Every attempt retains the timestamp and signing options. Other signing
errors and verification failures stop immediately; exhausted retries retain the
last diagnostic and signing failure status.

`--notarize` submits the disk image directly, or a temporary ZIP of an application
bundle, to `notarytool`, waits for the result and staples the ticket to the
artifact. Pass either the Apple ID and its team plus the app-specific password as
the second standard-input password, or the App Store Connect key file with its key
ID and issuer ID, which needs no password. Notarisation requires a real signing
identity; an ad-hoc identity is rejected. Without notarisation credentials the
packaging signs only. With `--check`, the packaging validates the environment and
the signing material, including a notarisation round trip, without building.
If submission fails or is rejected, the signer retrieves the Apple notary log
when a submission ID is available, prints its issues in the build log, and keeps
the original failure status and ID even if that diagnostic download fails.

To inspect CI submissions without rebuilding, manually run
[Query macOS Notarization](../.github/workflows/macos-notary-status.yml).
Leave `submission_id` empty to list recent submissions, then run it again with
one submission UUID to print its status and, once finished, its notary log.
The workflow reuses the existing `MACOS_NOTARY_*` secrets (Apple ID or API key),
needs no signing certificate, and does not submit, cancel or wait for notarisation.
Results appear in the query step's Actions log; workflow success means the query
succeeded, not that the selected package passed notarisation.

### iOS signing

Without a certificate, iOS packaging keeps using the signed-in Xcode account and
automatic signing, with the team taken from `LUDORK_IOS_DEVELOPMENT_TEAM` or
`--team-id`. Supplying `--certificate` and `--provisioning-profile` switches to
manual signing: the certificate is imported into a temporary keychain, the profile
is installed for the build and removed again when it was not already present, and
Xcode builds with `CODE_SIGN_STYLE=Manual`, the imported identity and the profile
name. The certificate password is read from standard input. The packer rejects a
profile whose team or application identifier does not match the signing team and
the derived bundle identifier.

| Command line | Environment variable |
|---|---|
| `--team-id TEAMID` | `LUDORK_IOS_DEVELOPMENT_TEAM` |
| `--certificate PATH.p12` | `LUDORK_IOS_SIGNING_CERTIFICATE` |
| `--provisioning-profile PATH.mobileprovision` | `LUDORK_IOS_PROVISIONING_PROFILE` |
| `--signing-identity NAME` | `LUDORK_IOS_SIGNING_IDENTITY` |

The environment variable wins over the command-line option. Add
`--ignore-environment` to use only the command-line options. Manual signing
requires a team ID and both the certificate and the profile; `--check` validates
them without building.

All game packers derive application identifiers and the `<game>` filename
component from the static `APP_NAME` in `Scripts/Entry.lua`. Set one top-level
`local APP_NAME` to a unique non-empty string literal before packaging; the
sample `LudorkSample`, missing definitions and dynamic expressions are rejected.
Mobile and macOS display names use the non-empty, control-character-free raw
`title.value` in `Data/Configs/System.json`. The project folder name is not used.
Changing the title or version preserves installation identity; changing `APP_NAME`
can require updated signing or provisioning.

Game packers accept `--version <base-version>` and `--release`; C++ Source projects
also accept `--dev`. Unspecified values come from `Main.proj` fields
`packaging.version` and, for C++ Source projects, `packaging.dev`, whose defaults
are `"1.0.0"` and `false`. Standalone projects always use release, ignore an older
`packaging.dev=true`, and reject an explicit `--dev`. CLI overrides never write
back to the project. Base versions contain three non-negative decimal components
without leading zeroes, prefixes or suffixes, up to 127 bytes. The editor's Pack
Options shows dev only for C++ Source projects and saves valid choices on
confirmation; confirming a Standalone package clears an older dev value. Cancelling
writes nothing. These settings do not belong in Entry.

```sh
./tools/pack_project.sh --version 1.0.0 --dev Game Game/dist
./tools/pack_android.sh --version 1.0.0 --release Game
./tools/pack_harmony.sh --version 1.0.0 --dev --device-form mobile Game
```

The full release identifier is the base version for formal packages, or
`<base-version>.YYYYMMDDHH` for dev packages, using one UTC+8 timestamp captured
at pack start. For example, `1.0.0` at `2026-01-01T07:00:00+08:00` produces
`1.0.0.2026010107` in dev mode. The full rules, platform version/build-number
mapping and `Data/BuildInfo.json` fields are documented in
[Release version and internal packages](<../docs/en_GB/02.Editor User Guide/08.Run Debug and Package.md#release-version-and-internal-packages>).
The dev flag does not change Release optimisation, signing or encryption.

`<full-version>` in the output paths is this full release identifier.
`pack_project` writes `dist/<game>-<full-version>/Main.exe` on Windows and
`dist/<game>-<full-version>.app` on macOS; iOS writes
`dist/<game>-<full-version>.ipa`. A custom output directory replaces `dist` as
the parent. Repacking the same identifier replaces only its artifact, preserving
other versions and unrelated files. A repeated dev build in the same hour has
the same identifier and platform build number.

With `--compile-lua`, every packaged `Scripts/**/*.lua` file is compiled with
`luac -s`, renamed to `.luac`, and written to `dist`.
With `--encrypt-saves`, a C++ Source package rebuilds Standard with
`LUDORK_SAVE_AS_LDC=ON`, causing the runtime to inject `SAVE_AS_LDC = true`
before Entry runs. This option is rejected for prebuilt Standalone projects.
Standalone projects can instead assign the global `SAVE_AS_LDC = true` at the
top of `Scripts/Entry.lua`, before all `require` calls; `Source.Save` caches it
when first loaded. Set it to `false` to select plain JSON saves.
With `--use-ldpak`, the complete `Assets`, `Data` and pruned `Scripts` trees in
staging become root `Assets.ldpak`, `Data.ldpak` and `Scripts.ldpak`. Entries
retain paths relative to their source directory. Use
`ScriptTools validate-ldpak-source <runtime-root>` for the common source preflight.
Encryption runs before archiving, and the source project remains loose and unchanged.
Packaging excludes the project-root `EditorCache` and `Cache` directories, with or
without `--use-ldpak`. Nested directories with those names in runtime
content remain included. Runtime rejects loose/archive conflicts and old per-group
archives rather than merging them.

Animation caches always use `Cache/Animations` beneath the application user-data
root, including when Data is loose. Initialisation regenerates missing caches;
project-root `Cache` is also excluded when generating templates.

Desktop Standalone output keeps its launcher at the root and native dependencies
under `Binaries`; a packaged macOS app uses its standard `Contents` layout.

Release native builds enable IPO/LTO for Standard, its ClassRuntime OBJECT target
and the final mobile application, alongside Runtime and Core; MinGW keeps IPO
disabled. ClassRuntime hides ordinary and inline symbols while preserving explicit
public exports. On Apple platforms, the game application, Standard, Runtime and
Core use `-dead_strip` for their final Release links. Debug optimisation and
symbol-stripping policies remain unchanged.

On macOS, single-arm64 AppleClang Release builds compile generated Core binding
sources with `-Os`. Handwritten runtime sources and other platforms, architectures
and configurations retain their existing optimisation levels.

Final native symbol handling is platform-specific:

| Platform | Distribution output |
|---|---|
| macOS | C++ Source and Standalone packing apply `strip -x` to Main and real dynamic libraries in the final app, skipping symlinks. Binaries receive ad-hoc signatures after dependency-path changes; the complete app is signed and verified after resource finalisation with the configured identity, or ad hoc when none is configured. Strip or signing failure aborts packing. |
| iOS | The Release application target uses `DEPLOYMENT_POSTPROCESSING=YES`, `STRIP_INSTALLED_PRODUCT=YES` and `STRIP_STYLE=non-global`, so Xcode strips before signing; input static libraries remain intact. |
| Windows | MSVC generates separate PDB files with `/DEBUG:FULL` and keeps `/OPT:REF /OPT:ICF`. Packages exclude PDB files; EXE/DLL files are not passed through a generic strip tool. |
| Android | NDK `llvm-strip --strip-unneeded` processes the staged `libludork.so` before Gradle builds the APK. |
| HarmonyOS | Hvigor strips native libraries for Mobile and both 2in1 Release variants before HAP signing; signed HAP files are not modified afterwards. |

Keep matching unstripped native outputs and any generated debug-symbol files for
release diagnostics. macOS packing leaves source `bin` outputs, Standalone
templates and shared preview libraries unstripped. Ad-hoc signatures provide
neither distributor certificate signing nor notarisation; configure a Developer ID
identity and notarisation credentials for a distributable package. Android and
HarmonyOS retain their toolchain section garbage collection without extra global flags.

`LUDORK_WITH_LUA` defaults to `ON` for game projects. The desktop project
preview target leaves the setting unchanged and reuses the project's Runtime,
Standard and SFML dependencies. Its dedicated `UiPreviewHostRuntime` compiles
the project's native UI sources with preview resource loading; the Host neither
links the full Engine target nor starts gameplay or Lua Controllers.

Source preview and game linker outputs share `bin/Debug` or `bin/Release`.
The editor launches the Host directly from the directory selected by the manifest,
using the JSON files in project `EditorCache`. Ordinary source builds publish metadata
only; Standalone generation copies the matched runtime to `Binaries`. C++ UI
changes take effect after recompilation. A source build stops the project's old
preview connections before compiling and refreshes UI and Actor previews after
successful publication. Each project owns its registry and processes.

On macOS the Host and its dylib dependencies retain `@loader_path` and relative
SONAME links in the same directory. Dependency closure and signatures are checked
before publication without rewriting shared libraries. Preview files do not use
the game Main's root-launcher or app-Frameworks relocation rules.
Mobile packaging first prepares the local desktop preview registry and passes
it explicitly to resource validation and cross-CMake; it never executes a
mobile Host or includes desktop preview artifacts in the device package.
A desktop Preview build in the mobile packaging log is this registry preparation
step; Lua View generation does not replace it.
Control descriptors must be platform-independent; platform differences belong
in their native implementations.

`Templates/Cpp` carries preview source and requires a first build.
`Templates/Standalone` carries the corresponding prebuilt preview snapshot,
including for the FFmpeg variant. Template native caches include the current
snapshot in `bin/<configuration>` and exactly `EditorCache/UiPreview.json` plus
`EditorCache/UiPreview.registry.json`. Template generation excludes other project EditorCache
content; C++ templates remove both compiled binaries and JSON before distribution.
Native caches use the same layout and must be republished by `ui-preview publish`
before reuse when their metadata layout is outdated; no old-path reads are used.
There is no editor-global Host distribution or compatibility fallback. Existing
source projects need updated project build files and a rebuild; Standalone
projects must be regenerated.

`Game` carries the Ludork licence and game-runtime legal materials, including
native dependencies, optional FFmpeg and bundled assets. Template generation
refreshes those materials in C++ templates and derives Standalone templates
from them. Editor, managed-runtime and build-tool notices remain in the editor
distribution. Final game packages remove only preview-specific files from
`Binaries`, retain shared libraries, and exclude root `EditorCache` and `Cache`.

LuaSF source archives contain two sibling CMake projects, `LuaSF/` and `LuaGlue/`. For local dependency and template checks, set `LUASF_SOURCE_ARCHIVE` to the generated `.tar.gz` (`.zip` on Windows) before `init_cpp_dependencies`. For an existing build, configure `LUDORK_LUASF_SOURCE_DIR` with the generator's `output/LuaSF` directory; LuaGlue is found next to it. Desktop native packages include the shared LuaGlue runtime; mobile builds use its static target.

Desktop Core builds use `Game/Engine/Tools/NativeStubDump` to load each module's exported stub writer after linking and from the aggregate native build. The tool does not create a Lua VM and publishes the compiler-verified copy methods into the existing native `.d.lua` files. Its sources travel with the Engine tree in project templates.
