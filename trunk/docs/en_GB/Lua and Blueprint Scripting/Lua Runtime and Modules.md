# Lua Runtime and Modules

Ludork runs Lua 5.5 with Standard extensions, LuaSF and project-native modules. This page covers project scripting architecture, module boundaries, hot reload, error handling and workspace declarations.

New to Lua? Start with [Lua Basics](<../Getting Started/Lua Basics.md>), then read [Ludork Lua Advanced](<../Getting Started/Ludork Lua Advanced.md>) for Ludork language additions (`class`, `bool`, `list`, `dict`, copying, filesystem helpers and `asyncio`).

| Model | Best suited to | Primary source |
|---|---|---|
| Blueprint graph | Visual event and execution flow | `Data/Blueprints/*.json` |
| Lua class | Reusable systems and gameplay types | `Scripts/**/*.lua` |
| Script Mixin | Lua behaviour attached directly to a Blueprint class | `Scripts/Mixins/**/*.lua` plus same-name `_meta.lua` |
| Declarative UI | JSON assets and generated typed View trees | `Data/UI/**/*.json`, generated `Scripts/Internal/UI/**/*.lua` and matching stubs |
| UI behaviour | Handwritten foundations and business Controllers | `Scripts/Internal/UIBase`; private Controllers inside window modules; independent row and Scene `Controller.lua` modules |

> **Important:** A Blueprint inheritance chain uses either graph mode or Script Mixin mode; do not switch modes within one chain.

## Entry and module order

Native startup runs the script passed as the first command-line argument, or `Scripts/Entry.lua` when no argument is supplied. Startup does not read a project-local configuration or `script` setting to choose the entry file.

Load the root native modules in the order below, finish direct `require` calls, then declare local aliases:

```lua
local Engine = require("Engine")
Engine.setAppName("MyGame")

local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Logging = require("Global.Utils.Logging")

local Input = Engine.Input
local System = GlobalCore.System
local ManagerFunctions = GlobalFunctions.Manager
```

### Module boundaries and loading rules

