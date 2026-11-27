# Build and Module Layout

Use a C++ Source project when Lua and Blueprint APIs are insufficient, when adding a native UI adapter, or when building for iOS, HarmonyOS, Android or Web. The native runtime uses C++20, CMake and declaration-driven bindings.

## Requirements and build model

Building a C++ Source project requires:

- CMake, a C++20 toolchain and the platform dependencies for your target.
- A supported target platform:

| Target | Minimum OS / API | Architecture / graphics |
|---|---|---|
| Windows | Windows 10 | x64 |
| macOS | macOS 13.3 | Apple Silicon |
| iOS | iOS 16.3 | arm64 |
| HarmonyOS | HarmonyOS 6.0.2 / API 22 | arm64-v8a |
| Android | Android 7.0 / API 24 | arm64-v8a |
| Web | Modern browser | WebGL 2 |

Public headers contain ordinary C++ declarations plus `BIND_*` markers that expand to nothing at compile time. Core bindgen reads the module and shared Runtime declaration roots, then generates one stable LuaGlue source per class, one module aggregate and stub source, a LuaLS stub and editor metadata.

### Reproducible Windows Release builds

Windows Release builds use reproducible compilation and linking across Ludork, SFML, Lua and the separate FFmpeg build:

- **Compilers and linkers:** MSVC uses deterministic compilation; LLVM Clang uses `/Brepro` and maps source and build paths. Both link with `/Brepro` and incremental linking disabled. MinGW suppresses PE timestamps.
- **Hash stability:** With identical sources, dependencies, compiler, SDK, build settings and paths, clean rebuilds preserve EXE and DLL hashes when updating a Standalone project's `Binaries`.
- **Scope:** Changing those inputs can change the output. Reproducibility does not promise identical binaries across different compilers or project paths, or byte-identical installers. Distribution packages omit PDB files.

### Optional server integration

