# Lua Runtime and Modules

Ludork runs Lua 5.5 with Standard extensions, LuaSF and project-native modules.

Readers new to Lua can begin with [Lua Basics](<../Getting Started/Lua Basics.md>). Ludork-native language additions such as `class`, `bool`, `list`, `dict`, copying, filesystem helpers and `asyncio` are described in [Ludork Lua Advanced](<../Getting Started/Ludork Lua Advanced.md>). This section covers project scripting architecture rather than language syntax.

| Model | Best suited to | Primary source |
|---|---|---|
| Blueprint graph | Visual event and execution flow | `Data/Blueprints/*.json` |
| Lua class | Reusable systems and gameplay types | `Scripts/**/*.lua` |
| Script Mixin | Lua behaviour attached directly to a Blueprint class | `Scripts/Mixins/**/*.lua` plus same-name `_meta.lua` |
| Declarative UI | JSON assets and generated typed View trees | `Data/UI/**/*.json`, generated `Scripts/Internal/UI/**/*.lua` and matching stubs |
| UI behaviour | Handwritten foundations and business Controllers | `Scripts/Internal/UIBase`; private Controllers inside window modules; independent row and Scene `Controller.lua` modules |

A Blueprint inheritance chain uses either graph mode or Script Mixin mode. Do not switch modes within one chain.

## Entry and module order

`Main.ini` selects the entry script, normally `Scripts/Entry.lua`. Load the root native modules in the order shown below, finish the direct `require` calls, then declare aliases:

```lua
local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Logging = require("Global.Utils.Logging")

local Input = Engine.Input
local System = GlobalCore.System
local ManagerFunctions = GlobalFunctions.Manager
```

The native `GlobalFunctions` module exposes `Components`, `UI`, `NodeGraph` and `Manager`. Lua Blueprint function libraries under `Scripts/GlobalFunctions` are separate modules, loaded with paths such as `require("GlobalFunctions.Math")`; they are not members of the native root table. Other modules use their full dotted path. Do not add directory-level `init.lua` aggregators. Outside Blueprint function libraries, delay a `require` to resolve a real dependency cycle, or at an explicit first-use boundary such as an optional window's factory. The owning scene or feature must define that lifecycle; ordinary dependencies remain direct imports.

Runtime modules under `Scripts/GlobalFunctions` do not call `require` at module scope, including for native modules. They resolve dependencies from functions on first use so Blueprint function libraries can load without cycles. Their Lua paths and exported names must not occupy the same `GlobalFunctions` paths as C++ bindings.

