# Source.Scenes.SceneMap

`SceneMap` declares the gameplay commands, dialogue windows, menu entry points and persistence records of a map Scene.

Metadata source: `Scripts/Source/Scenes/SceneMap_meta.lua`

## Scene

Direct metadata bases: `{ "Source.Gameplay.GameplayScene", "GameplayScene" }`

Gameplay commands follow the [GameplayScene contract](<Gameplay.md#gameplayscene>).

`SceneMap` is the public gameplay entry point and owns the current `inst` and `player`. `getGameInstance()`, `getPlayer()` and `getGameMap()` read the current objects. The Scene coordinates lifecycle and frame order, map input blocking, Game Over requests, tutorials and locale refresh. It creates HUD and Region Title directly; optional windows follow [Windows, Menus, Input and Controls](<../Default Gameplay/Windows Menus Input and Controls.md>).

Each Scene owns separate instances of these modules under `Source.Scenes.SceneMap`:

| Module | State and responsibility |
|---|---|
| `MapSession` | Loaded map, map path, region, builder and pending transfers; map replacement, destination resolution, floor previews and persistence commands. |
| `Dialogue` | Message window, active operation, voice, movement restoration and dialogue locale snapshots. |
| `Windows` | Other optional windows, focus groups, window operations, movement restoration and deferred menu/save requests. |
| `Environment` | Map audio and panorama settings, return-to-scene restoration and world ambient-light transitions. |

The Scene passes current objects to components or exposes them through getters and commands. Components keep their private state separate; gameplay scripts use the Scene entry points.

Ordinary map loading binds the Scene and current map identity, restores terrain and Actor records, then binds the Player and runs surviving Actors' `onCreate`. Map replacement and Scene destruction release the departing map's ConditionalActor subscriptions without dispatching Actor destruction or altering persistent records.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `getGameMap` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self" | gameMap: { "Global.GameMap", "GameMap" } | Pure |
| `openPlayerName` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self" | return: { "Engine", "AsyncOperation" } | Latent Closed |
| `showMessage` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; name: string; message: string; refActor: { "Engine", "Actor" }; localeArgs: Dict[string, any] = {  } | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "FinishedDialogue", FinishedDialogue = { [1] = true } } |
| `showSelection` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; name: string; options: string[]; refActor: { "Engine", "Actor" }; allowCancel: bool = true; localeArgs: Dict[string, any] = {  } | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Selected0", [2] = "Selected1", [3] = "Selected2", [4] = "Selected3", [5] = "Cancelled", Selected0 = { [1] = 0 }, Selected1 = { [1] = 1 }, Selected2 = { [1] = 2 }, Selected3 = { [1] = 3 }, Cancelled = { [1] = -1 } } |
| `applyLoadedGame` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; inst: { "Source.GameInstance", "GameInstance" } | — | ExecSplit { [1] = "default", default = nil } |
| `showEnemyBook` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self" | — | ExecSplit { [1] = "default", default = nil } |
| `showFloorTeleporter` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self" | — | ExecSplit { [1] = "default", default = nil } |
| `showShopRecords` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self" | — | ExecSplit { [1] = "default", default = nil } |
| `openMenu` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self" | — | ExecSplit { [1] = "default", default = nil } |
| `recordAddedActor` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; actor: { "Engine", "Actor" } | — | ExecSplit { [1] = "default", default = nil } |
| `recordActorPosition` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; actor: { "Engine", "Actor" }; position: sf.Vector2i = nil | — | ExecSplit { [1] = "default", default = nil } |
| `recordDestroyedActor` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; actor: { "Engine", "Actor" } | — | ExecSplit { [1] = "default", default = nil } |

### Script operations and lifetime

`showShopRecords()` opens the [visited-shop list](<../Default Gameplay/Windows Menus Input and Controls.md#visited-shops>) under the same conditions as V and returns a Lua boolean indicating whether it opened. `openShop(buyItemIDs, canSell, actor?)` accepts an optional shop Actor for recording; direct Scene calls supply that Actor explicitly.

`showMessage`, `showSelection`, `openPlayerName`, `openShop` and `openAttrShop` return `Engine.AsyncOperation`. Results and window replacement rules match the [Scene nodes](<../Node Functions/Scene.md#lua-waiting-and-results>).

`showVoiceMessage(name, message, voiceFileName, refActor, minDistance, spatial)` is also available to Lua. With `spatial = true`, it positions the voice at `refActor` and passes `minDistance` to the audio manager; with `false`, the voice is non-spatial. The reference actor still positions the message window.

Lua wrappers that supply node-local formatting values can call `showMessageWithContext`, `showSelectionWithContext` or `showVoiceMessageWithContext`. Each takes its corresponding ordinary method's arguments followed by `localVars: table<string, any>`, copied when the dialogue starts, and returns the same `Engine.AsyncOperation`. The ordinary Scene methods and Scene nodes capture their active node context before delegating. The `WithContext` methods have no Blueprint nodes.

Scene exit, destruction and map replacement cancel unfinished tutorials, dialogues, shops, player-name windows and scene timers, dismissing windows and releasing input blocks. They also close the shop-record list and release its remote shop Actor. Registered operations on departing map Actors, including movement and doors, are also cancelled. Waiting Lua tasks and Blueprint events stop without running subsequent statements or completion outputs.

Scene exit and destruction also discard deferred menu, Save/Load and quick-save requests and queued map transfers. Cancelling a queued transfer releases its frozen background and restores movement on the player captured by that request. `applyLoadedGame(inst)` and `applyPrimaryPlayer()` cancel outgoing interactions and pending requests before rebinding the player, HUD and existing windows, then force a map reload. Movement restoration therefore targets the outgoing player rather than changing the newly bound player's movement state.

Pushing another Scene stops map BGM/BGS and clears weather and panorama. On `SceneManager.popScene()`, the departing Scene's BGM/BGS and weather are cleared, then the map's configured audio and panorama are reapplied. Ordinary maps use settings captured at load; worlds follow the [composite-world environment rules](<../Default Gameplay/Resources Audio Video and Effects.md#composite-world-environment>). Music restarts with the map filter's offset and loop-point settings rather than its previous playback position. Temporary script changes to music, filters, weather and panorama are not restored.

The map, Actors, fog and ambient-light transitions remain alive. Returning neither reloads the map nor repeats Actor `onCreate`.

### Composite-world behaviour

`getGameMap` returns `GameMap` or a compatible `WorldGameMap`. `gotoMapAndPos` accepts either world coordinates for a manifest or local coordinates for a placed child, and stores the manifest as the primary player's map. `blockTransition` skips the mask, but not the target readiness checks.

World Actor records use the manifest. Terrain uses child paths and local cells. `recordActorPosition` updates moved-root ownership. Its optional `position` input defaults to nil, which records the Actor's current position; an explicit `sf.Vector2i` records that position instead. `recordDestroyedActorTag` releases a resident match's condition subscriptions and suppresses its tree without destruction events or child I/O. `recordDestroyedActor` remains record-only.
