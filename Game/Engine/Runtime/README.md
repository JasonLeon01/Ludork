# Ludork native runtime library

`Game/Engine/Runtime` is the native foundation shared by Core modules and the editor preview host. It owns `RuntimeData`, `RuntimeHandle`, `RuntimeValue`, VM sessions, reflection, metadata and binding services. Blueprint class generation, Script Mixins, graph execution, latent scheduling, component data, typed-value conversion and JSON also live here.

The CMake target is `LudorkRuntime`, exposed as `Ludork::Runtime`. Desktop builds produce a shared `LudorkRuntime` library; iOS, HarmonyOS and Android use the same sources as a static library.

Runtime depends on Standard and SFML. The default `LUDORK_WITH_LUA=ON` mode also links LuaSF and builds VM, binding and Blueprint services. The editor-owned preview entry sets the option to `OFF`, selecting value, resource, graphics and native WebView services; this mode does not link Lua or LuaSF. It must not include or link Engine, GlobalCore or GlobalFunctions. Engine links Runtime publicly and supplies Actor graph lookup, EventBus and frame-loop connections, gameplay components and resource adapters. Hosts disconnect their callbacks before Runtime clears pending work, class/default resolvers, caches and providers.

Runtime is not a Lua module. Runtime-owned `BIND_*` declarations are included in Engine binding generation and remain available only through `require("Engine")`.

Runtime also owns `Engine.OpenWebView(url)` and the native WebView host contract. Platform UI runs above the game in its own layer while Runtime supplies session identity and input blocking. Desktop and Apple implementations live under `src/WebView`; Android and HarmonyOS register their platform-shell factories through `Runtime/WebViewHost.hpp`. WebView2 uses the installed Evergreen Runtime, and Apple links the system WebKit framework. No browser engine is packaged with Ludork.
