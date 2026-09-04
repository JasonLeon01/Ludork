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
./tools/build_cpp.sh Sample Debug
./tools/run_cpp.sh Sample Debug
./tools/pack_project.sh Sample Sample/dist
./tools/pack_harmony.sh --device-form mobile --graphics-api opengl-es Sample
./tools/pack_android.sh Sample
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

`create_templates` accepts `--variant plain` or `--variant ffmpeg` to build one
source/Standalone pair; the matching `create_templates_plain` and
`create_templates_ffmpeg` scripts are thin entry points for automation. Without
`--variant`, it rebuilds all four templates.

Windows and macOS automation may pass `--templates <folder>` to copy an already
generated set of four editor templates and `--use-current-ui-preview-host`
after preparing the matching Release preview host under `.tools/UiPreviewHost`.
The normal command without these options rebuilds both inputs before packaging.
The prepared template folder must remain outside `obj/editor-package`, which is
recreated during packaging.

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
by the standalone ScriptTools executable. Python is needed only when `init` or
`build_script_tools` builds ScriptTools with Nuitka. Development build and
packaging commands consume the initialized executable; rerun `init` or
`build_script_tools` explicitly after changing ScriptTools. Python is not
required by an installed editor.

Validate convention-based C++ and Lua host-to-implementation boundaries with:

```sh
.tools/ScriptTools/ScriptTools impl-boundary-check Sample
```

```bat
.tools\ScriptTools\ScriptTools.exe impl-boundary-check Sample
```

The command discovers C++ host/same-name-directory pairs from the source tree. For Lua it discovers the equivalent host/module directory pairs and excludes child modules that have their own mirrored `.d.lua` contract. It reports source locations for reverse dependencies, host member definitions in implementation folders, and Lua partial-class or mixin reuse. CMake exposes the same check through the `ImplBoundaryValidate` target.

Low-level build and pack scripts do not export `Data/Locale/Locale.xlsx`. The Official Locale Tools editor plug-in performs export through before-run and before-pack hooks. Run or pack from the editor, or provide an equivalent deliberate export step when automating outside it.

`pack_harmony.sh` produces an arm64-v8a HAP for HarmonyOS 6.0.2 / API 22 or newer. It requires Apple Silicon macOS, a C++ Source project, and DevEco Studio with the OpenHarmony native SDK. Its form/backend matrix is fixed: Mobile uses OpenGL ES, while 2in1 uses OpenGL by default and can instead use OpenGL ES. The editor passes both choices explicitly; direct commands use `--device-form mobile|2in1` and `--graphics-api opengl|opengl-es`. Omitting the graphics option selects OpenGL ES for Mobile and OpenGL for 2in1; explicitly selecting OpenGL for Mobile is rejected.

```sh
./tools/pack_harmony.sh --device-form mobile --graphics-api opengl-es Sample
./tools/pack_harmony.sh --device-form 2in1 --graphics-api opengl Sample
./tools/pack_harmony.sh --device-form 2in1 --graphics-api opengl-es Sample
```

The three unsigned outputs are `dist/<game>-harmony-mobile-unsigned.hap`, `dist/<game>-harmony-2in1-opengl-unsigned.hap` and `dist/<game>-harmony-2in1-opengl-es-unsigned.hap`. Add `--export-to-device` to build the corresponding `-signed.hap`, install it and launch it. Mobile export accepts a connected target whose reported device type is `default`, `phone` or `tablet`; 2in1 export accepts only `2in1`. Exactly one connected device must match the requested form, while devices of the other form may remain connected. `--check` validates the same selected form/backend and, when combined with `--export-to-device`, the matching-device requirement without building or publishing a HAP.

