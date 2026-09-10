# Game tools

All scripts switch to the repository root before doing work. Use `.bat` on Windows 10 or newer on x64 and matching `.sh` scripts on Apple Silicon macOS 13.3 or newer. macOS tools find CMake from `PATH` or `/Applications/CMake.app`.

| Tool | Purpose |
|---|---|
| `init` | Prepare the generator environment and native dependencies |
| `setup_python` | Create `.venv` and install build-time Python requirements |
| `build_script_tools` | Build the repository-owned ScriptTools executable under `.tools` |
| `build_ui_preview_host` | Build and publish a project's native preview snapshot |
| `init_cpp_dependencies` | Download dependencies for a C++ project folder; per-package scripts live under `tools/cpp_dependencies` |
| `run_editor` | Start the editor from the repository root |
| `build_cpp` | Configure/build a C++ project, its preview and registry; regenerate Core bindings, stubs and metadata |
| `run_cpp` | Run a built native project with its source folder as working directory |
| `build_standalone` | Build a desktop runtime with a root launcher/host and native code under `Binaries` |
| `run_standalone` | Run a desktop Standalone project through its root launcher/host |
| `create_templates` | Recreate Cpp and Standalone template variants |
| `create_templates_plain` | Recreate only the non-FFmpeg Cpp and Standalone templates |
| `create_templates_ffmpeg` | Recreate only the FFmpeg-enabled Cpp and Standalone templates |
| `pack_project` | Produce the platform distribution layout |
| `pack_harmony.sh` | Build a HarmonyOS API 22 Mobile or 2in1 arm64-v8a HAP, with optional device export |
| `pack_android.sh` | Build an Android arm64-v8a Release APK from a C++ Source project, unsigned by default with optional signing |
| `pack_editor.bat` | Publish and validate the self-contained Windows 10-or-newer x64 editor package with official plug-ins |
| `pack_editor.sh` | Publish and validate the self-contained macOS Apple Silicon editor DMG |

Typical commands:

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

The editor **Construct** button runs `build_cpp` for C++ Source projects.
**Play** uses the same Debug build record: it stays unavailable until the
first successful Debug build, then asks to build and play when the record
is not current. The editor workflow is in
[Running, Testing and Packaging](<../docs/en_GB/02.Editor User Guide/08.Run Debug and Package.md>).

Both repository and installed-editor `build_cpp` scripts record the latest
successful native build with `ScriptTools native-build-state`, in
`build/NativeBuild-<configuration>.json`. Starting another build preserves that
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
Keep this folder outside both Game and the template output. The caller must
invalidate it when native sources, bindings, dependencies or build options
change. `build_standalone --use-current-build` packages the matching existing
`bin/<configuration>` output and Windows `build/launcher/<configuration>`.

`pack_editor --use-current-editor-build` restores NuGet dependencies and runs
`dotnet publish --no-build` against matching Release outputs for the editor and
its two contract projects. It still refreshes Content and compiles current locale
data in a clean staging directory. Windows also accepts `--launcher <file>` for
a prepared editor launcher outside that staging directory.

The [Export Editor workflow](../.github/workflows/export-editor.yml) uses separate
exact caches for the prepared environment, native components, C# build outputs
and Windows editor launcher. Keys include tracked input paths and Git object
IDs, platform, architecture and configuration; the C# key also includes the .NET
SDK version. Source additions, deletions and renames invalidate the affected key.
Only successful results are saved, with no prefix-key fallback. A missing or
evicted cache rebuilds that component. Workflow or cache-rule changes invalidate
all groups. Delete the relevant Actions cache to force a rebuild with unchanged
sources. Every run packages the current Game project, Lua, plug-ins, locale and docs.

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
├── Binaries/
│   ├── Ludork.exe             # Actual editor
│   ├── Ludork.dll
│   ├── Ludork.deps.json
│   ├── Ludork.runtimeconfig.json
│   └── …                      # DLLs and .NET runtime files
├── Locale/
├── Templates/
├── tools/
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

