# Gameplay and Scripting

## When should I use Blueprint, Lua or a Script Mixin?

Blueprint graphs define event and execution flow visually. Lua classes provide reusable systems and gameplay types, while Script Mixins attach Lua behaviour directly to a Blueprint class. A Blueprint inheritance chain must consistently use either graph mode or Script Mixin mode. See [Lua Runtime and Modules](<../Lua and Blueprint Scripting/Lua Runtime and Modules.md#lua-runtime-and-modules>).

## Can I reload Lua without restarting the game?

For an editor-launched game using loose Lua sources, save your edits and enter `$r` in **Console**. Hot reload updates existing functions while preserving the scene, objects and runtime state. It does not rerun `Entry.lua` or reload JSON, UI assets, metadata, declarations or native code; structural or data changes require a restart. Packed or bytecode-only scripts cannot be hot-reloaded. See [Lua hot reload](<../Lua and Blueprint Scripting/Lua Runtime and Modules.md#lua-hot-reload>).
