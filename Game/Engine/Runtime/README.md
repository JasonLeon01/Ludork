# Ludork native runtime library

`Game/Engine/Runtime` provides shared values, resources, graphics, native WebView, VM/binding services and Blueprint execution for Core modules and editor previews. Its CMake target is `LudorkRuntime` (`Ludork::Runtime`): shared on desktop and static on iOS, HarmonyOS and Android.

Runtime depends on Standard, SFML and private zlib; it must not include or link Engine, GlobalCore or GlobalFunctions. Engine supplies gameplay components, Actor graph lookup, EventBus and frame-loop integration. Hosts disconnect callbacks before Runtime clears session work and providers.

- `LUDORK_WITH_LUA=ON` (default): includes Lua, LuaSF, LuaGlue, VM, binding and Blueprint services; required for game builds.
- `LUDORK_WITH_LUA=OFF`: the editor preview subset, with values, resources, graphics and native WebView but no Lua or LuaSF linkage.

Runtime has no separate Lua module. Its `BIND_*` declarations are generated into `Engine` and accessed through `require("Engine")`.

Build from the repository root with `tools/build_cpp.bat Game Debug` or `./tools/build_cpp.sh Game Debug`. See [Build and Module Layout](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=04%2F01) for dependency, session and platform-host contracts, and [Runtime Value Boundaries](https://jasonleon01.github.io/Ludork/docs/v1.0/?lang=en_GB&doc=04%2F08) for value ownership and conversion.
