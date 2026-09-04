# Ludork Core modules

`Sample/Core` contains the C++20 Engine, GlobalCore and GlobalFunctions modules. Shared runtime values, Lua-session services and binding infrastructure live in the sibling `Sample/Runtime` library.

Supported release targets require Windows 10 or newer on x64, macOS 13.3 or newer on Apple Silicon, iOS 15.0 or newer on arm64, HarmonyOS 6.0.2 / API 22 or newer on arm64-v8a, or Android 7.0 / API 24 or newer on arm64-v8a.

## Modules and dependency direction

| Module | Owns | Native dependency direction |
|---|---|---|
| `Engine` | engine types, services and state | LudorkRuntime and SFML |
| `GlobalCore` | global gameplay classes and services | Engine, Standard and SFML; optional platform/FFmpeg libraries |
| `GlobalFunctions` | Components, UI, NodeGraph and Manager free functions | GlobalCore and SFML |

Standard owns the low-level filesystem services, Base64 and shared zlib byte helpers used by its Lua globals and Engine animation compression.

Lua must load them in order:

```lua
local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")

local Components = GlobalFunctions.Components
local UI = GlobalFunctions.UI
local NodeGraph = GlobalFunctions.NodeGraph
local Manager = GlobalFunctions.Manager
```

This load order is a runtime contract. Standard globals are installed before the entry script, and there is no `CoreSystem` module. No lower module links back to `GlobalFunctions`.

Headers declare public binding intent with `LudorkRuntimeBinding/Annotations.hpp`. Engine binding generation scans both `Runtime/include/Runtime` and `Core/Engine/include`, so Runtime-owned native values continue to register on the Engine Lua module rather than creating another Lua root. CMake invokes the standalone `ScriptTools` executable with the `core-bindgen` command, whose implementation lives under the repository's `ScriptTools/core_bindgen` directory. It produces one stable `<Module>.<NativeClass>.auto.cpp` registration unit per native class and exactly one `<Module>.stub.auto.cpp`, which is both the module aggregate and generated-stub writer; there is no separate `<Module>.auto.cpp`. It also produces `<Module>.traits.auto.hpp`, the module-level `Scripts/stub/<Module>.d.lua` and root `*_meta.lua` files. The traits header aggregates compile-time dynamic-value and opaque-identity selection from `BIND_CLASS`; CMake privately force-includes it in the corresponding complete Core module. Every generated declaration starts with `---@meta <Module>` so EmmyLua resolves it from the dedicated stub module root. Generated files must not be edited manually.

Bindgen writes outputs only when their content changes, so adding or removing a class creates or deletes only its stable class unit and leaves unrelated class objects reusable. Classes declared together in one public header still share that ordinary C++ dependency and may recompile together when the header changes.

A file-level `BIND_FUNCTION_GROUP(name = "...")` puts every `BIND_FUNCTION` in that header only into the named table. Ungrouped functions remain at the module root. Cross-module signatures include the complete declaration they require; generic conversions belong in the self-contained feature headers under `Runtime/include/LudorkRuntimeBinding/`, and bindgen includes only the features required by each generated module.

`BIND_ENUM` exposes an `enum class` once at the owning module root, emits its LuaLS type and uses integer values at the Lua and Blueprint boundaries. Do not mirror the same enumerators through module properties.

## Host and implementation boundaries

A `.cpp` that implements a public bound class is its host. Simple hosts do not need a same-name implementation directory. When one exists, the host is the only source allowed to define that class's `Host::...` members, and dependencies point only from the host down. Host-only state, ports, algorithms and runtime services below the directory must not include the host header, name the host type, use `../` includes or call back into the host contract. The host owns cross-component orchestration and passes explicit data or lower-level dependencies to those files.

Lua- and Blueprint-facing declarations belong in the public outer header. A bound class or companion must not live under its same-name implementation directory behind an include-only outer header. Private implementation headers expose only neutral implementation contracts.

An independently bound companion remains its own host. Add a child `CMakeLists.txt` only for a real target boundary with an independent build contract; Engine and GlobalCore already discover ordinary implementation sources recursively. CMake's `ImplBoundaryValidate` target discovers host/same-name-directory boundaries and invokes `.tools/ScriptTools/ScriptTools impl-boundary-check Sample` (or the Windows `.exe` path). Lua child modules with mirrored `.d.lua` declarations are excluded as independent hosts. The same validation runs in native CI, with no separate manifest.

Build from the repository root with `tools/build_cpp.bat Sample Debug` or `./tools/build_cpp.sh Sample Debug`. See [Build and Module Layout](<../../docs/en_GB/04.Native C++ Development/01.Build and Module Layout.md>) for class, property, method, event, function-group, metadata and troubleshooting examples.
