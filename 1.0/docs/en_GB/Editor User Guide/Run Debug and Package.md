# Run, Debug and Package

Run a project, edit its live state and package it for a supported platform.

## Construct, Export and Play

The toolbar orders its actions as **Construct → Export → Play**. Standalone projects hide Construct. **Construct** saves the project and builds the Debug executable and preview runtime without launching the game. Native binding generation remains part of the build; UI and locale export is a separate operation.

Ordinary-map, world and thumbnail previews use this native preview runtime. C++ Source projects prepare it through Construct; Standalone projects use the matching bundled runtime. A missing or incompatible runtime is reported in the viewport. Map previewing does not start gameplay and does not require Play or an Export after each map edit.

**Export** saves the project, runs Export plug-in hooks and generates UI Views, declarations and locale files. Export again after changing the inputs or removing generated files. A failed or cancelled export does not count as a successful export.

Variables and General Data still take effect on Save; their Lua files remain under version control and are outside Export. Ordinary Save, Construct, direct run and template creation do not generate UI files. The explicit `ui-assets generate` command produces UI files only; it does not run Export hooks or publish a complete editor export record.

Play is enabled after the first successful Export and, for C++ Source, a successful Debug build through **Construct** or `tools/build_cpp`. A bare CMake build does not enable Play.

Deleting `EditorCache` requires another Construct for C++ Source and another Export before Play.

After the first successful build and export, Play offers **Build and Play** or **Export and Play** when files have changed. Cancelling or failing either operation stops launch. Lua, UI JSON, game data and localisation edits do not require a native build.

While Play is preparing or running the game, its button becomes **Stop**. Clicking Stop cancels preparation or stops the current process. Construct and Export disable repeated actions and project editing until completion. Natural exit, cancellation and failures restore editing; closing the project cancels active preparation.

These operations keep the current tab selected; open **Console** manually to view output.

**Individual Window** opens a separate game window and keeps the selected map or world visible in the editor. Project editing, including already open document windows, Undo/Redo and editing shortcuts, is disabled during preparation, compilation and export. Running keeps these restrictions unless **Live Debug** was enabled before Play; see below. The Console, performance monitoring and Help remain available. Embedded mode instead displays the Windows runtime inside the editor and locks the game viewport layout. This window choice is configurable only on Windows; other platforms always use an individual window. An unavailable Windows embedded handle fails with `EmbeddedHandleUnavailable`.

Failures are reported in the operation dialog and Console. An export failure prevents Play or Pack from continuing; correct the first validation, hook or generation error and export again.

## Server connections

For a C++ Source project, configure the local development connection through **Game → Test Server Settings**, then rebuild Debug. **Pack Project → Enable Ludork Server** sets a separate Release connection and starts blank each time; dev packages also use Release. Standalone hides both options. See [Simple Server](<../Simple Server/Deployment and Editor Settings.md>) for deployment and connection settings.

## Live Debug

Enable **Edit → Development Tools and Settings → Live Debug** before Play to edit the running map from the editor. The project preference defaults to off and cannot change during preparation or running. It requires **Individual Window**; selecting embedded mode clears and disables it. Preparation, Construct and Export keep their normal editing locks.

Once the game is running, the editor follows the player's current ordinary map or the current child of a composite world. Manual map switching is disabled. World holes, maps that are still loading, scenes without a live player and an unavailable connection are read-only. **File Explorer** can browse and locate resources; creating, renaming, moving, deleting and editing project files remain disabled.

