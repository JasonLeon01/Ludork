# Build and Module Layout

Choose a C++ Source project when the Lua and Blueprint APIs are insufficient, when a new native UI adapter is required, or when producing an iOS, HarmonyOS or Android build. The runtime uses C++20, CMake and declaration-driven bindings.

## Prerequisites

- A C++ Source project with CMake, a C++20 toolchain and its platform dependencies installed.
- A supported target: Windows 10 or newer on x64, macOS 13.3 or newer on Apple Silicon, iOS 16.3 or newer on arm64, HarmonyOS 6.0.2 / API 22 or newer on arm64-v8a, or Android 7.0 / API 24 or newer on arm64-v8a.

Public headers contain ordinary C++ declarations plus `BIND_*` markers that expand to nothing at compile time. Core bindgen reads the module and shared Runtime declaration roots, then generates one stable LuaGlue source per class, one module aggregate and stub source, a LuaLS stub and editor metadata.

The `LUDORK_OPTIMIZE_DEBUG` setting also controls Debug interprocedural optimisation for LuaSF binding objects and the shared LuaGlue runtime.

Windows MSVC Release builds use deterministic compilation and `/Brepro` linking with incremental linking disabled, including the SFML and Lua dependencies. The separate FFmpeg build uses the same linker policy. This removes build-time timestamp differences from rebuilt EXE/DLL files when updating a Standalone project's `Binaries`. The first rebuild with these settings changes existing binaries once; changes to native inputs, dependencies, compiler, SDK or build settings can still change their contents. Packages continue to omit PDB files; retain the matching build symbols for debugging.

## Steps