`OfficialBlueprintAI`, `OfficialLocaleTools`, and `OfficialRandomMap` are placed
below the root `Plugins` directory, with `plugins.json` generated beside it from
their manifests. The published editor resolves its resources, plug-ins and
configuration against the installation root even when started directly from
`Binaries`. Runtime settings use root `Ludork.ini`; writable plug-in data uses
`Plugins/.data`. Repository development builds use `Plugins` and
`plugins.json` with the development marker. Desktop game Standalone packages retain their
own packaging layout.

`pack_editor.sh` requires macOS on Apple Silicon with a logged-in Finder session,
the .NET 9 SDK, CMake, ScriptTools built by `init.sh`, and initialized Game project
dependencies. It produces `dist/Ludork-<version>-macos-arm64.dmg` for macOS 13.3
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
by the standalone ScriptTools executable. Python is needed only when `init` or
`build_script_tools` builds ScriptTools with Nuitka. Development build and
packaging commands consume the initialized executable; rerun `init` or
`build_script_tools` explicitly after changing ScriptTools. Python is not
required by an installed editor.

`dotnet build` and `dotnet publish` generate `obj/.../EngineConstants.g.cs` with `ScriptTools engine-constants <EngineState.hpp> <output.cs>`. The C++ declaration is authoritative for the editor cell size; rebuild ScriptTools after changing the generator. The managed Actions cache includes this header and generator so changed constants cannot reuse stale editor binaries.

System UI descriptors are owned by each project's
`Engine/Source/Core/include/UI/UiControlAdapterDescriptors.hpp`. The compiled
project Host exports them with `UiPreviewHost --describe`. C# and global
ScriptTools load the resulting project JSON; neither embeds a generated control
table, and adding controls with supported property types does not require
rebuilding those tools.

`ScriptTools ui-assets generate <project-root>` generates all Lua Views under
`Scripts/Source/UI` and their typed declarations under `Scripts/stub/Source/UI`
from `Data/UI/Assets`. It also generates public window declarations under
`Scripts/stub/Source/UIWindows`, mirroring window modules below `Scripts/Source`.
Each window module returns `Ui.DefineWindow(ViewClass, Controller, nativeBase?)`;
its ordinary mirrored stub declares only the private Controller and business
types, with bare `---@meta` and no return. The generated declaration uses the
window's real module name and derives `new`, `FromView`, methods and constants
from that Controller contract. An empty Controller needs no handwritten stub.
Handwritten foundations live in `Source.UIBase`; independent row and Scene
Controllers use `Ui.Define(ViewClass, definition, base?)`.

Generation reads asset structure, canonical `controlId` values and window
declarations without executing Lua or requiring a Preview Host or registry.
Declare window modules with direct `require` imports and one final
`return Ui.DefineWindow(...)`; the optional native base defaults to
`Engine.Canvas`. `--check` reports missing, stale or obsolete output and exits
nonzero without writing. All inputs and output conflicts are checked before
updating either Views or window declarations.

Project saves generate after writing JSON. Native builds depend on the
`UiAssetGenerate` target, so repository and installed-editor `build_cpp` tools
and direct CMake builds synchronise Views. `run_cpp` generates before launching
an existing binary; editor Standalone Play generates after before-run hooks.
Desktop and mobile pack entry points generate before copying runtime resources.
Mobile `--check` only performs preflight and does not generate. Generation errors
stop saving, building, running or packaging.

`build_standalone --use-current-build` also regenerates before validation and
copying. Template generation therefore uses current JSON even when
`create_templates --native-cache` reuses native binaries. Unchanged generated
files retain their contents and timestamps; only obsolete files bearing the
generation marker are removed from the generated trees. Handwritten file
conflicts stop generation. Templates retain the generated stub tree, while game
packaging removes it before optional Lua compilation.

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
reads both outputs and writes only `Temp/UiPreview.json` and
`Temp/UiPreview.registry.json`, without copying the source runtime to `Binaries`.
Manifest v3 records a safe project-relative `runtimeDirectory`, binary filenames,
configuration and registry identity. The build ID hashes the declared binaries
and the exact UTF-8 registry bytes, including the final LF, under the fixed
`UiPreview.registry.json` name. Unrelated game files are preserved and identical
content does not rewrite the JSON.

