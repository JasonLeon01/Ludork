# Ludork native runtime library

`Sample/Runtime` is the native foundation shared by the Core modules and editor preview host. It owns `RuntimeValue`, opaque Lua identities, VM session access, reflection, metadata lookup, fixed provider slots, binding codecs and shared runtime caches.

The CMake target is `LudorkRuntime`, exposed as `Ludork::Runtime`. Desktop builds produce a shared `LudorkRuntime` library; iOS, HarmonyOS and Android use the same sources as a static library.

Runtime depends on Standard, LuaSF and SFML. It must not include or link Engine, GlobalCore or GlobalFunctions. Engine links Runtime publicly and supplies Engine-specific lifecycle, Blueprint, NodeGraph, component and resource adapters.

Runtime is not a Lua module. Runtime-owned `BIND_*` declarations are included in Engine binding generation and remain available only through `require("Engine")`.