Enable the optional [`Global.Net` snapshot/RPC APIs and health probe](<../Simple Server/Native Client API.md>) through the [server build settings](<../Simple Server/Deployment and Editor Settings.md>). Changing the development URL or key requires a Debug rebuild. Disabled builds retain the APIs, but server interactions only print a diagnostic and return; see [Disabled server integration](<../Simple Server/Native Client API.md#disabled-server-integration>).

### Choose a module

| Module | Responsibility | Native dependency direction |
|---|---|---|
| `LudorkRuntime` | Values, VM/binding services, Blueprint execution, typed/component data, JSON and Asset/Data/Script stores, GPU particle simulation and drawing | Standard, SFML and private zlib, plus LuaSF in full mode; never Core modules |
| `Engine` | Engine types, services, state and gameplay foundations | LudorkRuntime and SFML |
| `GlobalCore` | Global game classes and services | Engine, Standard, SFML and LudorkVideo; IXWebSocket for enabled server transport; optional platform libraries |
| `GlobalFunctions` | `Components`, `UI`, `NodeGraph` and `Manager` free-function groups | GlobalCore and SFML |
| `LudorkVideo` | Shared video decoding, seeking and audio extraction under `Engine/Source/Media` | LudorkRuntime, SFML and FFmpeg when enabled |

`LudorkVideo` (`Ludork::Video`) is a static library for video playback. It exposes no Lua root.

`LudorkRuntime` is shared on desktop and static on mobile, and it exposes no Lua root. Bindings remain under `Engine`. AssetStore, DataStore and ScriptStore define the read-only Asset, Data and Lua-module boundaries for both the loose layout and the `.ldpak` layout. Standard receives DataStore file access only, so script archives never appear through filesystem APIs. Private zlib handles archive decompression and CRC32. Gameplay modules keep a fixed load order, do not link back to `GlobalFunctions` and carry complete cross-module declarations.

`.ldpak` v1 is little-endian. Its `LDPK` header stores the version, group, entry count, index range and CRC32. The file and directory index is sorted by UTF-8 byte order and records paths, directory flags, 64-bit data offsets, original and stored sizes, file CRC32 and block counts. Each path is followed by its block descriptors: stored size, compression flag and original-data CRC32. File data starts on an 8-byte boundary; blocks follow consecutively, with original sizes derived from the 1 MiB block size and file size. Empty files have no blocks. The writer uses an independent zlib level-6 stream per block only when it is smaller than the original.

Archive and stream readers share block decoding. Each stream caches only its current decoded block, seeks in original bytes and validates block CRC32 on loading; complete reads also validate the file CRC32. Invalid block ranges, lengths, truncated or trailing compressed data and checksum failures are rejected. Streams return their existing failure result; complete-file reads throw an error.

Runtime owns Blueprint class generation, Script Mixins, graph execution and latent scheduling. Engine supplies Actor graph lookup, EventBus and frame-loop integration. `RuntimeProviders.installBlueprint(classDataByPath, compileGraph, instantiateGraphTemplate)` installs its three Blueprint callbacks. ClassDict delegates to the VM resolver. Cache and definition identity follow the rules in [Execution Flow, Events and Variables](<../Lua and Blueprint Scripting/Blueprint Scripting/Execution Flow Events and Variables.md#class-definitions-and-session-lifetime>).

The C++ `BlueprintRuntimeFacade::hasEvents(objects, eventNames)` returns an object-by-event boolean matrix. Actor update batches use it to reuse class event checks within one synchronisation. Instance graphs and own methods are checked separately, and the class cache ends with the call, so later synchronisations observe method and graph changes. Actors whose ticks are disabled defer event checks until ticks become enabled; their event mask is then resolved after the native update and before event dispatch, and retained until the next synchronisation. Native updates still run for every Actor.

Engine owns the `RuntimeProviders` binding facade. Its data resolvers retain typed Curve and PlainTextConfig callbacks in Core, while Blueprint and config installation delegates to Runtime. Fixed-signature providers validate one result at the call boundary; Graph results use shared native ownership. Runtime does not include Core data types. Missing or duplicate installations, wrong results and stopped sessions remain errors.

Bootstrap-only native modules are host-owned. The application registers `cjson` and `cjson.safe`, Standard owns `Class` registration, and Engine uses its initialised native tables and APIs instead of reverse module links. Dynamic module resolution remains driven by assets and metadata.

`LUDORK_SAVE_AS_LDC` is a CMake option that defaults to `OFF`. Standard exposes it as `SAVE_AS_LDC` before Entry runs. C++ Source packaging sets this default through **Encrypt save contents**. Entry overrides follow the rules in [Runtime Data, Configuration and Saves](<../Lua and Blueprint Scripting/Default Gameplay/Runtime Data Configuration and Saves.md#saves>).

The game application host lives in `Application/` (`include/` and `src/`). Android and HarmonyOS application shells live under `Engine/PlatformHosts/Android` and `Engine/PlatformHosts/Harmony`. Build support lives in `Engine/cmake/`, including the FFmpeg builds in `Engine/cmake/FFmpeg/`.

`runtimeWindowStyle()` in `GlobalRuntime.cpp` defines the game window style once. Its fixed-window policy flows to SFML window creation and recreation, native work-area measurements and the HarmonyOS ArkTS host. Platform adapters translate the supplied style into window controls and frame dimensions; they do not choose a separate resize policy. Display Scale and fullscreen remain runtime settings.

`LUDORK_WITH_LUA` defaults to `ON` and is required for game builds.

Game builds generate native bindings and validate UI assets through the project registry. UI Lua Views and declarations are generated separately by editor **Export** or explicit `ScriptTools ui-assets generate <project-root>`; builds do not generate them. The UI generator itself needs no desktop preview. The [Declarative UI Authoring Workflow](<../Lua and Blueprint Scripting/Declarative UI/Authoring Workflow.md>) describes generated and handwritten ownership.

Preview targets, artefacts and cross-build registries are described in [PreviewHost: Build, Publish and Troubleshooting](<../PreviewHost/Build Publish and Troubleshooting.md>). The shared renderer and editor/game boundary are covered in [Architecture and Data Flow](<../PreviewHost/Architecture and Data Flow.md>).

### Shared format and rendering constants

`ScriptTools/runtime_formats.py` owns the `.ldpak` and encrypted-payload layouts, versions, binary data tags and codec limits; `ScriptTools/resource_constants.py` owns resource groups, paths and suffixes. CMake runs `ScriptTools runtime-constants cpp <project-root> <output-directory>` to generate neutral C++ headers under `Intermediate/<build-name>-<hash>/RuntimeConstants/LudorkGenerated`. Native consumers link `Ludork::RuntimeConstants`; implementation layers never include a parent host's private headers to obtain these values. Engine exposes the four data and animation suffixes through `ResourceFileConstants`.

`Assets/Shaders/Global/UnoccludedLightPass.frag` contains the sole `LUDORK_MAX_SHADER_LIGHTS` definition: one decimal integer in `1..256`, currently `64`. Generation rejects missing, repeated or invalid definitions. Changing this limit requires rebuilding the native project so the shader, native renderer and read-only `GlobalCore.Light.MAX_SHADER_LIGHTS` agree. Ordinary shader reload alone does not update the compiled limit.

Generation runs for game, desktop Preview and `LUDORK_WITH_LUA=OFF` builds. C++ Source templates retain the shader source and build wiring; their normal build regenerates the headers. Generated headers are build outputs and are not edited or shipped as template sources.

### LDC binary data

`Runtime/Data.hpp` owns `loadData`, `saveData`, `dataExists`, `decodeData` and the binary payload reader `parseBinaryData`; file APIs accept `std::filesystem::path` or UTF-8 strings. Lua exposes the four data-file APIs on `Engine`; `parseBinaryData` is a C++ payload-level API. `Runtime/Json.hpp` retains exact-path JSON text operations. Standard owns file access, path resolution and payload compression/obfuscation; it does not depend on Runtime or interpret the binary value tree.

LDC retains the `LDDC` envelope, version `1`, zlib compression, stream obfuscation and checksum. Source length and checksum describe the uncompressed binary payload. A one-byte tag precedes each value:

| Tag | Value and payload |
|---|---|
| `0`, `1`, `2` | Null, false and true, with no following payload. |
| `3` | Little-endian signed 64-bit integer. |
| `4` | Little-endian finite IEEE 754 binary64 number. |
| `5` | Little-endian uint32 byte length, followed by string bytes. |
| `6` | Little-endian uint32 count, followed by tagged array values. |
| `7` | Little-endian uint32 count, followed by string keys and tagged values. Keys use the string length/bytes encoding without a tag and are written in UTF-8 byte order. |

Empty arrays and maps remain distinct. The decoded payload is limited to 512 MiB and nesting to 256. Invalid tags, truncation, invalid lengths, duplicate keys, non-finite numbers and trailing bytes fail decoding. Functions, objects, cycles and reference restoration are outside this pure-data format. Old LDC payloads containing JSON text are rejected without fallback or migration; regenerate them from source data. Shader payloads keep their separate format.

### GPU particle ownership

Core keeps the public `Emitter`, `EmitterComponent`, `EmitterView` and Scene scheduling interfaces. Its `EmitterConfiguration` contains `EmitterTrack` values, each combining Runtime's `EmitterTrackParameters` with Core's eight named `EmitterCurves` channels of `Curve::CurveData`. `setConfiguration` centrally validates these values, samples the existing `Curve` implementation into a 256 × 8 float lookup table per track and restarts playback. `Runtime/Graphics/GpuEmitterCurveLayout.hpp` owns this layout: 256 samples across eight channels, packed into two RGBA rows. CPU baking, GPU texture allocation and generated shader coordinates use the same sample counts and indexing. `getConfiguration` returns an independent value copy. The existing ParticleSystem remains in Core.

Within the Emitter path, `RuntimeData` is confined to resource JSON input in Core's `EmitterResource.hpp/.cpp`. The resource adapter maps authored string choices, 0–255 colours, arrays and curve references to the fixed configuration fields. Emitter stores that configuration value; the backend stores its compiled configuration.

`Runtime/Graphics/GpuEmitterConfiguration.hpp` carries Runtime track parameters and baked curve samples without Core types. The unbound `GpuEmitterBackend` in `Runtime/Graphics/GpuEmitterBackend.hpp` owns playback state, fixed simulation steps, GPU tracks, drawing, asynchronous statistics and deferred GPU release through a private implementation. It accepts resolved configuration and does not interpret `Data/Particles` keys or depend on Core Curve, Actor or UI classes. `EmitterStatistics` is a Runtime value record with required `capacity`/`time` and optional renderer, count and timing results. Core returns it directly; the Lua binding writes a declared-field snapshot table, preserving unavailable optional values as `nil`.

The public Core `Emitter` forwards playback operations to this backend. Configuration, track, curves, burst and statistics records keep separate class-named headers, and their Lua registrations and declarations are generated through the normal binding build under `Engine`. Runtime has no additional Lua root.

GPU implementation files live under `Runtime/src/Graphics/Emitters`. Raw OpenGL functions, shaders, buffer/query identifiers and private GPU headers stay there. Texture reads reuse Runtime AssetStore and AssetInputStream, including the existing loose and packed layouts. Runtime depends on Standard and SFML; Core includes only Runtime's public graphics headers.

The Runtime Graphics CMake subdirectory compiles this backend once in both full and `LUDORK_WITH_LUA=OFF` builds.

Shared GPU resources are keyed by the active SFML context ID. Shutdown queues GPU objects without requiring a context on the caller's thread; collection runs with the matching resource context active. Scene and preview hosts retain ownership of windows/render targets and collect retired resources before destroying those contexts. This GPU lifetime is independent of the Lua VM lifetime.

### Session and module lifetime

Standard owns the Lua session. `runtime::initialize` requires a running Standard session, and repeated calls preserve the attached Runtime state. After `runtime::shutdown` detaches Runtime, a restart requires a new Standard session and Lua VM.

Standard shuts down modules in reverse dependency order (Global, Engine, Runtime) before releasing its state. The host owns the Lua VM, and it must wait for shutdown to finish on the thread that initiated it before calling `lua_close`. If LuaSF initialisation or binding registration fails before Standard starts, the host must still call `LuaSF_quiesce_state` and `LuaSF_shutdown_state` before `lua_close` to release any partially initialised glue session.

AssetStore, DataStore, ScriptStore and the Standard read-only file provider are host configuration. They are independent of the VM session and survive session shutdown.

Standard establishes a LuaGlue execution scope at each host Lua entry. Nested glue operations may reuse that scope only while the host confirms that the current thread still owns the VM lock and the execution hooks are unchanged. `LuaExecutionPause` suspends that ownership; operations during a pause must acquire the lock again. Hosts without an ownership probe continue to use the ordinary enter/leave hooks.

Standard's C++ header `LuaInvocation.hpp` provides `ludork::standard::ensureLuaStack(state, count, context)` and `invokeLuaFunction(state, callable, arguments, context)` for use within that VM ownership scope. The stack check reserves space for `count` additional values; a count above `INT_MAX` raises `std::length_error`, and a failed stack expansion raises `std::runtime_error`. Invocation accepts a `lua_glue::Object` callable and a `std::vector<lua_glue::Object>` of arguments, checks capacity, pushes them and makes a protected call. Success leaves every result above the original stack top and returns their count; the caller consumes those results. Any exception restores the entry stack top before propagating. The separate `class_runtime::invoke(state, callable, argumentCount)` interface consumes arguments already on the Lua stack.

### Host and implementation sources

Each independent class or bound type uses a same-named header and, where needed, a source file. Public declarations belong under `include`, and member definitions under `src`. Owner-specific data types may be nested. Shared data and helper collections may stay together.

Host-private implementation stays under `src` and uses an `Impl` suffix, such as `HostImpl.hpp` for `Host::Impl`. Larger implementations may use a same-named subdirectory and contribute sources through `target_sources`. Dependencies flow from the host into its implementation. Child directories must not include parent-private headers, including through include-root shortcuts, and public headers must not include private headers.

Core shares SpriteVisuals, joystick device tracking and text configuration codecs through `src/Shared/include/CoreShared`. Their implementation and state remain under `src/Shared/src`; consumers include only the interface headers. Host companion headers stay beside their host source and are not shared interfaces.

Application platform implementations share neutral interfaces under `Application/src/Platform/include`. The application host depends on those interfaces and applies platform results to runtime launch options. Platform implementations do not include host-private headers or own the host’s startup-error reporting.

Windows editor and game launchers compile the shared source under `Engine/PlatformHosts/Common`; `WindowsLauncher.hpp` exposes `RunLauncher` with a relative executable path and `LauncherWorkingDirectory` policy. The editor launches `Binaries\Ludork.exe` with the launcher directory as its working directory. The game launches `Binaries\Main.exe` while inheriting the caller's working directory. Both forward arguments, standard streams and the child exit status. C++ Source templates retain this shared source.

Shared Runtime Lua services live in `Runtime/src/LuaServices`. Pure ConfigVars parsing lives in `Runtime/src/Metadata`.

Standard ClassRuntime dependencies flow `Class → Composite → Instance → Native → Detail`, with downward shortcuts allowed. Each layer owns a distinct concern:

- Detail owns neutral state and queries.
- Native owns native members and properties.
- Instance owns native construction and disposal.
- Composite owns indexing and metatables.
- Class coordinates allocation, finalisation and super.

The boundary checker covers private headers in Engine and Application, including parent-private headers reached through include search roots. It also checks Standard layer direction and cycles, and rejects static `Source.*` project Lua module paths or type references in first-party Engine C++ strings and binding annotations. This static check does not trace dynamically constructed paths; generic native services receive project module names from their callers.

System privately composes Display, FramePipeline, SceneStack and Lifecycle. Display owns the window. FramePipeline owns the canvas, effects and transition targets. SceneStack owns queued operations and retired scenes. Lifecycle owns shutdown state. Components expose operations instead of mutable state aggregates. Frame presentation remains outside the scene logic lock, and render targets rebuild between frames.

Each Scene logic worker creates and releases its own shared graphics context on that thread. The context stays alive throughout the logic loop, so rebuilding a Camera render texture on the render thread cannot destroy the logic worker's active context. The scene logic lock continues to serialise access to shared render textures.

`EngineState::CellSize` is a positive C++ compile-time constant. Changing it requires a rebuild of the engine, the editor and the templates. There is no project-level or runtime override.

### Native text input boundary

Engine owns the edit session through `Input/TextInputService.hpp`. Platform hosts implement `Input/TextInputHost.hpp`. The contract uses UTF-8, byte offsets at grapheme boundaries and caret rectangles in window-client pixels. Hosts return session-tagged events through a thread-safe sink. Service operations run on the game thread or before it starts. Standard provides Unicode segmentation and whitespace handling through utf8proc.

### Native WebView boundary

Runtime owns the native web-page service and the `Engine.OpenWebView(url)` binding. `Runtime/WebViewHost.hpp` defines the platform host factory, window attachment and session-tagged completion; `Runtime/WebViewService.hpp` supplies input blocking. Factories can be installed before VM initialisation. Display attaches a host to the live native window and detaches it before replacement or destruction. Each open request has a new ID; the host navigates its existing layer, and a late completion for an older request cannot close the current session. A rejected request preserves the existing page and session.

Platform hosts run UI work asynchronously and do not wait in the game loop or call Lua from completion. macOS and iOS use WKWebView, Android the system WebView, HarmonyOS ArkWeb, and Windows the system Evergreen WebView2 Runtime.

The HarmonyOS overlay root uses `HitTestMode.BLOCK_HIERARCHY`: its Web and Close button receive touch events, while lower siblings and ancestors do not. The game XComponent stays mounted beneath the overlay.

### Core headers and Runtime boundary

Core Lua-binding headers include `<CoreMinimal.hpp>` from `Engine/Runtime/include`. That header supplies annotations, runtime types, common C++ types and the SFML Graphics, Window and System modules. Include module exports and direct type dependencies explicitly. Every public header must compile independently.

Handwritten Core, including private sources, reaches VM and binding services through Runtime without direct or transitive LuaGlue dependencies. Runtime also owns isolated audio execution, while Core retains gameplay and playback policy. Generated bindings may use LuaGlue. Runtime internals use private operations within an active `RuntimeScope`. Facades provide the external entry boundary.

Choose API types, container ownership and Lua-reference operations using [Runtime Value Boundaries](<Runtime Value Boundaries.md>).

### Native pointer casts

`<CoreMinimal.hpp>` includes `<Cast.hpp>`; lower-level headers may include it directly. Use `ludork::Cast<T>(pointer)` for checked native downcasts and cross-casts after [declaring the cast protocol](<Macro Reference.md#native-cast-declarations>). `T` is the target class, without `*`. Known upcasts may use implicit conversion or `static_cast`; unchecked `static_cast` must not replace a dynamic type check.

Raw and shared pointers are supported. Null or mismatched inputs return empty results; constness and multiple-inheritance subobject offsets are preserved. Successful shared-pointer casts alias the original control block, and the rvalue overload transfers ownership only on success. There is no reference or `void` target overload.

Queries and dynamic type identification compare compiler-generated type-signature strings by content across modules. These keys are internal to a complete build with the same toolchain, not persistent IDs or a cross-toolchain ABI.

### Build the project

```bash
./tools/build_cpp.sh Game Debug
./tools/build_cpp.sh Game Release
```

```text
tools\build_cpp.bat Game Debug
tools\build_cpp.bat Game Release
```

In the editor, **Construct** runs the Debug build for a C++ Source project. After a direct CMake build, run an incremental `build_cpp` to update the build record used by **Play**. Changes to third-party sources or external CMake settings also require **Construct** or `build_cpp`. See [Run, Debug and Package](<../Editor User Guide/Run Debug and Package.md>) for the editor workflow.

`LUDORK_OPTIMIZE_DEBUG` defaults to `ON` and controls Debug optimisation. LuaGlue checks argument types and bounds in every configuration.

First-party C++ must not use `dynamic_cast`, `std::dynamic_pointer_cast` or `typeid`. `Engine/cmake/LudorkCppOptions.cmake` disables RTTI for C++ and Objective-C++ in every configuration with `/GR-` on MSVC and `-fno-rtti` on GCC, Clang and AppleClang. This covers game, Preview, Lua-disabled builds, source-built dependencies. LuaGlue identifies types without RTTI; exception handling stays enabled. Rebuild all native modules together after changing this policy. Imported libraries and system frameworks retain their own settings.

Release builds enable IPO/LTO for Standard, its separate ClassRuntime OBJECT target and the final mobile application target, alongside Runtime and Core. MinGW keeps IPO disabled. ClassRuntime uses hidden symbols and hidden inline symbols, while explicit public exports remain available. On Apple platforms, the game application, Standard, Runtime and Core use `-dead_strip` for their final Release links. MSVC keeps separate PDB files and `/OPT:REF /OPT:ICF`; Android and HarmonyOS use their toolchain's section garbage collection. These Release settings do not change Debug optimisation or the public Lua API. Keep the matching unstripped build outputs and any generated debug-symbol files for diagnosing a release; [packaging](<../Editor User Guide/Run Debug and Package.md#packaging>) strips only the platform's final distribution output.

On macOS, single-arm64 AppleClang Release builds compile the generated Core binding sources with `-Os`. Handwritten runtime sources and other platforms, architectures and configurations retain their existing optimisation levels.

LuaSF is a binder only: it owns neither the Lua VM nor its dependencies. The game runtime creates the `lua_State` with `luaL_newstate` and `luaL_openlibs`, hands it to LuaSF with `LuaSF_initialize_state` and `LuaSF_register`, and tears it down through `ludork::standard::shutdown` and `lua_close`.

CMake reads LuaSF, LuaGlue, SFML and Lua from their directories under `Game/Engine/ThirdParty`. Override them with `-DLUDORK_LUASF_SOURCE_DIR`, `-DLUDORK_LUAGLUE_SOURCE_DIR`, `-DLUDORK_SFML_SOURCE_DIR` and `-DLUDORK_LUA_SOURCE_DIR`. LuaGlue defaults to the sibling of the selected LuaSF source directory, so local generator integration uses `-DLUDORK_LUASF_SOURCE_DIR=/path/to/LuaSF-AutoGenerator/output/LuaSF`. Dependency initialisation installs the two projects from one source archive and writes matching `.ludork-version` markers.

Dependency initialization also installs IXWebSocket 12.1.0 at `Engine/ThirdParty/IXWebSocket`, pinned by `versions.conf` to commit `bf7ad53c02612643e1cf72e37c8749491798d33d`. `Engine/cmake/LudorkIXWebSocket.cmake` provides the static `ixwebsocket::ixwebsocket` target for GlobalCore's optional WebSocket transport. TLS, message compression, demos, tests and installation are disabled in that dependency's scope without changing parent build options. Source templates and mobile packaging retain its source; packages include `Licenses/IXWebSocket/LICENSE.txt` and `THIRD-PARTY.txt`. The client accepts an `http://` configuration and derives its `ws://` endpoint.

SFML is a required source project rather than an optional override, and LuaSF reuses the `sfml-*` targets Ludork builds from it. Mobile uses static SFML; desktop follows `LUASF_BUILD_SHARED_SFML`, defaulting to shared. `versions.conf` selects the SFML variant by repository and tag; the current pin is the SFML-ME `310-ME-OH-GLESVER` tag. OHOS GPU particles require an SFML source that preserves the GLES 3 context request; see [Common Particles](<../Editor User Guide/Common Particles.md>).

Local source archives can be selected with `LUASF_SOURCE_ARCHIVE` before dependency initialisation: use a `.tar.gz` with the shell entry point or a `.zip` on Windows. The archive must extract directly to `LuaSF/` and `LuaGlue/`; older source layouts are rejected. Both projects consume one Lua 5.5 runtime. Desktop builds share LuaGlue; mobile targets link it statically.

CMake runs `ScriptTools core-bindgen` automatically. See [Generated Metadata and Stubs](<Generated Metadata and Stubs.md>) for output locations, enum modules and cross-build requirements.

`Intermediate/` is excluded from templates and recreated by a normal build when missing. `bin/` and `build/` remain at the project root.

iOS uses a single-window `UIScene` host. The application scene manifest selects its scene delegate. The host associates its window with a `UIWindowScene` and starts the runtime once that scene is active. A temporary scene disconnection pauses the live runtime and retains its window for reconnection within the same session. This supports SDKs that require the scene lifecycle while the application and optional FFmpeg build retain an iOS 16.3 deployment target.

HarmonyOS packaging is an Apple Silicon macOS cross-build through DevEco Studio's OpenHarmony native SDK. Each package recreates its Stage and native build directories, so a CMake cache cannot cross device forms or graphics backends. The HAP target and the compatible SDK are both `6.0.2(22)`, and CMake receives `OHOS_COMPATIBLE_SDK_VERSION=22`. The toolchain therefore compiles Ludork, Lua, LuaSF, LuaGlue and SFML for `aarch64-linux-ohos22.0.0` with API availability diagnostics enabled. A newer installed SDK serves as the compile SDK only and does not raise that deployment target.

The Mobile variant configures `SFML_HARMONY_DEVICE_FORM=MOBILE` and `SFML_OPENGL_ES=ON`, advertises `phone` and `tablet`, and uses the fixed-landscape mobile host. The 2in1 variants configure `SFML_HARMONY_DEVICE_FORM=2IN1` and advertise only `2in1`. They support `fullscreen` and `floating` windows, publish `NEED_OPENGL=1`, request `ohos.permission.LOCK_WINDOW_CURSOR`, and set `SFML_OPENGL_ES=OFF` for OpenGL or `ON` for OpenGL ES. An FFmpeg-enabled HarmonyOS build keeps the same versioned compiler target for C and C++ compilation and linking, while retaining the OpenHarmony sysroot, MUSL selection and availability checks.

A 2in1 OpenGL binary has a hard dependency on `libGLv4.so`. If the image lacks that library, the dynamic loader rejects the NAPI module before native initialisation. The binary does not switch to OpenGL ES.

Android packaging is a cross-build from Apple Silicon macOS. The packer invokes host CMake 3.28 or newer with `Unix Makefiles`, the highest complete stable NDK r27-or-newer toolchain found under the local SDK's `ndk` directory, `ANDROID_PLATFORM=android-24`, `ANDROID_ABI=arm64-v8a` and `ANDROID_STL=c++_static`. The project never contains its own SDK or NDK. Lua, LuaSF, LuaGlue, Runtime, Core, cjson, SFML and optional FFmpeg targets are static. The only shared output is `libludork.so`, which Gradle consumes as a prebuilt `jniLibs` entry. Android Studio's SDK CMake and Ninja are not used.

### Load the runtime modules

```lua
local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")

local Components = GlobalFunctions.Components
local UI = GlobalFunctions.UI
local NodeGraph = GlobalFunctions.NodeGraph
local Manager = GlobalFunctions.Manager
```

Load these three roots in the displayed order. `GlobalFunctions` groups are accessed through their tables and must not leak into the module root. Standard globals are already installed. `CoreSystem` is not a runtime module.

## Related pages

- [Binding a Class](<Binding a Class.md>)
- [Functions, Events and Execution](<Functions Events and Execution.md>)