Before source compilation, `UiPreviewPrepare` calls `ui-preview begin-build`, stops
existing project preview connections and creates `Temp/UiPreview.building`.
Compilation, linking, description or publication failure leaves preview unavailable;
the next successful publication clears the marker. Full UI asset validation runs
after publication, so asset errors fail the build while preserving the new preview
for editing. The building marker also blocks metadata recovery.

`ui-preview copy <source-project> <target-project>` installs the matched binaries
in the target's `Binaries` and both JSON files in its `Temp`, rewriting
`runtimeDirectory` to `Binaries`. It retains identical shared game libraries and
unrelated target Temp content, and rejects mismatched dependency versions.
Template native-cache transfers use
`ui-preview copy --runtime-directory bin/<configuration> <source> <target>` after
copying the game build outputs, reusing that bin directory without an extra
`Binaries` copy. `ui-preview registry <project-root>` prints the ensured registry
path. Staging validation receives
`--registry <source-project>/Temp/UiPreview.registry.json` explicitly rather than
looking for development data in the final game package.

Validate convention-based C++ and Lua host-to-implementation boundaries, including the Standard ClassRuntime layer order, with:

```sh
.tools/ScriptTools/ScriptTools impl-boundary-check Game
```

```bat
.tools\ScriptTools\ScriptTools.exe impl-boundary-check Game
```

The command discovers C++ host/same-name-directory pairs from the source tree. For Lua it discovers the equivalent host/module directory pairs and excludes child modules that have their own mirrored `.d.lua` contract. It reports source locations for reverse dependencies, host member definitions in implementation folders, and Lua partial-class or mixin reuse. CMake exposes the same check through the `ImplBoundaryValidate` target.

Low-level build and pack scripts do not export `Data/Locale/Locale.xlsx`. The Official Locale Tools editor plug-in performs export through before-run and before-pack hooks. Run or pack from the editor, or provide an equivalent deliberate export step when automating outside it.

`pack_harmony.sh` produces an arm64-v8a HAP for HarmonyOS 6.0.2 / API 22 or newer. It requires Apple Silicon macOS, a C++ Source project, and DevEco Studio with the OpenHarmony native SDK. Its form/backend matrix is fixed: Mobile uses OpenGL ES, while 2in1 uses OpenGL by default and can instead use OpenGL ES. The editor passes both choices explicitly; direct commands use `--device-form mobile|2in1` and `--graphics-api opengl|opengl-es`. Omitting the graphics option selects OpenGL ES for Mobile and OpenGL for 2in1; explicitly selecting OpenGL for Mobile is rejected.

```sh
./tools/pack_harmony.sh --device-form mobile --graphics-api opengl-es Game
./tools/pack_harmony.sh --device-form 2in1 --graphics-api opengl Game
./tools/pack_harmony.sh --device-form 2in1 --graphics-api opengl-es Game
```

The three unsigned outputs are `dist/<game>-harmony-mobile-unsigned.hap`, `dist/<game>-harmony-2in1-opengl-unsigned.hap` and `dist/<game>-harmony-2in1-opengl-es-unsigned.hap`. Add `--export-to-device` to build the corresponding `-signed.hap`, install it and launch it. Mobile export accepts a connected target whose reported device type is `default`, `phone` or `tablet`; 2in1 export accepts only `2in1`. Exactly one connected device must match the requested form, while devices of the other form may remain connected. `--check` validates the same selected form/backend and, when combined with `--export-to-device`, the matching-device requirement without building or publishing a HAP.