Static dependencies flow from the root native modules through `Scripts/Global` to `Scripts/Source`. Global implementations and declarations must not require Source or reference its types, including through project-aware Lua libraries under `GlobalFunctions`. The native root `GlobalFunctions` remains a lower-layer API. `Scripts/Internal` and `Scripts/GlobalFunctions` can depend on project modules in `Source`. Generic infrastructure can resolve opaque module names supplied by data, but it must not hard-code a higher-layer business module. Project startup supplies callbacks and class objects when shared services need project behaviour; see [`Global.Data.InitializeRuntime(options)`](<Default Gameplay/Runtime Data Configuration and Saves.md#data-services>).

Native resource suffixes are exposed through `Engine.ResourceFileConstants`; game map identities and Gameplay tags live in `Source.Configs.MapConstants` and `Source.Configs.GameplayConstants`. Public enums use the independent `Enums.*` modules described below.

At runtime, scripts use exactly one storage form: loose `Scripts` during development, or a root `Scripts.ldpak` in a packed release. Mixed, missing or invalid layouts fail during startup. Logical module and entry paths are identical in both forms.

Every Lua state exposes project modules through `package.preload` without replacing native modules. Script Mixins still load fresh on every execution. Standard filesystem APIs do not expose archive contents.

Packed Data remains available through the same logical `Data/...` paths. `Engine.loadData` and `Engine.dataExists` resolve missing `.json` paths to binary `.ldc` alternatives; JSON platform APIs use only the exact path. This is a read-only runtime boundary, not a general archive filesystem.

Each independently reusable class has its own module, and that module returns the class. A window module may contain its private Controller and export the UI through `Ui.DefineWindow`; see the [Declarative UI Authoring Workflow](<Declarative UI/Authoring Workflow.md>). Utility modules may group related helpers.

Access private members only through `self` or same-instance `super(..., self)` dispatch. For other objects, including chained and dynamic access, use public getters, commands or data.

## Enum modules

Each public enum has its own module beneath `Scripts/Enums`. Its constants, EmmyLua annotations and API documentation stay together in the source file, without a mirrored enum stub. A handwritten enum declares a local table of string keys and scalar literals, then returns that table. It has no dependencies, functions or class construction:

```lua
-- Scripts/Enums/StairDirection.lua
---@enum Enums.StairDirection
local StairDirection = {
    None = "None",
    Up = "Up",
    Down = "Down",
}

return StairDirection
```

Load an enum directly, for example `local StairDirection = require("Enums.StairDirection")`, then use `StairDirection.Up`. Blueprint metadata references it with `type = { enum = "Enums.StairDirection" }`; see [Metadata Schema and Decorators](<Blueprint Scripting/Metadata Schema and Decorators.md>) for the full rules.

| Game enum module | Values or purpose |
|---|---|
| `Enums.StairDirection` | `None`, `Up`, `Down`, stored as the same strings |
| `Enums.BattleResultCode` | `WIN = 1`, `CANNOT_DAMAGE = 2`, `LETHAL_COUNTER_DAMAGE = 3` |
| `Enums.CriticalResultCode` | `VALUE = 1`, `NOT_NEEDED = 2`, `UNKNOWN = 3` |
| `Enums.DamageHintLevel` | `NONE = 0`, `BATTLE = 1`, `MAP = 2` |
| `Enums.WindowTransitionProfile` | `DEFAULT = "Default"`, `MENU = "Menu"` |
| `Enums.ShopMode` | `BUY = "buy"`, `SELL = "sell"` |
| `Enums.EventKey` | `LocaleChanged`, `AbilitySystemChanged`, `PlayerChanged` event names |
| `Enums.AbilitySystemChangeKind` | `Attribute`, `State` payload kinds |
| `Enums.PlayerChangeKind` | `Inventory`, `Name`, `Map` payload kinds |
| `Enums.MovementLatentOutput` | `STARTED = 0`, `FINISHED = 1` execution output indices |

Native builds generate `Enums.Engine.*`, `Enums.GlobalCore.*`, `Enums.GlobalFunctions.*` and `Enums.sf.*`. Project data saves generate `Enums.GeneralDataKey`, one `Enums.GeneralData.<TypeName>` module per data type, `Enums.Animation` and `Enums.Particle`. The editor resolves these string enums from the current project catalogue before saving, including unsaved changes; runtime `require` reads the generated source modules. These generated modules retain their owning native declarations or database records as the source of truth; edit those sources and regenerate.

## Native modules and values

Standard globals such as `os.createDirectories`, `os.removeFile`, `zlib` and `base64` are host-installed and are not required. The host also installs the `SAVE_AS_LDC` default from `LUDORK_SAVE_AS_LDC`; project overrides are described in [Saves](<Default Gameplay/Runtime Data Configuration and Saves.md#saves>). `LudorkRuntime` is a native library, not a Lua module. Core bindings load only through `Engine`, `GlobalCore` and `GlobalFunctions`, and there is no `Runtime` or `CoreSystem` alias.

LuaSF registers the global `sf`. Use exact native values such as `sf.Vector2i.new(...)`, `sf.Vector2f.new(...)` and `sf.Color.new(...)` when an API declares those types. Native containers and ordinary tables are not implicit vector adapters.

## Asynchronous operation updates

The default `Scripts/Entry.lua` calls `GlobalFunctions.NodeGraph.initLatent()` to enable operation updates. Custom entry points must also call it, including for script-only gameplay. See [Lua waiting examples](<../Getting Started/Ludork Lua Advanced.md#cooperative-tasks-and-file-batches>) and [Blueprint execution](<Blueprint Scripting/Execution Flow Events and Variables.md>).

## Session lifetime

The main thread and coroutines in one Lua VM share a native session. Captured callbacks and opaque values survive yields but cannot cross VMs, and session shutdown invalidates them. Native host integration is covered in [Session and module lifetime](<../Native C++ Development/Build and Module Layout.md#session-and-module-lifetime>).

## Live Debug

Live Debug lets you change Actor state in an editor-launched game, including Actors created at runtime. Changes are not saved to project data or reapplied when instances are rebuilt. See [Live Debug](<../Editor User Guide/Run Debug and Package.md#live-debug>) for the controls.

## Lua hot reload

In an editor-launched game that uses loose Lua sources under `Scripts`, save the changes and enter `$r` in the Console. At the next Console update, between gameplay callbacks, the runtime updates loaded project modules and used Script Mixins while keeping the current scene, objects and Lua VM. Both Debug and Release builds support the command. Packed or compiled-only scripts do not.

Reload replaces existing named functions while preserving module and class identities, instance fields, module data, constants, defaults and captured non-function values. Changing a function body takes effect. Changing a class default requires a restart. Cached references to matched functions, including native callbacks, are redirected to new function objects. Existing anonymous callbacks and suspended coroutine frames keep their original bodies, although references to matched helpers are updated. Constructors and lifecycle callbacks do not run again.

Loaded modules and used Script Mixins must retain their export names, class bases, field and function kinds, native callback members and compatible captured state. Deleting one of these modules, or making an incompatible structural change, requires a restart. Adding or removing ordinary global accesses is supported. An unmatchable custom `_ENV` requires a restart.

The runtime compiles all Lua sources before it applies changes. Entry is checked but not rerun. New and unloaded modules remain available on demand through `require`. Metadata, declarations, JSON data, UI assets and native code are excluded. Existing [audio processors](<Default Gameplay/Resources Audio Video and Effects.md#audio>) keep their code until a new playback starts.

Syntax, compatibility or candidate-execution failures leave the existing definitions in place and report an error in the Console. External side effects from candidate execution cannot be rolled back, so restart after an error that may have changed native resources or shared objects. Keep module top levels to definitions and dependencies, and put resource creation and subscriptions in explicit startup or lifecycle functions. See the [Game entry sequence](<Default Gameplay/Default Template Overview.md#entry-and-modules>).

## Logging and errors

Runtime Lua logs through `Global.Utils.Logging`: `debug` for diagnostic detail, `info` for significant stages, `warning` for recoverable data problems and `error` for unrecoverable failures. Logging does not raise. Use `error` or `assert` when execution must stop.

Engine providers are installed explicitly during [startup](<Default Gameplay/Default Template Overview.md#entry-and-modules>), not by `require`. Missing or duplicate providers, stopped sessions and invalid return counts or types raise errors. Returning no values differs from returning one `nil`.

## Runtime, declarations and metadata

Lua files have separate owners:

- `.lua` files contain runtime behaviour and implementation-only annotations. Pure-data enum modules also carry their public type annotations and documentation.
- Mirrored `Scripts/stub/**/*.d.lua` files contain other public documentation and LuaLS declarations, with no runtime side effects or `require`. Native API enum types remain in their owning declarations, such as `Engine.d.lua` and `LuaSF.d.lua`.
- Same-name `_meta.lua` files contain pure editor metadata and never run as gameplay scripts.

A requireable declaration uses `---@meta` with its module path; for example, `Global/Pool.d.lua` declares `---@meta Global.Pool`. Global-only `Standard.d.lua`, `Class.d.lua` and `LuaSF.d.lua` use a bare `---@meta`.

The EmmyLua workspace keeps `Scripts/stub` as its module root and library. In `.emmyrc.json`, `workspace.moduleMap` maps `^Scripts[.]Enums[.](.*)$` to `Enums.$1`, allowing enum imports to resolve to their annotated sources while other public imports retain their stub declarations. Enum sources do not use `---@meta`.

See [Project Templates](<../Getting Started/Project Templates.md>) for which declarations templates and game packages include.

Standalone and C++ Source templates include a Windows VS Code configuration named `Ludork: EmmyLua Attach (Windows)` that attaches the `tangzx.emmylua` `emmylua_attach` debugger to a running `Main.exe`. C++ Source templates also keep the native `cppvsdbg` and `lldb` launch and attach configurations. The EmmyLua attach debugger supports Windows only.

## Related pages

- [Lua Basics](<../Getting Started/Lua Basics.md>)
- [Ludork Lua Advanced](<../Getting Started/Ludork Lua Advanced.md>)
- [Metadata Schema and Decorators](<Blueprint Scripting/Metadata Schema and Decorators.md>)
- [Default Template Overview](<Default Gameplay/Default Template Overview.md>)
