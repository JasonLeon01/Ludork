# Runtime Data, Configuration and Saves

This page documents Default Gameplay's configuration files, data-service bootstrap contract, runtime state and persistence model, composite-world save records and `.json`/`.ldc` save formats.

## Configuration

`Data/Configs` stores project configuration JSON, while mutable user preferences live in the [platform game configuration](<../../Editor User Guide/Project Settings and Asset Layout.md#game-configuration>). Use the configuration APIs and editor controls so unknown sections and keys survive round trips.

> **Note:** Optional [remote account dictionaries](<../../Simple Server/Native Client API.md>) are an independent service and do not automatically upload, download or overwrite local saves; gameplay code explicitly chooses what to read and write.

### Paths, constants and keys

- **Config resource vs. data references**: Resource-valued `file` fields store complete `/Game/Assets/...` paths (`base` and `ext` constrain only the editor selector). In `System.json`, `startPlayerBlueprint` is relative to `Data/Blueprints` (`root = "Data"`, resolving to `Data.Blueprints.*`) and `startRegion` names the starting region; both apply to new games only, as saves keep their stored player class and region.
- **Engine constants and Lua config modules**: Grid cell size is compiled into the engine (`32` by default; read with `Engine.GetCellSize()`, not configured in `System.json`). Lua config modules live under `Scripts/Source/Configs` (`Source.Configs.*`). `Source.Locale.Core.DEFAULT_LANGUAGE` (`en_GB`) is the shared initial and fallback language (also used by `Source.Configs.Main`), `Source.Configs.MapConstants.WORLD_MANIFEST_FILE` is `"_world.json"`, and data readers/saves use suffixes from `Engine.ResourceFileConstants`.
- **Initial data keys**: Keys are paths relative to their category directory using `/` separators with only the final extension stripped, preserving internal dots (e.g. `folder.v1/item.v2.json` becomes `folder.v1/item.v2`). Animation keys strip the full `.anim.json` cache suffix.
- **User-data directories**: `Engine.getUserDataPath(appNameOverride?)`, `Engine.getMainConfigPath(appNameOverride?)` and `Engine.getSavePath(appNameOverride?)` resolve the user-data root, config file and `Save/` directory after `Entry` calls `Engine.setAppName(APP_NAME)`. Passing `appNameOverride` queries another application name without altering the active one. All launch modes obey the same [directory rules](<../../Editor User Guide/Project Settings and Asset Layout.md#game-configuration>); `LUDORK_EDITOR` does not redirect them.
- **Loading user config**: `Source.Configs.Main.LoadOrCreate()` returns the user configuration path and `ConfigParser`, creating platform defaults if absent and raising an error on unreadable or malformed plist files rather than overwriting them. Entry script selection is independent of user preferences (see [Entry and module order](<../Lua Runtime and Modules.md#entry-and-module-order>)).
- **Android paths**: Android mirrors immutable runtime assets into `filesDir/ludork/runtime/<runtime-hash>` before native startup and writes mutable configuration and saves to `filesDir/ludork/user-data/<APP_NAME>` without modifying the runtime directory or requiring external-storage permission.

### Shared Gameplay configuration

`Data/Configs/Gameplay.json` stores project-wide overrides for shared script defaults. Edit them in [Database → System Config → Gameplay](<../../Editor User Guide/Project Settings and Asset Layout.md#system-config>).

| Key | Type | Script default | Effect |
|---|---|---|---|
| `enemyDamageHintLevel` | `Enums.DamageHintLevel` | `BATTLE` (`1`) | Sets `EnemyDamageText.EnemyDamageHintLevel`: `NONE` (`0`) hides expected-damage hints, `BATTLE` shows enemy readouts, and `MAP` (`2`) also shows movement-danger cell totals. Owning the required Item gates hints; actual damage particles are unaffected. |
| `enemyDefeatShatterEffectEnabled` | boolean | `true` | Sets `Enemy.DefeatShatterEffectEnabled`, toggling the [enemy-defeat shatter effect](<Resources Audio Video and Effects.md#shaders-fog-and-transitions>). |
| `hideDisconnectedRegions` | boolean | `false` | Sets `GameMap.HideDisconnectedRegions`, enabling the [tile mask for disconnected regions](<../Global and Core Modules/Global APIs/GameMap.md#disconnected-regions>) on newly constructed ordinary maps. |
| `enableShopRecord` | boolean | `true` | Sets `GameInstance.EnableShopRecord`. Disabling stops new shop records and the [visited-shop shortcut](<Windows Menus Input and Controls.md#visited-shops>) without deleting saved records. |
| `maxSaveSlots` | positive integer | `100` | Sets `WindowSaveSlot.MAX_SAVE_SLOTS` for the slot list and quick-save, quick-load and latest-slot queries. Lowering it hides higher slots without deleting their files. |
| `tutorialShrinkDuration` | finite positive number (seconds) | `0.25` | Sets `WindowTutorial.SHRINK_DURATION`, the tutorial mask contraction time. |

- **JSON override schema**: Fields use string `type` values (`"int"`, `"float"`, `"bool"`), `nullable: true` to permit script defaults, and `enum: "Enums.DamageHintLevel"` for enum choices. Overrides store under `value` with no separate `useDefault` flag; new Gameplay files default every override to `null`.
- **`null` vs. concrete overrides**: `null` selects **Use default** (leaving the script default active without assigning `nil`), whereas explicit `false` and `0` are concrete overrides (`0` selects `NONE` for damage hints, but fails validation for positive numeric fields). Actor and map defaults (such as movement speed or `GameMap.MapViewRect`) stay on their class or Blueprint properties.
- **`scriptDefault` source reference**: Every nullable field requires `scriptDefault: { module, field }` pointing to a Lua file under `Scripts` and a `table.field` assignment (such as `Controller.MAX_SAVE_SLOTS`). The editor parses one unconditional top-level assignment of scalar literals, imported enum members or simple scalar arithmetic without executing Lua; missing files or unsupported expressions keep **Use default** selected and report an error.
- **Startup application and runtime overrides**: `Source.System.Init()` calls `Source.Configs.Gameplay.Apply()`, which validates every value and fails startup on invalid types, enum values or ranges. JSON edits take effect after saving and restarting (not via Lua hot reload). Scripts can toggle `GameInstance.EnableShopRecord` directly at runtime without erasing recorded shops.

## Data services

`Global.Data` owns shared data loading, resource queries, Blueprint classes and graphs, Actor construction and static previews; require it directly. `Source.Data` owns project startup, default General Data queries (such as `GetGeneralPlayerData` and `GetGeneralItemData`) and `CreateGeneralAttributeSet(typeName, memberID)`.

`Source.Data.InitializeRuntime()` invokes `Global.Data.InitializeRuntime(options)` once per Lua session (a second call raises an error; requiring either module never initialises it). Custom projects must pass all four `Global.Data.RuntimeOptions` fields from startup:

| Required option | Type and purpose |
|---|---|
| `configResolver` | `(configName: string, settingName: string) -> string`; resolves config references for empty string overrides. |
| `nodeModuleCandidates` | `(prefix: string) -> string[]`; extra node module paths in search order (the unprefixed path is tried first). |
| `conditionalActorBase` | `Class.ClassType<Engine.Actor>`; Actor base whose subclasses expose `conditionVariable`, `conditionOperator` and `conditionValue` in previews. |
| `childActorComponentBase` | `Class.ClassType<Engine.Component>`; component base whose subclasses supply `className` and `relativePosition` through `childActorComp` in previews. |

Pass finalised class objects (not module-name strings or instances) satisfying `Class.isSubclass` against `Engine.Actor` and `Engine.Component`. Default Gameplay supplies `Source.System.GetConfigValue`, the ordered `Source.` and `Global.` node candidates, `Source.MapActors.ConditionalActor` and `Source.Components.ChildActorComponent`, keeping `Global` independent of `Source`. Configuration provider registration is handled separately by `Source.System.InstallRuntimeProviders()`.

## Runtime state

Keep static definitions in Blueprint, Config and General Data files, and mutable session state on `GameInstance`, Actors and components — avoid writing transient state back to source JSON.

- `Global.Data.GetClassData(path)` returns the shared session definition; see [Class definitions and session lifetime](<../Blueprint Scripting/Execution Flow Events and Variables.md#class-definitions-and-session-lifetime>).
- Construct General Data Attribute Sets with `Source.Configs.GeneralDataTypes.Create`, using type keys from `Enums.GeneralDataKey` and member keys from `Enums.GeneralData.<TypeName>` (e.g. `Enums.GeneralData.Item`). The editor resolves these from live data and regenerates their Lua modules alongside `Enums.Animation` and `Enums.Particle` on save; see [General Data and Text Config](<../../Editor User Guide/General Data and Text Config.md>).

### Static Actor display data

`Global.Data.GetActorPreviewData(classPath, classVarChanges?)` returns `Global.Data.ActorPreviewData` (or `nil` for an empty path; invalid paths and non-`Engine.Actor` classes raise an error):

- **Resolution**: Merges inherited Blueprint and Script Mixin defaults, then applies declared display and condition overrides using typed conversion. Unknown overrides fail; unrelated gameplay values and `BlueprintOnly` overrides are skipped.
- **Returned record**: Contains texture and material paths, first-frame `rect`, `translation`, rotation (degrees), `scale`, `origin`, `visible`, `materialParameters`, condition fields and optional recursive `child` with `childOffset` (from declarative `childActorComp`; cycles raise an error). Every call returns independent copies of tables and mutable `sf` vectors/rectangles so shared templates cannot be mutated.
- **No gameplay side effects**: Extraction neither constructs gameplay Actors, instantiates graphs, loads textures, runs component/lifecycle callbacks nor initialises combat state. Call `Global.Data.ClearActorPreviewCache()` to discard cached class display templates without affecting shared class definitions (clearing the floor-preview cache calls it automatically).

### Players

- **Party storage and order**: `GameInstance` keeps ordered keys in `_playerKeys` and objects in `_players`, keyed by the non-empty, unique Player General Data key (`Player.ID`). `getPlayers()` returns the live dictionary; `getPlayerKeys()` returns the live order (its first key is the primary player returned by `getPlayer()`). `getPlayerByIndex()` resolves 0-based indices; `setPlayer(playerKey)` and `setPlayerByClass(playerClass)` promote an existing member to primary; `addPlayerByClass(playerClass, mapPath, position)` appends a member with its required map/cell; `removePlayerByClass` also removes that key's stored map.
- **Player serialisation**: Saves mirror `playerKeys` and `players` (every key must match exactly one entry with the same restored `Player.ID`), plus a required `customName` string alongside `attr` (`""` selects the localised General Data default; non-empty values must be pre-trimmed and satisfy [Source.MapActors.Player](<../Source Classes/Player.md>)).
- **Persisted attributes and inventory**: Each Player saves finite numeric Attribute Set Base values under `attr` via `getNumericAttributeBases()` (matching the generated `int`/`float` schema; missing keys in older saves keep the initial Base, derived Current values are not saved, and non-numeric fields rebuild from Player General Data), along with inventory/equipment counts, equipped IDs and State stacks.
- **Load rebuild order**: Loading restores Base values first, then Item abilities, equipment Effects and State Effects without running initial-equipment or member graphs. Handles, derived Current values, transient Effects, ability specs and tags are not saved — custom persistent additions require their own record and rebuild logic.

### Game variables

Initial game variables load from generated `Scripts/Source/Configs/GameVariables.lua` (save edits in the [Game Variable Manager](<../../Editor User Guide/General Data and Text Config.md#game-variable-manager>) before launching). List and dictionary initial values are ordinary Lua tables rather than Standard containers, and empty tables round-tripped through JSON may not preserve array-vs-object shape.

- **New game vs. save load**: `GameInstance:init` deep-copies the generated map so each new game starts independently (Default Gameplay declares `MTExample = 0` and `ShopPrice = 20`; runtime code can add dynamic keys, though only editor-declared keys appear in `InstVar` selectors). Loading a save replaces the variable table wholesale — generated initial values apply to new games only, and adding, removing or retyping a declaration does not migrate existing saves (retyping resets only the project initial value).
- **Monitored fields**: Snapshots capture logical values while `deepcopy` strips monitor proxies and subscriptions so `ConditionalActor` listeners stay local to the live session. On load, `onCreate` resubscribes and recomputes visibility without extra save fields.

### Map Actor identity

- **Map tags**: Map JSON `tag` becomes the Actor's map-persistence tag read by `Actor:getMapTag()` — `GameMap:getActorByTag` and `GameInstance` persistence use only this tag, keeping `Actor.tag` independent. Untagged Actors are not persisted, and map paths normalise via `Global.Utils.MapPath.Normalise`.
- **Added and destroyed pairs**: Runtime-spawned Actors that must survive map rebuilds save as added-Actor records with generated placement tags; destroying them writes a destroyed record that suppresses the addition (`Reborn` enemies use this same mechanism instead of reusing the original tag).
- **Composite-world tags**: World runtime tags use `<child-file-stem>.runtime_default_<N>` inside placements and `<world-directory>.runtime_default_<N>` in holes; all tags stay reserved for the world's lifetime and conflicts raise errors. Editor tags are unique world-wide and survive map renames.
- **Typed overrides**: Map `BPClassVarChanged` and persisted `classVarChanges` store values only for declared class fields resolved from metadata, Script Mixins and `attrDefs`. Unknown names or schema mismatches fail loading; enum overrides store scalar values, and vectors/records apply typed conversion.

### Composite-world persistence

- **Manifest identity**: The primary player's `map` entry stores the `_world.json` manifest path even when entry targeted a child map. Player, added, moved and destroyed records use manifest identity, world coordinates and global tags; conflicting added or moved records fail loading.
- **Restoring Actors**: Added Actors spawn at `actorPositions[tag]` if present (otherwise their original position); destroyed tags are skipped. Placement Actors restore with their child without distant child I/O, while hole Actors can restore immediately.
- **Moved roots (`worldMovedActors`)**: Required in a save whenever any player's stored map is a world manifest. Tracks authored roots that left their definition child (identity, owner, layer overrides, world position), rebuilding them only at their current owner; returning home or destroying removes the record. Moved snapshots preserve reconstruction identity, `classVarChanges`, layer and position, but not arbitrary Lua fields or timer progress (persist those separately or recover elapsed time in `onWorldWake`).
- **Suppression and terrain**: Destroying a tag removes its moved record and suppresses authored, added, resident and in-progress instances without loading absent children (tags stay reserved; invalid records/collisions fail). Terrain edits store child paths and local coordinates, applying when that child builds and updating only its live layer (holes cannot own terrain edits).

## Saves

The Default Gameplay save schema is version `1` (numeric `1`). `SaveCodec` accepts only the current structure and version, rejecting incompatible saves without migration. Schema versions are independent of application versions and the packaging dev checkbox: use `vN-dev` while developing a breaking format change and freeze it as `vN` before release.

> **Note:** [Live Debug](<../../Editor User Guide/Run Debug and Package.md#live-debug>) never edits project files or save schemas, though saving during a debug session persists gameplay state altered by live edits.

### Save data collections

- **`telepoints`**: Maps normalised map paths (manifest paths + world coordinates for worlds) to ordered `{ position = {x, y}, tag = "raw tag" }` records (`sf.Vector2u` at runtime; unlocalised string tags). `GameInstance:recordTelepoint(mapPath, position, tag)` preserves the first entry per `(map, position)`; `getTelepoints(mapPath)` returns records, `getVisitedMapPaths()` returns sorted paths (copy), and `getTelepointsForMap(key)` matches ignoring extensions (copy).
- **`triggeredTutorials`**: Unique tutorial keys in display-start order (`list<string>` at runtime, JSON array in saves preserving `[]`). Loading validates the native reader's `n` count against array elements and rejects holes or invalid keys. `hasTriggeredTutorial(key)` queries history and `recordTriggeredTutorial(key)` appends once when mask contraction starts (not while queued). Legacy v1 saves missing this field load as empty.
- **`shops` record format**: Shared array of `[mapPath, mapTag]` pairs using normalised map/manifest paths and `Actor:getMapTag()`. Entries are unique by full pair in first-visit order, saving empty as `[]` (legacy v1 saves missing `shops` load empty).
- **`shops` API and events**: `GameInstance:getShops()` returns a copy; `recordShop(mapPath, tag)` adds a pair while `GameInstance.EnableShopRecord` is enabled (successful shop open records without requiring a purchase; see [Visited shops](<Windows Menus Input and Controls.md#visited-shops>)); `removeShop(mapPath, tag)` removes even when recording is disabled. Both return whether the list changed, publishing `Enums.EventKey.ShopRecordsChanged` (`Source.Configs.ShopRecordsChangePayload`, `{ instance = self }`) only on real changes so subscribers can match `instance`. Revisiting a removed shop can record it again.
- **`map`**: Object keyed by `playerKeys` where every member has one non-empty normalised map path (extra keys fail loading). `GameInstance:getCurrentMapPath()` returns the primary player's map or `nil`; `applyMapInfo` writes it; `restoreFromData(data)` applies a validated save.

### Save format, encryption and UI contract

- **Format selection (`SAVE_AS_LDC`)**: `false` (default) writes plain `.json`; `true` writes compressed, obfuscated binary `.ldc`. **Encrypt save contents** sets the native default for C++ Source packages. Any project (including Standalone) can override it by setting `SAVE_AS_LDC = true` or `false` at the top of `Scripts/Entry.lua` before all `require` calls; `Source.Save` captures the setting on first load and ignores later writes.
- **LDC codec and encryption**: `Source.Save` reads and writes through `Engine.saveData` and `Engine.loadData`, decoding `.ldc` directly into runtime data without intermediate JSON (legacy compressed-JSON `.ldc` saves are unsupported). Save encryption is independent of project JSON encryption, though Pack Options groups both under **Encrypt game data**.
- **Loading and previews**: `GetSavePath(slot)` returns the 1-based slot path for the active format. Previews load asynchronously without constructing gameplay objects; confirming a load invokes `Save.LoadGame(filePath)`.
- **Save/Load window**: `open` accepts `initialMode` as `Enums.SaveLoadMode.LOAD` (`"load"`, default) or `.SAVE` (`"save"`; load-only windows stay in load mode). `onClose(reason)` receives `Enums.SaveLoadCloseReason.CANCEL` (`"cancel"`), `.SAVED` (`"saved"`) or `.LOADED` (`"loaded"`) after closing. The window shows loading, empty and failed states, preselects the most recently modified slot (or slot 1 when none exists), and focuses it after the open animation. New screenshots preserve aspect ratio up to `224 × 168` pixels (larger existing screenshots stay readable).

> **Important:** Changing `SAVE_AS_LDC` does not rename or migrate existing slots — choose the format before release or write a migration. Save-version upgrades, atomic writes, backups, cloud synchronisation and custom tuple-key record formats are project responsibilities.

## Native containers at data boundaries

Convert Standard `list`, `tuple` and `dict` values to ordinary Lua tables before writing JSON or LDC:

```lua
local Engine = require("Engine")
Engine.saveData("slot.ldc", payload:toTable())
```

Decoding either format produces plain Lua tables: arrays carry logical length in `n`, root `null` and `null` array slots become `nil`, and `null` object fields keep `cjson.null`. See [Ludork Lua Advanced](<../../Getting Started/Ludork Lua Advanced.md>) for tuple-key strings, `cjson.null` handling and record-array reconstruction; never persist an internal tuple hash.