- **Native vs. Lua `GlobalFunctions`**: The native `GlobalFunctions` module exposes `Components`, `UI`, `NodeGraph` and `Manager`. Lua Blueprint function libraries under `Scripts/GlobalFunctions` are separate modules loaded by full path (such as `require("GlobalFunctions.Math")`) and are not members of the native root table.
- **Blueprint function library restrictions**: Modules under `Scripts/GlobalFunctions` must not call `require` at module scope (including for native modules); resolve dependencies inside functions on first use so libraries load without cycles. Their paths and exported names must not collide with C++ `GlobalFunctions` bindings. Run `ludork-check global-functions <repository-root>` to validate these rules statically; it also flags malformed strings, long comments and numeric literals with file, line and column.
- **No aggregators or ad-hoc lazy imports**: Load project modules by their full dotted path and do not add directory-level `init.lua` aggregators. Outside Blueprint function libraries, use direct top-level `require` calls; defer a `require` only to break a genuine dependency cycle or at an explicit first-use boundary (such as an optional window's factory) whose lifecycle is owned by the enclosing scene or feature.
- **Layer dependency direction**: Static dependencies flow from root native modules through `Scripts/Global` to `Scripts/Source`. `Global` implementations and declarations must not require `Source` or reference its types (including via project-aware `GlobalFunctions` libraries); the native root `GlobalFunctions` is a lower-layer API, while `Scripts/Internal` and `Scripts/GlobalFunctions` may depend on `Source`.
- **Injecting project behaviour into Global**: Generic infrastructure may resolve opaque module names supplied by data, but must not hard-code higher-layer business modules. Inject project callbacks and class objects at startup via [`Global.Data.InitializeRuntime(options)`](<Default Gameplay/Runtime Data Configuration and Saves.md#data-services>).
- **Shared constants**: Native resource suffixes are exposed through `Engine.ResourceFileConstants`; map identities and Gameplay tags live in `Source.Configs.MapConstants` and `Source.Configs.GameplayConstants`. Public enums live in `Enums.*`.
- **Classes and encapsulation**: Keep each reusable class in its own module that returns the class. A window module may define a private Controller and export the UI through `Ui.DefineWindow` (see [Declarative UI Authoring Workflow](<Declarative UI/Authoring Workflow.md>)); utility modules may group related helpers. Access private members only through `self` or same-instance `super(..., self)` dispatch — use public getters, commands or data for all other objects, including chained and dynamic access.

### Script and Data storage boundaries

- **Script root layout**: The game root uses one script storage form: loose `Scripts/` during development or a root `Scripts.ldpak` in a packed release. Mixed, missing or invalid layouts fail startup. Once `Entry` calls `Engine.setAppName`, loose files in the user data directory override either layout per [runtime resource lookup](<../Editor User Guide/Project Settings and Asset Layout.md#runtime-resource-lookup>).
- **Module resolution and Lua loaders**: Every Lua state exposes project modules through `package.preload` and a dynamic searcher without shadowing native modules. New user-directory modules load on first `require`; modules already cached in `package.loaded` do not re-execute merely because their file changed. Script Mixins load fresh on every execution, and audio states use the same effective script sources. `loadfile` and `dofile` accept `/Game/Scripts/...` with standard return, error and environment semantics alongside ordinary physical paths. General filesystem APIs do not expose script archive contents.
- **Data lookup**: Loose and packed `Data` resolve through logical `Data/...` and `/Game/Data/...` paths. `Engine.loadData` and `Engine.dataExists` fall back from a missing `.json` path to a binary `.ldc` alternative within each source, giving user data priority over root data. JSON platform APIs read only the exact path. This is a read-only runtime boundary, not a general archive filesystem.

## Enum modules

Each public enum lives in its own module under `Scripts/Enums`, keeping its constants, EmmyLua annotations and API documentation together in the source file without a mirrored stub. A handwritten enum declares a local table of string keys and scalar literals and returns it, with no dependencies, functions or class construction:

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

Load an enum with `local StairDirection = require("Enums.StairDirection")` and read `StairDirection.Up`. Blueprint metadata references it via `type = { enum = "Enums.StairDirection" }`; see [Enum schemas](<Blueprint Scripting/Metadata Schema and Decorators.md#enum-schemas>).

| Game enum module | Values or purpose |
|---|---|
| `Enums.StairDirection` | `None`, `Up`, `Down` (stored as the same strings) |
| `Enums.BattleResultCode` | `WIN = 1`, `CANNOT_DAMAGE = 2`, `LETHAL_COUNTER_DAMAGE = 3` |
| `Enums.CriticalResultCode` | `VALUE = 1`, `NOT_NEEDED = 2`, `UNKNOWN = 3` |
| `Enums.DamageHintLevel` | `NONE = 0`, `BATTLE = 1`, `MAP = 2` |
| `Enums.WindowTransitionProfile` | `DEFAULT = "Default"`, `MENU = "Menu"` |
| `Enums.ShopMode` | `BUY = "buy"`, `SELL = "sell"` |
| `Enums.EventKey` | `LocaleChanged`, `AbilitySystemChanged`, `PlayerChanged` event names |
| `Enums.AbilitySystemChangeKind` | `Attribute`, `State` payload kinds |
| `Enums.PlayerChangeKind` | `Inventory`, `Name`, `Map` payload kinds |
| `Enums.MovementLatentOutput` | `STARTED = 0`, `FINISHED = 1` execution output indices |
| `Enums.CombatResolutionCode` | Intermediate [combat resolution results](<Source Classes/Gameplay.md#motabattleability>) |
| `Enums.MovementResultCode` | [Movement-hazard query and commit results](<Source Classes/Gameplay.md#movementspecials-and-movement-danger>) |
| `Enums.PlayerNameValidationError` | [Player name validation errors](<Source Classes/Player.md#lua-runtime-api>) |
| `Enums.SaveLoadMode`, `Enums.SaveLoadCloseReason` | [Save/load window mode and close reason](<Default Gameplay/Runtime Data Configuration and Saves.md#saves>) |
| `Enums.WorldActorState` | World Actor lifecycle labels: `NEVER_ACTIVE = "NeverActive"`, `ACTIVE = "Active"`, `DORMANT = "Dormant"` |
| `Enums.WorldPublishPhase` | Region publication labels: `CONVERT = "convert"`, `BUILD = "build"`, `INDEX = "index"`, `FINALISE = "finalise"` |

Native builds generate `Enums.Engine.*`, `Enums.GlobalCore.*`, `Enums.GlobalFunctions.*` and `Enums.sf.*`. Saving project data generates `Enums.GeneralDataKey`, one `Enums.GeneralData.<TypeName>` module per data type, `Enums.Animation` and `Enums.Particle`. The editor resolves these project-managed string enums directly from the current catalogue (including unsaved edits), while runtime `require` loads the generated modules. Treat the owning native declarations or database records as the source of truth: edit those sources and regenerate rather than editing generated enum files directly.

## Native modules and values

Standard globals such as `os.createDirectories`, `os.removeFile`, `zlib` and `base64` are host-installed and require no `require`. The host also sets the `SAVE_AS_LDC` default from `LUDORK_SAVE_AS_LDC`; see [Saves](<Default Gameplay/Runtime Data Configuration and Saves.md#saves>) for project overrides. `LudorkRuntime` is a native library rather than a Lua module — Core bindings load only through `Engine`, `GlobalCore` and `GlobalFunctions`, with no `Runtime` or `CoreSystem` alias.

LuaSF registers the global `sf`. Pass exact native values such as `sf.Vector2i.new(...)`, `sf.Vector2f.new(...)` and `sf.Color.new(...)` whenever an API expects them; native containers and plain Lua tables do not implicitly convert to SFML vectors.

## Asynchronous operation updates

The default `Scripts/Entry.lua` calls `GlobalFunctions.NodeGraph.initLatent()` to drive asynchronous operation updates. Custom entry scripts must also call it, even for script-only gameplay. See [Cooperative tasks and file batches](<../Getting Started/Ludork Lua Advanced.md#cooperative-tasks-and-file-batches>) and [Execution Flow, Events and Variables](<Blueprint Scripting/Execution Flow Events and Variables.md>).

## Session lifetime

The main thread and all coroutines within one Lua VM share a single native session. Captured callbacks and opaque handles survive coroutine yields, cannot cross VMs, and become invalid when the session shuts down. See [Session and module lifetime](<../Native C++ Development/Build and Module Layout.md#session-and-module-lifetime>) for native host integration.

## Live Debug

Live Debug edits Actor state inside an editor-launched game, including Actors spawned at runtime. Edits are transient and are neither saved to project data nor reapplied when instances rebuild. See [Live Debug](<../Editor User Guide/Run Debug and Package.md#live-debug>) for the editor controls.

## Lua hot reload

In an editor-launched Debug or Release game running from loose `Scripts/` sources, save your edits and enter `$r` in the Console. On the next Console update (between gameplay callbacks), the runtime updates loaded project modules and used Script Mixins while keeping the active scene, live objects and Lua VM intact. Packed or compiled-only scripts do not support hot reload.

### What updates and what requires a restart

- **Updated in place**: Existing named functions are replaced while preserving module and class identities, instance fields, module data, constants, defaults and captured non-function values. Cached references to matched functions (including native callbacks) redirect to the new functions, and ordinary global accesses may be added or removed.
- **Unchanged until recreated**: Existing anonymous callbacks and suspended coroutine frames keep their original bodies (though calls to matched helpers update). Constructors and lifecycle callbacks do not rerun. Existing [audio processors](<Default Gameplay/Resources Audio Video and Effects.md#audio>) keep their code until a new playback starts.
- **Requires a restart**: Changing class defaults, deleting a loaded module or used Script Mixin, changing export names, class bases, field-vs-function kinds, native callback members, incompatible captured state, or using an unmatchable custom `_ENV`.
- **Scope and exclusions**: The runtime compiles all effective Lua sources (including user-directory overrides) before applying changes. `Entry` is checked but not rerun, and new or unloaded modules load on demand via `require`. Metadata, declarations, JSON data, UI assets and native code are excluded.

> **Warning:** Syntax, compatibility or candidate-execution failures leave existing definitions intact and log an error in the Console, but external side effects from candidate execution cannot be rolled back. Restart the game after any error that may have modified native resources or shared objects. Keep module top levels restricted to definitions and dependencies, and place resource creation and subscriptions in explicit startup or lifecycle functions (see [Entry and modules](<Default Gameplay/Default Template Overview.md#entry-and-modules>)).

## Logging and errors

Log runtime diagnostics through `Global.Utils.Logging`: `debug` for diagnostic detail, `info` for major stages, `warning` for recoverable data issues and `error` for unrecoverable failures. Logging never raises an exception; call `error` or `assert` when execution must stop.

- **Scheduled callback isolation**: When a scheduled Scene input/frame callback, Actor frame callback or UI input/frame/animation-completion callback raises a Lua error, the engine aborts that invocation and prints the callback name and Lua stack trace to the Console. Synchronous calls inside the failing callback unwind with it, while state changes made before the error stay applied. Other scheduled callbacks continue, and repeating callbacks can run on their next turn. A failing mouse-button-down callback leaves the input unconsumed.
- **Fatal startup errors and async tasks**: An error escaping the entry script or required initialisation aborts launch. See [Cooperative tasks and file batches](<../Getting Started/Ludork Lua Advanced.md#cooperative-tasks-and-file-batches>) and [Scene timers](<Global and Core Modules/Core Modules/GlobalCore/Scenes and System.md#scene-timers>) for their error rules.
- **Runtime providers**: Engine providers register explicitly during [startup](<Default Gameplay/Default Template Overview.md#entry-and-modules>), never as a side effect of `require`. Missing or duplicate providers, stopped sessions and invalid return counts or types raise errors; returning zero values differs from returning one `nil`.

## Runtime, declarations and metadata

Lua files are divided by ownership:

- **`.lua` runtime modules**: Contain runtime behaviour and implementation-only annotations. Short pure constant tables and simple configuration tables keep their public annotations and documentation directly in the source file (like pure-data enums), without mirrored stubs. This exception does not apply to short modules with business methods or lifecycle behaviour.
- **`Scripts/stub/**/*.d.lua` declarations**: Contain all other public documentation and LuaLS declarations, with no runtime side effects or `require` calls. Type-only declarations with no runtime implementation live here even when short. Native API enum types live in their owning declarations, such as `Engine.d.lua` and `LuaSF.d.lua`.
- **`*_meta.lua` metadata**: Contain pure editor metadata and never execute as gameplay scripts.

A requireable declaration mirrors its runtime module's path and begins with `---@meta <module.path>` (for example, `Global/Pool.d.lua` declares `---@meta Global.Pool`). Global-only declarations (`Standard.d.lua`, `Class.d.lua`, `LuaSF.d.lua`) and declaration-only `Scripts/stub/Source/Configs/EventChangePayload.d.lua` use a bare `---@meta`. Source-owned declarations omit `---@meta`. The language server reads declarations directly from the workspace without runtime `require`.

The EmmyLua workspace configures `Scripts/stub` as its workspace root and library. In `.emmyrc.json`, `workspace.moduleMap` maps `Scripts/Enums` modules to `Enums.*` and maps the exact source-owned modules (`Global.WorldMapConstants`, `Source.Configs.HotKey`, `Source.Configs.Tutorial`, `Source.Configs.MapConstants` and `Source.Configs.GameplayConstants`) directly to their runtime import paths, while all other public imports resolve to their stub declarations:

```json
"moduleMap": [
    {
        "pattern": "^Scripts[.]Enums[.](.*)$",
        "replace": "Enums.$1"
    },
    {
        "pattern": "^Scripts[.](Global[.]WorldMapConstants|Source[.]Configs[.](HotKey|Tutorial|MapConstants|GameplayConstants))$",
        "replace": "$1"
    }
]
```

See [Project Templates](<../Getting Started/Project Templates.md>) for which declarations templates and game packages ship.

Standalone and C++ Source templates include the Windows VS Code launch configuration `Ludork: EmmyLua Attach (Windows)` for attaching `tangzx.emmylua`'s `emmylua_attach` debugger to a running `Main.exe` (Windows only). C++ Source templates also include native `cppvsdbg` and `lldb` launch and attach configurations.