Every variant sets the HAP target and compatible SDK to `6.0.2(22)` and passes `OHOS_COMPATIBLE_SDK_VERSION=22` to the native build, producing the versioned compiler target `aarch64-linux-ohos22.0.0`. The Mobile CMake contract is `SFML_HARMONY_DEVICE_FORM=MOBILE` with `SFML_OPENGL_ES=ON`; the two 2in1 contracts use `SFML_HARMONY_DEVICE_FORM=2IN1` with `SFML_OPENGL_ES=OFF` for OpenGL or `ON` for OpenGL ES. FFmpeg-enabled builds use that same versioned target for compilation and linking.

The 2in1 OpenGL HAP requires the target image to provide HarmonyOS desktop OpenGL through `libGLv4.so` and the platform capability query. Some API 24 PC emulator images omit that runtime even though the compile SDK contains its import library. Such an image cannot load the OpenGL native module; the app reports the missing runtime and never silently falls back to OpenGL ES. Export the separate OpenGL ES variant for that image, or use a 2in1 device/image that provides desktop OpenGL to validate the OpenGL variant.

`pack_android.sh` produces an arm64-v8a Release APK for Android 7.0 / API 24 or newer. It requires Apple Silicon macOS, Android Studio at one of its two standard application locations, SDK Platform 36, Build Tools 36.0.0, a complete stable NDK r27 or newer under the locally installed SDK, system CMake 3.28 or newer with Unix Makefiles support, and `/usr/bin/make`. The SDK is resolved from `ANDROID_SDK_ROOT`, then `ANDROID_HOME`, then `~/Library/Android/sdk`. The packer selects the highest complete stable NDK under that SDK's `ndk` directory; projects and editor packages never carry an SDK or NDK. Set `LUDORK_CMAKE` only when selecting a particular system CMake executable. The tool does not use an SDK-bundled CMake, Ninja, SDK Manager, an emulator, AVD or adb. It runs `ScriptTools android-pack`, packages the prebuilt `libludork.so` with Gradle and, by default, writes `dist/<game>-android-arm64-v8a-unsigned.apk` without installing or launching it.

Optional signing uses `--sign --keystore <absolute-path> --key-alias <alias>`. Supply exactly two UTF-8, newline-delimited passwords on standard input, using the same value twice when they match; never place them in command-line arguments. With `--check`, the same protocol validates the environment and credentials without publishing. A successful run signs and verifies the APK, then publishes only `dist/<game>-android-arm64-v8a-signed.apk`; the command does not persist credentials. Reuse the same signing key for later application updates. A signed package is not installed or launched.

`pack_project` refuses a project whose `Scripts/Entry.lua` still uses the Game project `APP_NAME = "LudorkSample"`; set a unique application name first.
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
Packaging excludes the project-root `Temp` and `Cache` directories, with or
without `--use-ldpak`. Nested directories with those names in runtime
content remain included. Runtime rejects loose/archive conflicts and old per-group
archives rather than merging them.

Animation caches always use `Cache/Animations` beneath the application user-data
root, including when Data is loose. Initialisation regenerates missing caches;
project-root `Cache` is also excluded when generating templates.

Desktop Standalone output keeps its launcher at the root and native dependencies
under `Binaries`; a packaged macOS app uses its standard `Contents` layout.

`LUDORK_WITH_LUA` defaults to `ON` for game projects. The desktop project
preview target leaves the setting unchanged and reuses the project's Runtime,
Standard and SFML dependencies. Its dedicated `UiPreviewHostRuntime` compiles
the project's native UI sources with preview resource loading; the Host neither
links the full Engine target nor starts gameplay or Lua Controllers.

Source preview and game linker outputs share `bin/Debug` or `bin/Release`.
The editor launches the Host directly from the directory selected by the manifest,
using the JSON files in project `Temp`. Ordinary source builds publish metadata
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
snapshot in `bin/<configuration>` and exactly `Temp/UiPreview.json` plus
`Temp/UiPreview.registry.json`. Template generation excludes other project Temp
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
`Binaries`, retain shared libraries, and exclude root `Temp` and `Cache`. macOS game
packaging does not provide distributor signing or notarisation.