The native viewport reflects the live Actors' appearance, transforms, materials and lights, together with the map's fog, panorama and ambient light. A world child retains world coordinates for these effects. [Viewport Options](<Tilesets Autotiles and Maps.md#viewport>), including Lit/Unlit, affect the editor image only; they do not change the running game or grant additional editing permissions.

On existing editable layers, Live Debug allows:

- The same tile brush as [project map editing](<Tilesets Autotiles and Maps.md#layers-and-painting>): paint, erase, sample and fill. Layer structure, visibility, tileset assignment, map properties and world placements stay locked.
- Selecting and deleting live Actors, including the player, runtime-spawned Actors and child Actors, and moving top-level Actors. The editor cannot add, paste or duplicate Actors.
- Reading **Actor Info** and changing ordinary variables, including `Optional[T]` when `T` is supported. Optional values accept `nil`; non-nil values still require the declared type. **Components**, the parent class, Blueprint identity and map tags stay read-only.

Actor positions, creation, destruction and inspected values follow the running game. Moving an Actor cancels its current movement route. Deletion uses normal Actor destruction and its existing lifecycle callbacks. Hidden Actors remain inspectable according to the [map preview rules](<Tilesets Autotiles and Maps.md#lights-and-actors>); hidden layers stay read-only. Live edits have no Undo/Redo and do not write project documents or mark them modified.

**World Outliner** nests child Actors under their current parents. Clicking or dragging a child on the map selects its top-level owner; dragging moves that owner and its descendants. Select a child in **World Outliner** to inspect or edit ordinary variables, or to delete it through its normal lifecycle. Direct child movement, including **Actor Info** coordinates, is disabled.

Edits belong to the live Actor or map instance. They remain while that instance exists, including a dormant world instance, and are not replayed onto rebuilt instances. Rebuilt instances use the project's definitions and the normal gameplay persistence records. Game saves continue to work and may keep gameplay state affected by live edits; this mode protects project source data, not save slots.

Stopping, natural exit or a launch failure restores the project map view and the map selected before Play, and discards the temporary snapshots. The game in an **Individual Window** keeps running while the editor has focus.

## Reload Lua while running

Save the changed Lua source files in your external editor, enter `$r` in the Console, then press `Enter` or **Send**. This updates existing functions while preserving the scene, objects and runtime state. The game must be launched from the editor and use loose Lua sources.

The Console reports the result. Changes to data or module structure may require a restart; see [Lua hot reload](<../Lua and Blueprint Scripting/Lua Runtime and Modules.md#lua-hot-reload>) for the supported changes and the failure handling.

## Packaging

Choose **File → Pack Project**, select the target and follow the packaging log. The project must have a valid entry script and a compatible template. Define exactly one top-level `local APP_NAME` in `Scripts/Entry.lua` as a non-empty static string literal unique to the game, without control characters or later reassignment. Packaging rejects the sample value `LudorkSample`, a missing definition and expressions that require executing Lua to determine the name.

If packaging reports that access to an output path was denied, close any running copy of the packaged game and File Explorer windows on the output folder, then retry. Language servers and antivirus software can also hold generated files open.

The editor requires absolute signing-material paths without NUL, carriage returns or line feeds. Signing dialogs check that the files are readable before accepting them. Loading or saving a credential also validates its keystore or certificate path. Signing aliases and passwords must contain no NUL or line breaks.

Mobile and macOS application display names use the raw `title.value` text from `Data/Configs/System.json`. The title must be non-empty and contain no control characters; packaging does not resolve localisation keys. `APP_NAME` supplies the stable application and bundle identifiers and the sanitised `<game-name>` filename component. Changing the title or version does not change the installed application's identity; changing `APP_NAME` can require new signing or provisioning setup.

Each editor Pack performs one complete Export, including Export hooks and UI generation, before continuing with before-Pack hooks and packaging. Official Locale Tools uses its before-Pack hook only to exclude `Data/Locale/Locale.xlsx`. Direct command-line run and pack consume files exported beforehand; they keep their validation but do not perform complete Export or implicit UI generation.

**Encrypt game data** controls script compilation and shader, JSON and save encryption, and a partial selection shows its indeterminate state. **Encrypt save contents** sets the C++ Source runtime default. Standalone projects configure it through `SAVE_AS_LDC` in `Scripts/Entry.lua`. See [Saves](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#saves>) for the timing and format rules. The `.ldpak` option is independent of encryption. Data encryption converts staged JSON values to compressed, obfuscated binary `.ldc`; source JSON stays editable. Rebuild packages containing the old compressed-JSON LDC format.

Packaging requires a valid project preview registry. See [Project preview artefacts](<../Native C++ Development/Native UI Adapters.md#project-preview-artefacts>) for preparing the preview runtime.

Packaging works on a staging copy and leaves project sources unchanged. It removes development files, preview-only binaries, project-root `EditorCache`/`Cache` and UI `editor` design data. UI asset keys remain extensionless and Controller references keep their logical Lua module paths, whether Data is stored as JSON, encrypted `.ldc` or in an archive. Directories named `EditorCache` or `Cache` inside `Assets`, `Data` or `Scripts` remain ordinary project content.

### Release version and internal packages

Set **Release version** in Pack Options. C++ Source projects also show **Internal test package (dev)**. The window reads `Main.proj` fields `packaging.version` and, for C++ Source projects, `packaging.dev`, defaulting to `"1.0.0"` and `false`. Confirming valid options saves the version and the applicable dev setting; cancelling writes nothing. Standalone projects always save `packaging.dev=false`, including when an older project has `true`. The version is three non-negative decimal components, such as `1.0.0` or `0.1.0`, up to 127 bytes, with no `v` prefix, leading zeroes, date suffix or prerelease suffix. Version metadata is not defined in Entry.

All game packaging commands accept `--version 1.0.0` and `--release`. C++ Source projects also accept `--dev`; either channel option overrides `packaging.dev` for that invocation only. Standalone projects always package as releases, ignore an older `packaging.dev=true` value and reject an explicit `--dev`. Command-line options do not write back to the project.

The packer captures one UTC+8 build time at the start and reuses it through every stage. Formal packages use the base version as their full release identifier. A dev package appends `YYYYMMDDHH` to it: at `2026-01-01T07:00:00+08:00`, version `1.0.0` becomes `1.0.0.2026010107`, using the build time captured at packaging start. dev does not change Release optimisation, signing, encryption or installation identity.

| Platform field | Value in both modes | Example at the time above |
|---|---|---|
| Android/HarmonyOS `versionName`; Apple `CFBundleShortVersionString` | Base version | `1.0.0` |
| Android/HarmonyOS `versionCode` | `YYMMDDHH` integer | `26010107` |
| iOS/macOS `CFBundleVersion` | `YYMM.DD.HH` | `2601.1.7` |

Build dates are restricted to 2000–2099 and platform limits are validated before replacing an output. Rebuilding within the same hour uses the same build number; wait until the next hour when a new store build record is required. Package paths use `<game-name>-<full-version>` before the existing platform, architecture, variant and signing suffixes. Repacking the same identifier replaces that artefact; other versions and unrelated files remain.

The staging copy receives `Data/BuildInfo.json` containing `version`, `fullVersion`, `dev` and `builtAt` (an ISO 8601 timestamp with `+08:00`). Both dev and release **game packages** retain this file through the normal Data encryption and `.ldpak` flow; it is never written back into source Data. Application release versions are independent of the top-level [project compatibility version](<Project Settings and Asset Layout.md#mainproj>) and [save schema versions](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#saves>).

### Loose and packed output

Every platform keeps the runtime files, `Assets`, `Data` and the runtime `Scripts` as loose directories by default. The `.ldpak` option is clear by default. When it is selected, the complete contents of each directory become root `Assets.ldpak`, `Data.ldpak` and `Scripts.ldpak`. Archive entries retain paths relative to their original directory, so resource paths, Data paths and Lua module names stay unchanged. Encryption and Lua compilation run before archiving, and an archive does not encrypt its contents by itself. The option is independent of encryption. Mobile packages enforce the same loose-or-packed contract, and Android and HarmonyOS do not recompress `.ldpak`.

Resource packaging rejects symbolic links, case collisions, illegal paths and output conflicts. A package contains either all three loose directories (`Assets`, `Data`, `Scripts`) or all three root archives. Corrupt archives, missing roots, mixed layouts and old per-group archives are rejected.

Packaging removes the development-only `Scripts/stub` tree. Any other `.d.lua` file left under `Scripts` fails packaging.

### Windows and macOS

Windows output is `dist/<game-name>-<full-version>/Main.exe` with its runtime files alongside it. macOS output is `dist/<game-name>-<full-version>.app`, with its display name taken from the System title. A custom output directory replaces `dist` as the parent directory. The package directory and its ancestors must not be symbolic links or directory junctions.

Windows and macOS packages copy the runtime files, `Assets`, `Data` and the runtime `Scripts`. Standalone packages keep a root launcher and place the runtime binaries in the platform package directory (`Binaries` on Windows, `Contents/Frameworks` for a macOS app). FFmpeg notices and source-offer material are included where applicable; see [FFmpeg option](<../Getting Started/Project Templates.md#ffmpeg-option>) for the distribution requirements. Windows packages exclude PDB files; retain them with the corresponding build for debugging.

Selecting macOS shows **Sign Application**, which is clear by default. Leave it clear to sign with the identity configured in this machine's environment variables, such as `LUDORK_MACOS_SIGNING_IDENTITY`, `LUDORK_MACOS_SIGNING_CERTIFICATE` and the `LUDORK_MACOS_NOTARY_*` credentials; without them the app receives an ad-hoc signature. Select it to choose a `.p12` certificate with its password, an optional signing identity, and optional notarisation with either an Apple ID, its team ID and an app-specific password, or an App Store Connect key with its key ID and issuer ID. For that run, the dialog settings take precedence over environment variables. Notarisation signs with a real identity, submits the app to Apple, waits for the result and staples the ticket; it is rejected for an ad-hoc identity. Saved signing details are restored after selecting the same certificate. An ad-hoc signature provides neither distributor certificate signing nor notarisation.

### iOS

Selecting the iOS 16.3-or-newer target on macOS requires a C++ Source project, a full Xcode installation with the iPhoneOS SDK, and a completed first-launch Xcode setup. Selecting iOS shows **Sign Application**, which is clear by default. Leave it clear to use the signed-in Apple Development team, taken from `LUDORK_IOS_DEVELOPMENT_TEAM` when it is set, and let Xcode sign automatically. Select it to provide the team ID, a `.p12` certificate with its password and a `.mobileprovision` provisioning profile; the packer then signs manually with that identity and profile and rejects a profile whose team or application identifier does not match. For that run, the dialog settings take precedence over environment variables. The certificate and the profile are required together, and a saved record is restored after selecting the same certificate.

By default, iOS packaging only creates `dist/<game-name>-<full-version>.ipa`, and no iPhone needs to be connected. Select **Export to iPhone** to install and launch the app on the device as well. This mode requires exactly one physical device on the iPhoneOS platform, either an iPhone or an iPad, that `xcdevice` reports as available. Simulators do not qualify. Any unlock, trust or Developer Mode message comes from the device itself. After installation the device must also trust the developer profile, which currently surfaces as a signing error.

### HarmonyOS

HarmonyOS packaging requires a C++ Source project, Apple Silicon macOS and DevEco Studio with the OpenHarmony native SDK. Every HarmonyOS variant uses HarmonyOS 6.0.2 / API 22. Select **Mobile** for the OpenGL ES phone and tablet host, or **2in1** and then OpenGL (default) or OpenGL ES.

Selecting HarmonyOS shows **Sign HAP**, which is clear by default. Leave it clear to produce one arm64-v8a unsigned HAP in `dist`: `<game-name>-<full-version>-harmony-mobile-unsigned.hap`, `<game-name>-<full-version>-harmony-2in1-opengl-unsigned.hap` or `<game-name>-<full-version>-harmony-2in1-opengl-es-unsigned.hap`. Select it to choose a `.p12` keystore, `.cer` certificate and `.p7b` Profile, and enter the key alias and passwords. The key password defaults to the keystore password. A successful signed run publishes the corresponding `-signed.hap`; signing alone requires no connected device and does not install or launch the game.

Signing information can be saved on this machine and restored after selecting the same keystore. The editor saves the material paths and alias in `harmony-signing.json` beside `Ludork.ini`, under `~/Library/Application Support/Ludork` on macOS, and stores passwords in the system Keychain. Keep the signing materials outside the project and repository. Passwords and material details are redacted from packaging logs; no credentials or signing configuration are written to the generated HarmonyOS project.

Both Debug and Release Profiles are supported. The Profile must be valid, signed and match the project’s bundle name. If packaging or signing fails, the previous successful output is retained. Command-line options are documented in [tools/README.md](https://github.com/JasonLeon01/Ludork/blob/main/tools/README.md).

Select **Export to matching HarmonyOS device** to enable signing and additionally install and launch the signed HAP. Only device export invokes HDC and checks the target device's Profile authorisation. Mobile export accepts a connected device that reports `default`, `phone` or `tablet`, although the mobile HAP itself declares `deviceTypes: ["phone","tablet"]`. The 2in1 export accepts only `2in1`. Exactly one connected device must match the selected form, and devices of the other form may remain connected. A connected device whose type cannot be read aborts the export even when it is of the other form. The 2in1 OpenGL package requires `libGLv4.so` on the device. Some emulators omit it. The app reports the missing module and does not fall back to OpenGL ES, so use the OpenGL ES package on those images.

Retain the matching native build outputs for debugging; the packaged native libraries have their symbols stripped.

### Android

Android packaging is available to C++ Source projects on macOS with Android Studio installed in `/Applications` or the current user’s `Applications` directory. It requires SDK Platform 36, Build Tools 36.0.0, a complete stable NDK r27 or newer and host CMake 3.28 or newer. The highest qualifying NDK in the local SDK is selected. SDK and NDK tools must be installed separately. The APK targets API 36 and runs on Android 7.0 / API 24 or later in either landscape orientation. Packaging creates an APK without installing or launching it.

Selecting Android shows **Sign APK**, which is clear by default. Leave it clear to publish `dist/<game-name>-<full-version>-android-arm64-v8a-unsigned.apk`. Select it to choose a JKS or PKCS12 keystore, enter its alias and passwords, and optionally save the information on this device. The picker accepts `.keystore`, `.p12`, `.pfx` and `.pkcs12` files and detects the type automatically. The key password defaults to the keystore password, and a saved record restores the keystore path and alias automatically. Cancelling the dialog returns to Pack Options, and a successful signed run publishes only `dist/<game-name>-<full-version>-android-arm64-v8a-signed.apk`.

Saved records live in the editor's per-user settings directory, as `android-signing.json` beside `Ludork.ini`, and on macOS under `~/Library/Application Support/Ludork`. A record stores the keystore path and alias only, while passwords use the credential store of the operating system. Passwords are never written to the project, Gradle files or logs. Android itself requires an update to an installed application to carry the same signing certificate, so the device rejects an update signed with a different key.

Retain the matching native build outputs for debugging; the packaged native library has its symbols stripped.

## Related pages

- [Project Templates](<../Getting Started/Project Templates.md>)
- [Tilesets, Autotiles and Maps](<Tilesets Autotiles and Maps.md>)
- [Distribution](<../Plug-in Development and Installation/Distribution.md>)
- [Script Mixin Authoring Workflow](<../Lua and Blueprint Scripting/Script Mixins/Authoring Workflow.md>)