Enable the optional [`Global.Net` snapshot/RPC APIs and health probe](<../Simple Server/Native Client API.md>) through the [server build settings](<../Simple Server/Deployment and Editor Settings.md>). Changing the development URL or key requires a Debug rebuild. Disabled builds keep the APIs, but server interactions print a diagnostic and return; see [Disabled server integration](<../Simple Server/Native Client API.md#disabled-server-integration>).

## Module architecture

| Module | Responsibility | Native dependency direction |
|---|---|---|
| `LudorkRuntime` | Values, VM/binding services, Blueprint execution, typed/component data, JSON and Asset/Data/Script stores, GPU particle simulation and drawing | Standard, SFML and private zlib, plus LuaSF in full mode; never Core modules |
| `Engine` | Engine types, services, state and gameplay foundations | LudorkRuntime and SFML |
| `GlobalCore` | Global game classes and services | Engine, Standard, SFML and LudorkVideo; IXWebSocket for enabled server transport; optional platform libraries |
| `GlobalFunctions` | `Components`, `UI`, `NodeGraph` and `Manager` free-function groups | GlobalCore and SFML |
| `LudorkVideo` | Shared video decoding, seeking and audio extraction under `Engine/Source/Media` | LudorkRuntime, SFML and FFmpeg when enabled |

`LudorkVideo` (`Ludork::Video`) is a static library for video playback and exposes no Lua root. `LudorkRuntime` is shared on desktop and static on mobile, and exposes no Lua root; its bindings live under `Engine`. Gameplay modules follow a fixed load order, never link back to `GlobalFunctions` and carry complete cross-module declarations.

### Resource stores and `.ldpak` archives

`AssetStore`, `DataStore` and `ScriptStore` define the read-only Asset, Data and Lua-module boundaries for both loose directories and `.ldpak` archives, with a user-directory overlay. Core supplies the user root after `setAppName`; Runtime owns lookup and never calls back into Core to choose that directory. See [Runtime resource lookup](<../Editor User Guide/Project Settings and Asset Layout.md#runtime-resource-lookup>) for priority and format rules.

Standard receives `DataStore` file access only, so script archives never appear through filesystem APIs. Private zlib handles archive decompression and CRC32 checks.

`.ldpak` v1 uses a little-endian layout:

- **Header:** The `LDPK` header stores the format version, group, entry count, index range and CRC32.
- **Index:** Sorted in UTF-8 byte order; records paths, directory flags, 64-bit data offsets, original and stored sizes, file CRC32 and block counts. Each path is followed by its block descriptors: stored size, compression flag and original-data CRC32.
- **Blocks:** File data starts on an 8-byte boundary. Blocks follow consecutively, with original sizes derived from the 1 MiB block size and file size; empty files have no blocks. Each block is compressed with an independent zlib level-6 stream and stored compressed only when smaller than the original.
- **Decoding and validation:** Archive and stream readers share block decoding. Each stream caches only its current decoded block, seeks in original bytes and validates block CRC32 on load; complete reads also validate file CRC32. Invalid block ranges, lengths, truncated or trailing compressed data and checksum failures are rejected: streams return their failure result, and complete-file reads throw an error.

### Blueprint runtime and host boundaries

- **Blueprint execution:** Runtime owns Blueprint class generation, Script Mixins, graph execution and latent scheduling. Engine supplies Actor graph lookup, EventBus and frame-loop integration. `RuntimeProviders.installBlueprint(classDataByPath, compileGraph, instantiateGraphTemplate)` installs the three Blueprint callbacks, and `ClassDict` delegates to the VM resolver. Cache and definition identity follow [Execution Flow, Events and Variables](<../Lua and Blueprint Scripting/Blueprint Scripting/Execution Flow Events and Variables.md#class-definitions-and-session-lifetime>).
- **Batched Actor event checks:** `BlueprintRuntimeFacade::hasEvents(objects, eventNames)` returns an object-by-event boolean matrix so Actor update batches reuse class event checks within one synchronisation. Instance graphs and instance-owned methods are checked separately, and the class cache lasts only for the call so subsequent synchronisations observe changes. Actors with ticks disabled defer event checks until ticks are enabled; their event mask resolves after the native update and before event dispatch, and persists until the next synchronisation. Native updates run for every Actor.
- **Provider facade:** Engine owns the `RuntimeProviders` binding facade. Typed `Curve` and `PlainTextConfig` callbacks live in Core, while Blueprint and config installation delegates to Runtime. Fixed-signature providers validate one result at the call boundary; Graph results use shared native ownership. Runtime does not include Core data types. Missing or duplicate installations, wrong results and stopped sessions are errors.
- **Bootstrap modules:** Bootstrap-only native modules are host-owned. The application registers `cjson` and `cjson.safe`, Standard owns `Class` registration, and Engine uses its initialised native tables and APIs instead of reverse module links. Dynamic module resolution is driven by assets and metadata.

### Application hosts and build options

- **Host layout:** The game application host lives in `Application/` (`include/` and `src/`). Android and HarmonyOS application shells live under `Engine/PlatformHosts/Android` and `Engine/PlatformHosts/Harmony`. Build support lives in `Engine/cmake/`, including the FFmpeg builds under `Engine/cmake/FFmpeg/`.
- **Window style:** `runtimeWindowStyle()` in `GlobalRuntime.cpp` defines the game window style in one place. Its fixed-window policy flows to SFML window creation and recreation, native work-area measurements and the HarmonyOS ArkTS host. Platform adapters translate the supplied style into window controls and frame dimensions without choosing a separate resize policy. Display Scale and fullscreen are runtime settings.
- **Save encryption option:** `LUDORK_SAVE_AS_LDC` is a CMake option defaulting to `OFF`. Standard exposes it as `SAVE_AS_LDC` before Entry runs. C++ Source packaging sets this default via **Encrypt save contents**; Entry overrides follow [Runtime Data, Configuration and Saves](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#saves>).
- **Lua toggle:** `LUDORK_WITH_LUA` defaults to `ON` and is required for game builds.
- **UI generation boundary:** Game builds generate native bindings and validate UI assets through the project registry, but do not generate UI Lua Views or declarations. Run editor **Export** or `ludork-assets ui-assets generate <project-root>` (which requires no desktop preview) to generate UI Views. See [Declarative UI Authoring Workflow](<../Lua and Blueprint Scripting/Declarative UI/Authoring Workflow.md>) for file ownership.
- **PreviewHost:** Preview targets, artefacts and cross-build registries are covered in [PreviewHost: Build, Publish and Troubleshooting](<../PreviewHost/Build Publish and Troubleshooting.md>) and [Architecture and Data Flow](<../PreviewHost/Architecture and Data Flow.md>).

## Shared format and rendering constants

`Toolchain/runtime_formats.py` owns `.ldpak` and encrypted-payload layouts, versions, binary data tags and codec limits; `Toolchain/resource_constants.py` owns resource groups, paths and suffixes. CMake runs `ludork-codegen runtime-constants cpp <project-root> <output-directory>` to generate neutral C++ headers under `Intermediate/<build-name>-<hash>/RuntimeConstants/LudorkGenerated`.

Native consumers link `Ludork::RuntimeConstants` rather than including a parent host's private headers. Engine exposes the script, data and animation suffixes through [`ResourceFileConstants`](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Events and Module Root.md#resourcefileconstants>).

`Assets/Shaders/Global/UnoccludedLightPass.frag` holds the sole `LUDORK_MAX_SHADER_LIGHTS` definition: one decimal integer in `1–256`, currently `64`. Generation rejects missing, repeated or invalid definitions. Changing this limit requires rebuilding the native project so the shader, native renderer and read-only `GlobalCore.Light.MAX_SHADER_LIGHTS` agree; an ordinary shader reload does not update the compiled limit.

Constant generation runs for game, desktop Preview and `LUDORK_WITH_LUA=OFF` builds. C++ Source templates keep the shader source and build wiring so normal builds regenerate the headers. Generated headers are build outputs and must not be edited or shipped as template sources.

## LDC binary data

`Runtime/Data.hpp` owns `loadData`, `saveData`, `dataExists`, `decodeData` and the binary payload reader `parseBinaryData`; file APIs accept `std::filesystem::path` or UTF-8 strings. Lua exposes the four data-file APIs on `Engine`, while `parseBinaryData` is a C++ payload-level API. `Runtime/Json.hpp` provides exact-path JSON text operations. Standard owns file access, path resolution and payload compression/obfuscation without depending on Runtime or interpreting the binary value tree.

LDC uses the `LDDC` envelope (version `1`) with zlib compression, stream obfuscation and a checksum. Source length and checksum cover the uncompressed binary payload. A one-byte tag precedes each value:

| Tag | Value and payload |
|---|---|
| `0`, `1`, `2` | Null, false and true, with no following payload |
| `3` | Little-endian signed 64-bit integer |
| `4` | Little-endian finite IEEE 754 binary64 number |
| `5` | Little-endian `uint32` byte length, followed by string bytes |
| `6` | Little-endian `uint32` count, followed by tagged array values |
| `7` | Little-endian `uint32` count, followed by string keys and tagged values; keys use length/bytes encoding without a tag and are written in UTF-8 byte order |

- **Limits and distinction:** Empty arrays and empty maps are distinct. Decoded payloads are limited to 512 MiB and nesting depth to 256.
- **Validation:** Invalid tags, truncation, invalid lengths, duplicate keys, non-finite numbers and trailing bytes fail decoding. Functions, objects, cycles and reference restoration are outside this pure-data format.
- **Legacy payloads:** Old LDC payloads containing JSON text are rejected without fallback or migration; regenerate them from source data. Shader payloads use a separate format.

## MRB rendering

Rendering uses the SFML MRB source tree selected by `versions.conf` together with matching regenerated LuaSF bindings. Set `LUDORK_SFML_SOURCE_DIR`, `LUDORK_LUASF_SOURCE_DIR` and `LUDORK_LUAGLUE_SOURCE_DIR` in the existing CMake build to use external source directories. Dependency initialisation follows the pinned sources and cannot refresh a locally supplied SFML copy.

| Target | Game backend | Editor preview backend |
|---|---|---|
| Windows | Direct3D 11 | OpenGL 3.2 Core |
| macOS | Metal | OpenGL 3.2 Core |
| iOS | Metal | No local PreviewHost |
| Android and HarmonyOS (including 2-in-1) | OpenGL ES 3 | No local PreviewHost |
| Web | WebGL 2 | No local PreviewHost |

Desktop builds include OpenGL alongside the native game backend. The game selects its platform backend before creating GPU resources and does not fall back to OpenGL if that backend fails. PreviewHost runs as a separate process with an explicit OpenGL Core context; preview programs are compiled from GLSL in memory and do not populate the game's persistent shader cache.

### `ContextSettings` and LuaSF

SFML MRB replaces `ContextSettings.majorVersion`, `minorVersion`, `attributeFlags` and `Attribute` with `ContextSettings.backend`:

- Set `backend` to `nil` / `std::monostate` to inherit the device configuration, or to `OpenGLSettings`, `OpenGLESSettings`, `VulkanSettings`, `DirectXSettings` or `MetalSettings`.
- OpenGL version, profile and debug flags belong on `OpenGLSettings`; framebuffer depth, stencil, anti-aliasing and sRGB settings live on `ContextSettings`.
- `Engine.ContextSettings` constructors accept `(depth, stencil, antiAliasing, sRgb, backend)` or a dictionary with those fields.
- Regenerate LuaSF from the matching public headers. The generator excludes `sf::rhi` and the RHI header subtree while exposing ordinary SFML public APIs.

### Shader translation and cache preparation

Shader assets and generated materials are authored in GLSL. The [initialisation scene](<../Lua and Blueprint Scripting/Default Gameplay/Default Template Overview.md#entry-and-modules>) prepares both before running shader-dependent transitions; see [Shader preparation](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Maps and Materials.md#shader-preparation-in-mrb>) for asset programs, generated material passes and built-in lighting and GPU-particle programs. Missing sources, unsupported stages or compilation failures halt initialisation, and drawing requires a prepared program.

- **Translation pipeline:** The C++ translator emits HLSL for Direct3D 11, MSL for Metal and ESSL for OpenGL ES / WebGL, preserving stage interfaces, uniform names and types, texture bindings and engine coordinate conventions. On native platforms, programs needing translation share a worker pool with material and shader cache checks; each worker translates a complete program so stages link together. WebGL runs on a single thread, yields between batches and keeps the last complete loading frame visible while writing caches to browser storage. GPU object creation and pipeline prewarming run in program order on the preparation thread.
- **Derived cache layout:** Derived files mirror each source's relative path under the user data directory, with target-language and program-identity suffixes where needed. GLSL sources and translated text follow the shader-encryption setting, while cache records follow the data-encryption setting. Derived caches are always stored as loose files even when the game uses `.ldpak`.
- **Validation and incremental recovery:** Every `Init` validates source content, stage combinations and cached outputs in C++. Cache hits restore GPU objects, skip repeated translation and compilation, and leave unchanged files untouched. GLSL or translation-setting changes affect only the matching program combinations. Translator and platform-compiler identities are tracked separately: valid HLSL can repair missing or invalid DXBC without retranslating GLSL, changing encryption updates stored forms while reusing valid DXBC, and material export keeps validated derived shaders when generated GLSL is unchanged.
- **Windows file-lock retries:** On Windows, each cache rename or rollback-file removal blocked by a temporary access or sharing violation retries for up to 10 seconds. Cancellation interrupts the wait, and rollback gets a separate retry window to restore the previous cache.

### Backend binary caches and GPU particles

- **Direct3D 11 (Windows):** Initialisation reuses validated DXBC across stages sharing the same HLSL content, stage type, entry point and compiler fingerprint, compiles each remaining distinct input once in parallel and writes raw DXBC at each stage's cache path. Runtime loading reads these bytes without a shader-encryption wrapper. A driver change recreates and validates GPU objects while keeping valid DXBC. DXBC is Direct3D bytecode rather than GPU machine code; the OS and driver own the final Direct3D 11 cache.
- **Metal (macOS and iOS):** Retains MSL and a device- and driver-specific `MTLBinaryArchive`. Archive hits reuse pipelines without re-recording them, and unchanged archives are neither serialised nor republished. Missing or invalid entries recover incrementally while usable archive entries stay intact. Session preparation uses Metal's MSL frontend to create libraries.
- **OpenGL ES and WebGL:** Retains ESSL and skips persistent GPU-binary generation; GPU programs compile and link at runtime.
- **MRB GPU particles:** Simulation and drawing run on the GPU via SFML RHI, including vertex transformation, instanced drawing and floating-point curve textures. Alive counts are reduced on the GPU and only the final 4-byte count is read asynchronously. Count and timer results are optional when the backend lacks those statistics capabilities; missing simulation capabilities are an error with no CPU fallback.

## GPU particle ownership

GPU particle configuration and execution are split cleanly between Core and Runtime:

- **Core public surface:** Core owns `Emitter`, `EmitterComponent`, `EmitterView`, Scene scheduling and the existing `ParticleSystem`. `EmitterConfiguration` holds `EmitterTrack` values, each combining Runtime's `EmitterTrackParameters` with Core's eight named `EmitterCurves` channels (`Curve::CurveData`). `setConfiguration` centrally validates values, samples each `Curve` into a 256 × 8 float lookup table per track and restarts playback. `getConfiguration` returns an independent value copy.
- **Resource decoding:** Within the Emitter path, `RuntimeData` is confined to resource JSON input in Core's `EmitterResource.hpp/.cpp`, which maps authored string choices, 0–255 colours, arrays and curve references to fixed configuration fields. `Emitter` stores that configuration value; the backend stores its compiled configuration.
- **Curve layout and configuration:** `Runtime/Graphics/GpuEmitterCurveLayout.hpp` owns the 256 × 8 curve layout and its packing into two RGBA rows so CPU baking, GPU texture allocation and generated shader coordinates share sample counts and indexing. `Runtime/Graphics/GpuEmitterConfiguration.hpp` carries Runtime track parameters and baked curve samples without Core types.
- **Unbound GPU backend:** `GpuEmitterBackend` (`Runtime/Graphics/GpuEmitterBackend.hpp`) owns playback state, fixed simulation steps, GPU tracks, drawing, asynchronous statistics and deferred GPU release through a private implementation. It accepts resolved configuration and never interprets `Data/Particles` keys or depends on Core Curve, Actor or UI classes.
- **Statistics and bindings:** `EmitterStatistics` is a Runtime value record with required `capacity` and `time` fields plus optional renderer, count and timing fields. Core returns it directly; the Lua binding writes a declared-field snapshot table preserving unavailable optional values as `nil`. Public `Emitter` forwards playback calls to the backend. Configuration, track, curve, burst and statistics records each use a separate class-named header whose Lua registrations and declarations are generated under `Engine` (Runtime exposes no Lua root).
- **Implementation and GPU lifetime:** GPU sources, programs, buffers and private headers live under `Runtime/src/Graphics/Emitters`, built on SFML RHI. Texture reads reuse Runtime `AssetStore` and `AssetInputStream` across loose and packed layouts. Runtime depends on Standard and SFML; Core includes only Runtime's public graphics headers. The Runtime Graphics CMake subdirectory compiles this backend once in both full and `LUDORK_WITH_LUA=OFF` builds.
- **Deferred GPU release:** Shared GPU resources are keyed by the active SFML RHI device. Shutdown queues retired GPU objects without requiring a context on the caller's thread, and collection runs with the matching resource context active. Scene and preview hosts own windows and render targets and collect retired resources before destroying those contexts. GPU resource lifetime is independent of the Lua VM lifetime.

## Session and module lifetime

- **Session ownership:** Standard owns the Lua session. `runtime::initialize` requires a running Standard session, and repeated calls preserve the attached Runtime state. Once `runtime::shutdown` detaches Runtime, restarting requires a new Standard session and Lua VM.
- **Shutdown order:** Standard shuts down modules in reverse dependency order (`Global`, `Engine`, `Runtime`) before releasing its state. The host owns the Lua VM and must wait for shutdown to finish on the initiating thread before calling `lua_close`. If LuaSF initialisation or binding registration fails before Standard starts, the host must still call `LuaSF_quiesce_state` and `LuaSF_shutdown_state` before `lua_close` to release any partially initialised glue session.
- **Session-independent stores:** `AssetStore`, `DataStore`, `ScriptStore` and the Standard read-only file provider are host configuration. They are independent of the VM session and survive session shutdown.
- **Execution scope and VM lock:** Standard establishes a LuaGlue execution scope at each host Lua entry. Nested glue operations reuse that scope only while the host confirms the current thread owns the VM lock and execution hooks are unchanged. `LuaExecutionPause` suspends ownership; operations during a pause must acquire the lock again. Hosts without an ownership probe use the ordinary enter/leave hooks.
- **Stack checks (`LuaInvocation.hpp`):** Within a VM ownership scope, `ludork::standard::ensureLuaStack(state, count, context)` reserves space for `count` additional values, throwing `std::length_error` above `INT_MAX` and `std::runtime_error` if stack expansion fails.
- **Protected Lua calls:** `invokeLuaFunction(state, callable, arguments, context)` takes a `lua_glue::Object` callable and `std::vector<lua_glue::Object>` arguments, checks capacity, pushes them and performs a protected call. On success it leaves all results above the entry stack top and returns their count; on any exception it restores the entry stack top before propagating. The separate `class_runtime::invoke(state, callable, argumentCount)` consumes arguments already on the Lua stack.

## Host and implementation sources

Each independent class or bound type uses a same-named header under `include` and, where needed, a source file under `src`. Owner-specific data types may be nested; shared data and helper collections may stay together.

### Implementation layering and shared internals

- **Host-private `Impl` layout:** Host-private implementation lives under `src` with an `Impl` suffix, such as `HostImpl.hpp` for `Host::Impl`. Larger implementations may use a same-named subdirectory contributing sources via `target_sources`. Dependencies flow strictly from the host into its implementation: child directories must not include parent-private headers (including via include-root shortcuts), and public headers must not include private headers.
- **Shared Core and Runtime internals:** Core shares `SpriteVisuals`, joystick device tracking and text configuration codecs through `src/Shared/include/CoreShared` (implemented under `src/Shared/src`); consumers include only the interface headers, while host companion headers stay beside their host source. Shared Runtime Lua services live in `Runtime/src/LuaServices`, and pure ConfigVars parsing lives in `Runtime/src/Metadata`.
- **Application platform interfaces:** Platform implementations share neutral interfaces under `Application/src/Platform/include`. The application host applies platform results to runtime launch options; platform implementations neither include host-private headers nor own startup-error reporting.
- **Windows launchers:** Windows editor and game launchers compile the shared source under `Engine/PlatformHosts/Common`, where `WindowsLauncher.hpp` exposes `RunLauncher` with a relative executable path and `LauncherWorkingDirectory` policy. The editor launches `Binaries\Ludork.exe` with the launcher directory as its working directory; the game launches `Binaries\Main.exe` inheriting the caller's working directory. Both forward arguments, standard streams and the child exit status. C++ Source templates retain this shared source.

### Standard `ClassRuntime` layers

Standard `ClassRuntime` dependencies flow `Class → Composite → Instance → Native → Detail`, with downward shortcuts permitted:

- **Detail:** Neutral state and queries.
- **Native:** Native members and properties.
- **Instance:** Native construction and disposal.
- **Composite:** Indexing and metatables.
- **Class:** Allocation, finalisation and `super`.

### Boundary validation (`ImplBoundaryValidate`)

The boundary checker inspects private headers in Engine and Application (including parent-private headers reached via include search roots), verifies Standard layer direction and acyclicity, and rejects static `Source.*` project Lua module paths or type references in first-party Engine C++ strings and binding annotations. Static checks do not trace dynamically constructed paths; generic native services receive project module names from callers.

CMake's `ImplBoundaryValidate` target records a successful check in the build directory and reuses it while inputs remain unchanged:

- **Tracked inputs:** Source, Runtime, Standard and Application source trees, Lua runtime files, LuaSF headers, the compiled Toolchain bundle, validation build rules, and file/directory additions or removals.
- **Lua stubs and metadata:** Adding or removing a Lua stub invalidates the cache, while refreshing stub contents does not. Generated `*_meta.lua` contents are not inputs.
- **Failures and direct runs:** A failed check removes the success marker and blocks the build until a subsequent check passes. Direct `ludork-check impl-boundary-check <project-root>` commands (including native CI) always run the full check. Rebuild Toolchain after changing checker sources before invoking its compiled executable.

### System composition, threading and `CellSize`

- **System components:** `System` privately composes `Display` (window), `FramePipeline` (canvas, effects and transition targets), `SceneStack` (queued operations and retired scenes) and `Lifecycle` (shutdown state). Components expose operations rather than mutable state aggregates. Frame presentation stays outside the scene logic lock, and render targets rebuild between frames.
- **Scene logic worker context:** On hosts with a separate scene logic worker, OpenGL and OpenGL ES give that worker its own shared graphics context for the lifetime of the logic loop. Rebuilding a Camera render texture on the game thread leaves the worker's context active, and the scene logic lock serialises access to shared render textures. See the [SceneBase threading rules](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/GlobalCore/Scenes and System.md#scenebase>).
- **Cross-thread video playback:** During the game loop, a synchronous video playback request from another thread is queued onto the game thread, and the caller waits until playback finishes or fails.
- **`EngineState::CellSize`:** A positive C++ compile-time constant with no project-level or runtime override. Changing it requires rebuilding the engine, the editor and the templates.

## Native text input boundary

Engine owns the edit session through `Input/TextInputService.hpp`, while platform hosts implement `Input/TextInputHost.hpp`. The contract uses UTF-8 text, byte offsets at grapheme boundaries and caret rectangles in window-client pixels.

Hosts emit session-tagged events through a thread-safe sink. Service operations run on the game thread or before it starts. Standard provides Unicode segmentation and whitespace handling through `utf8proc`.

## Native WebView boundary

Runtime owns the native web-page service and the `Engine.OpenWebView(url)` binding. `Runtime/WebViewHost.hpp` defines the platform host factory, window attachment and session-tagged completion; `Runtime/WebViewService.hpp` supplies input blocking.

- **Lifecycle:** Factories may be installed before VM initialisation. `Display` attaches a host to the live native window and detaches it before replacement or destruction. Each open request gets a new ID; the host navigates its existing layer, so a late completion for an older request cannot close the current session. A rejected request preserves the existing page and session.
- **Platform backends:** Hosts run UI work asynchronously without blocking the game loop or calling Lua on completion. macOS and iOS use `WKWebView`, Android uses the system WebView, HarmonyOS uses ArkWeb, and Windows uses the system Evergreen WebView2 Runtime.
- **HarmonyOS input isolation:** The HarmonyOS overlay root sets `HitTestMode.BLOCK_HIERARCHY` so its Web component and Close button receive touch events while lower siblings and ancestors do not. The game `XComponent` stays mounted beneath the overlay.

## Core headers and Runtime boundary

Core Lua-binding headers include `<CoreMinimal.hpp>` from `Engine/Runtime/include`, which supplies annotations, runtime types, common C++ types and the SFML Graphics, Window and System modules. Include module exports and direct type dependencies explicitly; every public header must compile independently.

Handwritten Core (including private sources) accesses VM and binding services exclusively through Runtime APIs. LuaGlue API calls belong to Runtime internals and generated bindings; precompiling LuaGlue headers does not relax this boundary. Runtime also owns isolated audio execution, while Core retains gameplay and playback policy. Runtime internals execute private operations within an active `RuntimeScope`, and facades provide the external entry boundary.

For API types, container ownership and Lua-reference operations, see [Runtime Value Boundaries](<Runtime Value Boundaries.md>).

## Native pointer casts

`<CoreMinimal.hpp>` includes `<Cast.hpp>`; lower-level headers may include `<Cast.hpp>` directly. Use `ludork::Cast<T>(pointer)` for checked native downcasts and cross-casts after [declaring the cast protocol](<Macro Reference.md#native-cast-declarations>), passing the target class `T` without `*`. Statically known upcasts may use implicit conversion or `static_cast`; never use an unchecked `static_cast` in place of a dynamic type check.

- **Pointer semantics:** Both raw pointers and `std::shared_ptr` are supported; reference and `void` targets are not provided. Null or mismatched inputs return empty results. Constness and multiple-inheritance subobject offsets are preserved. Successful `std::shared_ptr` casts alias the original control block, and the rvalue overload transfers ownership only on success.
- **Type keys:** Dynamic type queries compare compiler-generated type-signature strings by content across modules. These keys are internal to a complete build with the same toolchain, not persistent IDs or a cross-toolchain ABI.

## Build the project

Run the platform build script from the repository or project root:

```bash
./tools/build_cpp.sh Game Debug
./tools/build_cpp.sh Game Release
```

```bat
tools\build_cpp.bat Game Debug
tools\build_cpp.bat Game Release
```

### Toolchain bundle and entry points

Repository initialisation prepares the shared toolchain bundle at `.tools/Toolchain`; installed editors and extracted BuildTools packages place it at `BuildTools/Toolchain`. The bundle contains six command-family executables (`ludork-build`, `ludork-codegen`, `ludork-assets`, `ludork-pack`, `ludork-editor` and `ludork-check`, with `.exe` on Windows) sharing one runtime directory — keep the complete directory together when copying or distributing it. Published tools need no system Python; rebuilding Toolchain from source requires Python 3.12.

- **Build command:** Source-checkout and installed `build_cpp` scripts call `ludork-build native-build <project> <Debug|Release> --tools-directory <tools>`, which owns configuration, compilation and the successful-build record. `build_preview_host` runs the same command with `--target PreviewHost` and validates the published preview. Shared editor-launcher and host Lua-compiler tasks are documented in [Shared build tasks](https://github.com/JasonLeon01/Ludork/blob/main/tools/README.md#shared-build-tasks).
- **Direct CMake and dependencies:** Platform scripts locate Toolchain automatically. Direct CMake integration passes `-DLUDORK_TOOLCHAIN_DIRECTORY=/path/to/Toolchain`, and IDE generation accepts `--toolchain-directory`. Prepare individual native dependencies with `ludork-build dependencies <project-root> --repository-root <repository-root> --only <name>` (omit `--only` for all dependencies); see [toolchain maintenance commands](https://github.com/JasonLeon01/Ludork/blob/main/tools/README.md#toolchain-maintenance-commands).
- **Editor workflow:** **Construct** runs the Debug build for a C++ Source project. After a direct CMake build, run an incremental `build_cpp` to refresh the build record used by **Play**. Changes to third-party sources or external CMake settings also require **Construct** or `build_cpp`; see [Run, Debug and Package](<../Editor User Guide/Run Debug and Package.md>).
- **CI runners:** Download `Ludork-BuildTools-<version>-<platform>.tar.gz` from the matching Release, extract it and run the build or packaging entry point under `BuildTools/`. Install the project's required compiler and SDK on the runner.

### Windows compilers

Windows x64 game builds support MSVC, MinGW-w64 GCC and LLVM Clang (`clang-cl.exe`), including PreviewHost, FFmpeg-enabled builds, Source-to-Standalone conversion and game packaging. GNU Make and Git Bash are required wherever a dependency build uses them (including FFmpeg); installed editor tools supply Toolchain and GNU Make under `BuildTools/`. Tool preparation and editor packaging keep their standard toolchain.

- **LLVM Clang requirements:** Requires `clang-cl.exe`, `lld-link.exe` and `llvm-lib.exe` from the same LLVM installation, Visual Studio 2022 C++ build tools, a Windows SDK and Ninja on `PATH`. It targets the MSVC ABI and runtime libraries using Ninja, and does not require Visual Studio's ClangCL integration component.
- **Explicit compiler selection:** `build_cpp`, `build_preview_host`, `pack_standalone` and `pack_project` read the project's [compiler setting](<../Editor User Guide/Project Settings and Asset Layout.md#mainproj>) and configure the selected MSVC, MinGW or LLVM toolchain explicitly rather than inheriting another compiler from `PATH`. A missing configured compiler fails the build without silently substituting another installation.
- **Switching compilers:** All three compilers write CMake output to `build` and runtime output to `bin/<configuration>`. Use [Change project compiler](<../Editor User Guide/Run Debug and Package.md#windows-project-compiler>) in the editor to confirm removal of previous native outputs before switching. Command-line builds fail without deleting files if existing CMake or build records belong to another compiler; do not preconfigure a different generator or compiler in the same cache.
- **MinGW runtime files:** MinGW builds publish `WebView2Loader.dll` and the required GCC runtime DLLs beside the game and PreviewHost in `bin/<configuration>`. Editable Standalone projects and game packages carry these files in `Binaries`, so target machines need no MinGW installation (web pages still require the installed Evergreen WebView2 Runtime). When distributing a MinGW-built game, include the runtime notices and compiler runtime source-distribution materials described in `Licenses/NativeDependencies/MinGW-RUNTIME-NOTICE.md`.

> **Note:** WinLibs GCC 14.2 can fail to create generated `.obj.d` files when full paths exceed Windows' traditional path-length limit, even when the parent directory exists. Create MinGW projects under a short root path to leave room for generated binding filenames.

### Compiler flags, RTTI, IPO and precompiled headers

- **Debug optimisation:** `LUDORK_OPTIMIZE_DEBUG` defaults to `ON` and controls Debug optimisation, including Debug IPO for LuaSF binding objects and the shared LuaGlue runtime. LuaGlue checks argument types and bounds in every configuration.
- **Disabled RTTI:** First-party C++ must not use `dynamic_cast`, `std::dynamic_pointer_cast` or `typeid`. `Engine/cmake/LudorkCppOptions.cmake` disables RTTI for C++ and Objective-C++ in every configuration (`/GR-` on MSVC and `clang-cl`; `-fno-rtti` on GCC, GNU-style Clang and AppleClang) across game, Preview, Lua-disabled builds and source-built dependencies. LuaGlue identifies types without RTTI, and exception handling stays enabled. Rebuild all native modules together after changing this policy; imported libraries and system frameworks keep their own settings.
- **Release IPO and symbol visibility:** Release builds enable IPO/LTO for Standard, its `ClassRuntime` OBJECT target, the final mobile application target, Runtime and Core (MinGW keeps IPO disabled). `ClassRuntime` hides symbols and inline symbols while keeping explicit public exports available.
- **Release linking and debug symbols:** Apple Release links use `-dead_strip`; MSVC and LLVM Clang emit separate PDB files with `/OPT:REF /OPT:ICF`; Android and HarmonyOS use toolchain section garbage collection. Retain unstripped build outputs and debug symbols for diagnosing releases — [packaging](<../Editor User Guide/Run Debug and Package.md#packaging>) strips only the final distribution output.
- **Precompiled headers (PCH):** Enabled by default for `LudorkStandard`, `LudorkStandardClassRuntime`, `LudorkRuntime`, `Engine`, `GlobalCore`, `GlobalFunctions`, `PreviewHostRuntime` and `PreviewHostExecutable`. Each target builds a PCH specific to its compiler, configuration, architecture and compile definitions — never reuse PCH files across targets or build variants.
- **PCH contents and exclusions:** Lua-enabled builds precompile `LuaSF.hpp` and `LuaGlue/LuaGlue.hpp`; `LUDORK_WITH_LUA=OFF` builds precompile standard-library headers. Objective-C/Objective-C++ sources and files with per-file MSVC or `clang-cl` Debug optimisation overrides skip PCH. Every source still needs its direct includes, and every public header must compile independently.
- **Generated binding targets:** Generated Core bindings compile in the internal `EngineBindings`, `GlobalCoreBindings` and `GlobalFunctionsBindings` OBJECT targets whose PCH starts with the module's generated conversion traits. Their object files link into the existing host libraries with no extra runtime libraries or Lua modules. On macOS, single-arm64 AppleClang Release builds compile each binding target's PCH and generated sources with `-Os`.
- **Disabling PCH:** Run `cmake -S Game -B Game/build -DCMAKE_DISABLE_PRECOMPILE_HEADERS=ON` (substituting your project's source and build paths), then run `build_cpp` to update the build record for **Play**. Generated binding units then receive their conversion traits via forced inclusion. Set `-DCMAKE_DISABLE_PRECOMPILE_HEADERS=OFF` to re-enable PCH.

### Third-party source dependencies

- **LuaSF and Lua:** LuaSF is a binder only and owns neither the Lua VM nor its dependencies. The game runtime creates `lua_State` with `luaL_newstate` and `luaL_openlibs`, hands it to LuaSF via `LuaSF_initialize_state` and `LuaSF_register`, and tears it down through `ludork::standard::shutdown` and `lua_close`. Both LuaSF and SFML consume one Lua 5.5 runtime; desktop builds share LuaGlue, while mobile and Web targets link it statically.
- **Source overrides:** CMake reads LuaSF, LuaGlue, SFML and Lua from `Game/Engine/ThirdParty`. Override them with `-DLUDORK_LUASF_SOURCE_DIR`, `-DLUDORK_LUAGLUE_SOURCE_DIR`, `-DLUDORK_SFML_SOURCE_DIR` and `-DLUDORK_LUA_SOURCE_DIR`. LuaGlue defaults to the sibling of the selected LuaSF directory (for example, `-DLUDORK_LUASF_SOURCE_DIR=/path/to/LuaSF-AutoGenerator/output/LuaSF`). Dependency initialisation installs LuaSF and LuaGlue from one archive with matching `.ludork-version` markers.
- **Local source archives:** Before dependency initialisation, set `LUASF_SOURCE_ARCHIVE` (`.tar.gz` on Unix shells or `.zip` on Windows; must extract directly to `LuaSF/` and `LuaGlue/` — older layouts are rejected) or `SFML_SOURCE_ARCHIVE` (`.zip` or `.tar.gz` rooted at `SFML/` containing `CMakeLists.txt` and `include/SFML/Config.hpp`). Explicit imports replace the installed sources; packaging never replaces them.
- **SFML:** SFML is a required source project whose `sfml-*` targets LuaSF reuses. Mobile uses static SFML; desktop follows `LUASF_BUILD_SHARED_SFML` (default shared). `versions.conf` selects the SFML repository and tag (currently SFML-ME `MRB3100`). HarmonyOS GPU particles require an SFML source preserving the GLES 3 context request; see [Common Particles](<../Editor User Guide/Common Particles.md>).
- **IXWebSocket:** Dependency initialisation installs IXWebSocket 12.1.0 at `Engine/ThirdParty/IXWebSocket` (pinned in `versions.conf` to commit `bf7ad53c02612643e1cf72e37c8749491798d33d`). `Engine/cmake/LudorkIXWebSocket.cmake` provides static `ixwebsocket::ixwebsocket` for GlobalCore's optional WebSocket transport, disabling TLS, compression, demos, tests and installation within its scope.
- **IXWebSocket packaging and transport:** Source templates and mobile packaging retain IXWebSocket source, and packages ship `Licenses/IXWebSocket/LICENSE.txt` and `THIRD-PARTY.txt`. Native clients accept `http://` and derive `ws://`; Web builds use browser HTTP/WebSocket transports. See [Configure a packaged client](<../Simple Server/Deployment and Editor Settings.md#configure-a-packaged-client>).
- **Bindgen and output directories:** CMake runs `ludork-codegen core-bindgen` automatically; see [Generated Metadata and Stubs](<Generated Metadata and Stubs.md>). `Intermediate/` is excluded from templates and recreated by a normal build; `bin/` and `build/` live at the project root.

### Mobile and Web builds

- **Web (WebGL 2):** Uses Emscripten's CMake toolchain with Ninja, writing outputs to `build-web` separately from native previews. Runtime, Lua modules and optional FFmpeg link statically into one Wasm module using the OpenGL ES 3 shader translation path. The browser main thread runs the game with Asyncify (cooperative background work, no pthreads or cross-origin isolation headers): custom native code must yield during long work and must not start native threads or rely on OS sockets. The host restores browser storage and prepares audio before the start gesture enters Lua.
- **Web FFmpeg:** `Engine/cmake/FFmpeg/build_web.sh` builds static Wasm MOV/H.264/AAC software decoders with `swscale` and `swresample`, excluding threading, hardware acceleration, network protocols, encoders and CLI tools (Windows requires Git Bash and GNU Make). See [WebGL packaging](<../Editor User Guide/Run Debug and Package.md#webgl>).
- **iOS:** Uses a single-window `UIScene` host whose scene manifest selects the scene delegate. The host associates its window with a `UIWindowScene` and starts the runtime once active. A temporary scene disconnection pauses the live runtime and retains its window for reconnection within the same session. Both the application and optional FFmpeg target iOS 16.3.
- **HarmonyOS toolchain:** Cross-built on Apple Silicon macOS via DevEco Studio's OpenHarmony native SDK; each package recreates its Stage and native build directories. Both the HAP target and compatible SDK are `6.0.2(22)` (`OHOS_COMPATIBLE_SDK_VERSION=22`), compiling Ludork, Lua, LuaSF, LuaGlue and SFML for `aarch64-linux-ohos22.0.0` with API availability diagnostics enabled (a newer compile SDK does not raise the deployment target).
- **HarmonyOS runtime and FFmpeg:** The app selects OpenGL ES 3 before creating GPU resources, declares `phone`, `tablet` and `2in1` in its Stage manifest, and requests `ohos.permission.LOCK_WINDOW_CURSOR`. Phones and tablets open in landscape; 2-in-1 devices support fullscreen and floating windows. FFmpeg-enabled builds use the same versioned target, OpenHarmony sysroot, MUSL selection and availability checks.
- **Android:** Cross-built on Apple Silicon macOS using host CMake 3.28+ with `Unix Makefiles`, the highest complete stable NDK toolchain (r27+) under the local SDK's `ndk` directory, `ANDROID_PLATFORM=android-24`, `ANDROID_ABI=arm64-v8a` and `ANDROID_STL=c++_static`. Projects never bundle an SDK or NDK, and Android Studio's SDK CMake and Ninja are not used. Lua, LuaSF, LuaGlue, Runtime, Core, `cjson`, SFML and optional FFmpeg link statically into the single shared output `libludork.so`, consumed by Gradle as a prebuilt `jniLibs` entry.

## Load the runtime modules

```lua
local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")

local Components = GlobalFunctions.Components
local UI = GlobalFunctions.UI
local NodeGraph = GlobalFunctions.NodeGraph
local Manager = GlobalFunctions.Manager
```

Load these three roots in the order shown above. Access `GlobalFunctions` groups through their tables so they do not leak into the module root. Standard globals are already installed; `CoreSystem` is not a runtime module.