Every variant sets the HAP target and compatible SDK to `6.0.2(22)` and passes `OHOS_COMPATIBLE_SDK_VERSION=22` to the native build, producing the versioned compiler target `aarch64-linux-ohos22.0.0`. The Mobile CMake contract is `SFML_HARMONY_DEVICE_FORM=MOBILE` with `SFML_OPENGL_ES=ON`; the two 2in1 contracts use `SFML_HARMONY_DEVICE_FORM=2IN1` with `SFML_OPENGL_ES=OFF` for OpenGL or `ON` for OpenGL ES. FFmpeg-enabled builds use that same versioned target for compilation and linking.

The 2in1 OpenGL HAP requires the target image to provide HarmonyOS desktop OpenGL through `libGLv4.so` and the platform capability query. Some API 24 PC emulator images omit that runtime even though the compile SDK contains its import library. Such an image cannot load the OpenGL native module; the app reports the missing runtime and never silently falls back to OpenGL ES. Export the separate OpenGL ES variant for that image, or use a 2in1 device/image that provides desktop OpenGL to validate the OpenGL variant.

`pack_android.sh` produces an arm64-v8a Release APK for Android 7.0 / API 24 or newer. It requires Apple Silicon macOS, Android Studio at one of its two standard application locations, SDK Platform 36, Build Tools 36.0.0, a complete stable NDK r27 or newer under the locally installed SDK, system CMake 3.28 or newer with Unix Makefiles support, and `/usr/bin/make`. The SDK is resolved from `ANDROID_SDK_ROOT`, then `ANDROID_HOME`, then `~/Library/Android/sdk`. The packer selects the highest complete stable NDK under that SDK's `ndk` directory; projects and editor packages never carry an SDK or NDK. Set `LUDORK_CMAKE` only when selecting a particular system CMake executable. The tool does not use an SDK-bundled CMake, Ninja, SDK Manager, an emulator, AVD or adb. It runs `ScriptTools android-pack`, packages the prebuilt `libludork.so` with Gradle and, by default, writes `dist/<game>-android-arm64-v8a-unsigned.apk` without installing or launching it.

Optional signing uses `--sign --keystore <absolute-path> --key-alias <alias>`. Supply exactly two UTF-8, newline-delimited values on standard input: the keystore password followed by the key password; supply the same value twice when both passwords are identical. Never place either password in command-line arguments. With `--check`, the same arguments and standard-input protocol validate the environment and signing credentials without building or publishing an APK. The signing flow validates the keystore and alias, builds and validates the unsigned APK, signs it with Android SDK `apksigner`, verifies the result and atomically publishes only `dist/<game>-android-arm64-v8a-signed.apk`. The unsigned APK remains build-intermediate data. Keystore details and passwords are not written to the project, settings, Gradle files, logs or a system keychain. Keep the keystore and credentials secure and reuse the same signing key for every later version that must update an installed application. A signed package is still not installed or launched.

`pack_project` refuses a project whose `Scripts/Entry.lua` still uses the Sample `APP_NAME = "LudorkSample"`; set a unique application name first.
With `--compile-lua`, every packaged `Scripts/**/*.lua` file is compiled with
`luac -s`, renamed to `.luac`, and written to `dist`.
With `--pack-assets`, each first-level `Assets/<Group>` directory in the staging
copy becomes `Assets/<Group>.ldpak`; the source project remains loose and unchanged.

`Templates/Cpp` is the reusable source template; `Templates/Standalone` is the flat packaged-runtime target. `Sample` carries the Ludork licence and a game-runtime legal set containing native runtime, optional FFmpeg and bundled-asset materials. Template generation refreshes that set in both C++ templates and carries it into the derived Standalone templates; editor, managed-runtime, preview-host and build-tool notices remain only in the editor distribution. The standard `init` command without a custom C++ project builds the editor-owned Release `UiPreviewHost` into `.tools/UiPreviewHost`; `build_ui_preview_host` remains available for explicit Debug builds or refreshes, and Debug is preferred by the development editor when both configurations exist. The Host is distributed once under the editor's `tools/UiPreviewHost`; it is not part of any project template or game package. macOS packaging does not sign or notarise the result.
