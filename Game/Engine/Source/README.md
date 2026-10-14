# Ludork Core modules

`Game/Engine/Source` contains the C++20 gameplay modules and video library. Shared values, resources, VM/binding services and Blueprint execution live in [Runtime](../Runtime/README.md).

## Modules and dependency direction

| Module | Owns | Native dependency direction |
|---|---|---|
| `Engine` | engine types, services, state and game-specific Runtime connections | LudorkRuntime and SFML |
| `GlobalCore` | global gameplay classes and services | Engine, Standard, SFML and LudorkVideo; optional platform libraries |
| `GlobalFunctions` | Components, UI, NodeGraph and Manager free functions | GlobalCore and SFML |
| `LudorkVideo` | video decoding, seeking and audio extraction | LudorkRuntime, SFML and optional FFmpeg |

Standard owns low-level filesystem, Base64 and zlib helpers. `LudorkVideo` is a static library with no Lua root.

Lua must load them in order:

```lua
local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
```

This order is required. Standard globals are installed before the entry script; function groups are accessed as `GlobalFunctions.Components`, `.UI`, `.NodeGraph` and `.Manager`. Lower modules must not link back to `GlobalFunctions`.

Declare public bindings with `BIND_*` annotations. CMake runs `ScriptTools core-bindgen` to generate C++ registrations, Lua declarations and editor metadata; Runtime bindings belong to `Engine`. Do not edit generated files. See [Binding a Class](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=04%2F02), [Macro Reference](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=04%2F06) and [Generated Metadata and Stubs](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=04%2F07).

## Host and implementation boundaries

- Give each independent class or bound type its own header and any required source file. Public headers must compile independently and include direct dependencies.
- Keep host-private implementation under `src` with an `Impl` suffix. Dependencies point from hosts into implementation layers; child directories must not include parent-private headers, and public headers must not include private headers.
- Keep handwritten Core access to VM/binding services behind Runtime. RTTI is disabled; use `ludork::Cast<T>` for checked native casts.
- CMake's `ImplBoundaryValidate` target and native CI run `ScriptTools impl-boundary-check Game`.

Build from the repository root with `tools/build_cpp.bat Game Debug` or `./tools/build_cpp.sh Game Debug`. See [Build and Module Layout](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=04%2F01) for supported platforms, source layout and build settings.
