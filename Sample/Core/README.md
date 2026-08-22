# Ludork native runtime modules

`Sample/Core` contains the C++20 implementation and binding declarations for the Sample runtime.

Supported release targets require Windows 10 or newer on x64, macOS 13.3 or newer on Apple Silicon, iOS 15.0 or newer on arm64, HarmonyOS 6.0.1 / API 21 or newer on arm64-v8a, or Android 7.0 / API 24 or newer on arm64-v8a.

## Modules and dependency direction

| Module | Owns | Native dependency direction |
|---|---|---|
| `CoreSystem` | low-level system utilities | independent module target; zlib |
| `Engine` | engine types, services and state | Core binding/Standard, SFML and zlib |
| `GlobalCore` | global gameplay classes and services | Engine, Standard and SFML; optional platform/FFmpeg libraries |
| `GlobalFunctions` | Components, UI, NodeGraph and Manager free functions | GlobalCore and SFML |

Lua must load them in order:

```lua
local CoreSystem = require("CoreSystem")
local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")

local Components = GlobalFunctions.Components
local UI = GlobalFunctions.UI
local NodeGraph = GlobalFunctions.NodeGraph
local Manager = GlobalFunctions.Manager
```

This load order is a runtime contract; it does not mean the Engine target links to CoreSystem. No lower module links back to `GlobalFunctions`.

Headers declare public binding intent with the macros in `include/BindAnnotations.hpp`. CMake invokes the standalone `ScriptTools` executable with the `core-bindgen` command, whose implementation lives under the repository's `ScriptTools/core_bindgen` directory. It produces one stable `<Module>.<NativeClass>.auto.cpp` registration unit per native class and exactly one `<Module>.stub.auto.cpp`, which is both the module aggregate and generated-stub writer; there is no separate `<Module>.auto.cpp`. It also produces the module-level `Scripts/stub/<Module>.d.lua` and root `*_meta.lua` files. Every generated declaration starts with `---@meta <Module>` so EmmyLua resolves it from the dedicated stub module root. Generated files must not be edited manually.

Bindgen writes outputs only when their content changes, so adding or removing a class creates or deletes only its stable class unit and leaves unrelated class objects reusable. Classes declared together in one public header still share that ordinary C++ dependency and may recompile together when the header changes.

A file-level `BIND_FUNCTION_GROUP(name = "...")` puts every `BIND_FUNCTION` in that header only into the named table. Ungrouped functions remain at the module root. Cross-module signatures include the complete declaration they require; generic conversions belong in the self-contained feature headers under `include/LudorkCoreBinding/`, and bindgen includes only the features required by each generated module.

Build from the repository root with `tools/build_cpp.bat Sample Debug` or `./tools/build_cpp.sh Sample Debug`. See [Build and Module Layout](<../../docs/en_GB/04.Native C++ Development/01.Build and Module Layout.md>) for class, property, method, event, function-group, metadata and troubleshooting examples.
