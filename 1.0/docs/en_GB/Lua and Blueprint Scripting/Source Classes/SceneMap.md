# Source.Scenes.SceneMap

`SceneMap` declares the gameplay commands, dialogue windows, menu entry points and persistence records of a map Scene.

Metadata source: `Scripts/Source/Scenes/SceneMap_meta.lua`

## Scene

Direct metadata bases: `{ "Source.Gameplay.GameplayScene", "GameplayScene" }`

Gameplay commands follow the [GameplayScene contract](<Gameplay.md#gameplayscene>).

The Scene creates HUD and Region Title directly and creates optional windows on first display. Window ownership and reuse follow [Windows, Menus, Input and Controls](<../Default Gameplay/Windows Menus Input and Controls.md>).

Ordinary map loading binds the Scene, the current map identity and the Player. It restores terrain and Actor records, and only then runs surviving Actors' `onCreate`. Map replacement and Scene destruction release the departing map's ConditionalActor subscriptions without dispatching Actor destruction or altering persistent records.

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
| `openMenu` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self" | — | ExecSplit { [1] = "default", default = nil } |
| `recordAddedActor` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; actor: { "Engine", "Actor" } | — | ExecSplit { [1] = "default", default = nil } |
| `recordActorPosition` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; actor: { "Engine", "Actor" }; position: sf.Vector2i = nil | — | ExecSplit { [1] = "default", default = nil } |
| `recordDestroyedActor` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; actor: { "Engine", "Actor" } | — | ExecSplit { [1] = "default", default = nil } |

### Script operations and lifetime

`showMessage`, `showSelection`, `openPlayerName`, `openShop` and `openAttrShop` return `Engine.AsyncOperation`. Results and window replacement rules match the [Scene nodes](<../Node Functions/Scene.md#lua-waiting-and-results>).

`showVoiceMessage(name, message, voiceFileName, refActor, minDistance, spatial)` is also available to Lua. With `spatial = true`, it positions the voice at `refActor` and passes `minDistance` to the audio manager; with `false`, the voice is non-spatial. The reference actor still positions the message window.

Scene exit, destruction and map replacement cancel unfinished tutorials, dialogues, shops, player-name windows and scene timers, dismissing windows and releasing input blocks. Registered operations on departing map Actors, including movement and doors, are also cancelled. Waiting Lua tasks and Blueprint events stop without running subsequent statements or completion outputs.

Pushing another Scene stops map BGM/BGS and clears weather and panorama. On `SceneManager.popScene()`, the departing Scene's BGM/BGS and weather are cleared, then the map's configured audio and panorama are reapplied. Ordinary maps use settings captured at load; worlds follow the [composite-world environment rules](<../Default Gameplay/Resources Audio Video and Effects.md#composite-world-environment>). Music restarts with the map filter's offset and loop-point settings rather than its previous playback position. Temporary script changes to music, filters, weather and panorama are not restored.

The map, Actors, fog and ambient-light transitions remain alive. Returning neither reloads the map nor repeats Actor `onCreate`.

### Composite-world behaviour

`getGameMap` returns `GameMap` or a compatible `WorldGameMap`. `gotoMapAndPos` accepts either world coordinates for a manifest or local coordinates for a placed child, and stores the manifest as the primary player's map. `blockTransition` skips the mask, but not the target readiness checks.

World Actor records use the manifest. Terrain uses child paths and local cells. `recordActorPosition` updates moved-root ownership. Its optional `position` input defaults to nil, which records the Actor's current position; an explicit `sf.Vector2i` records that position instead. `recordDestroyedActorTag` releases a resident match's condition subscriptions and suppresses its tree without destruction events or child I/O. `recordDestroyedActor` remains record-only.
