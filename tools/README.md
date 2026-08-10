# Game tools

All scripts switch to the repository root before doing work. Use `.bat` on Windows 10 or newer on x64 and matching `.sh` scripts on Apple Silicon macOS 13.3 or newer. macOS tools find CMake from `PATH` or `/Applications/CMake.app`.

| Tool | Purpose |
|---|---|
| `init` | Prepare the generator environment and native dependencies; the standard root-Sample init also builds the Release `UiPreviewHost` |
| `setup_python` | Create `.venv` and install build-time Python requirements |
| `build_script_tools` | Build the repository-owned ScriptTools executable under `.tools` |
| `build_ui_preview_host` | Build the repository-owned native preview tool under `.tools` |
| `init_cpp_dependencies` | Download dependencies for a C++ project folder |
| `run_editor` | Start the editor from the repository root |
| `build_cpp` | Configure/build a C++ project; also regenerates Core bindings, stubs and metadata |
| `run_cpp` | Run a built native project with its source folder as working directory |
| `build_standalone` | Build and copy a flat runtime plus Assets, Data and Scripts |
| `run_standalone` | Run a flat standalone project |
| `create_templates` | Recreate Cpp and Standalone template variants |
| `pack_project` | Produce the platform distribution layout |
| `pack_editor.bat` | Publish and validate the self-contained Windows 10-or-newer x64 editor package with official plug-ins |
| `pack_editor.sh` | Publish and validate the self-contained macOS Apple Silicon editor DMG |

Typical commands:

```sh
./tools/init.sh
./tools/run_editor.sh
./tools/build_cpp.sh Sample Debug
./tools/run_cpp.sh Sample Debug
./tools/pack_project.sh Sample Sample/dist
./tools/pack_editor.sh
```

```bat
tools\init.bat
tools\run_editor.bat
tools\build_cpp.bat Sample Debug
tools\run_cpp.bat Sample Debug
tools\pack_project.bat Sample
tools\pack_editor.bat
```

Both editor packaging scripts use the shared ScriptTools command
`editor-official-plugins prepare <source> <output-root>` to clean-copy the fixed
official plug-ins and generate their registry. The matching
`editor-official-plugins validate <output-root>` command verifies the exact
directory set, manifests, generated index and excluded build or user-state
artifacts.

`pack_editor.bat` places `OfficialBlueprintAI`, `OfficialLocaleTools`, and
`OfficialRandomMap` below the packaged editor's `Plugins` directory and
generates `plugins.json` beside that directory from their manifests. A
published Windows editor uses its program directory as the plug-in root, so
plug-in writable data is stored below `Plugins/.data` there as well.

`pack_editor.sh` requires macOS on Apple Silicon with a logged-in Finder session,
the .NET 9 SDK, CMake, ScriptTools built by `init.sh`, and initialized Sample
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
by the standalone ScriptTools executable. Python is only needed by `init` to
build ScriptTools with Nuitka and is not required by an installed editor.

Low-level build and pack scripts do not export `Data/Locale/Locale.xlsx`. The Official Locale Tools editor plug-in performs export through before-run and before-pack hooks. Run or pack from the editor, or provide an equivalent deliberate export step when automating outside it.

`pack_harmony.sh` produces an arm64-v8a mobile HAP for HarmonyOS 6.0.1 / API 21 or newer. It requires Apple Silicon macOS, a C++ Source project, and DevEco Studio with the OpenHarmony native SDK.

`pack_project` refuses a project whose `Scripts/Entry.lua` still uses the Sample `APP_NAME = "LudorkSample"`; set a unique application name first.
With `--compile-lua`, every packaged `Scripts/**/*.lua` file is compiled with
`luac -s`, renamed to `.luac`, and written to `dist`.

`Templates/Cpp` is the reusable source template; `Templates/Standalone` is the flat packaged-runtime target. FFmpeg variants carry the video-capable runtime and its separate licensing material. The standard `init` command without a custom C++ project builds the editor-owned Release `UiPreviewHost` into `.tools/UiPreviewHost`; `build_ui_preview_host` remains available for explicit Debug builds or refreshes, and Debug is preferred by the development editor when both configurations exist. The Host is distributed once under the editor's `tools/UiPreviewHost`; it is not part of any project template or game package. macOS packaging does not sign or notarise the result.
