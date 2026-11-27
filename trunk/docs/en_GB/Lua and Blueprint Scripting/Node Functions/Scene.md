# GlobalFunctions.Scene

`GlobalFunctions.Scene` provides Blueprint nodes for map transfers, timers, tutorials, dialogue and shop windows, camera control, terrain editing, dynamic Actor spawning and persistence records.

**Metadata source:** `Scripts/GlobalFunctions/Scene_meta.lua`

## Scene

### Functions and events

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `GotoMap` | `mapPath: string = ""`, `blockTransition: bool = false`, `position: sf.Vector2i` | — | Transfers to `position` on `mapPath`. `Transfer` metadata links `position` to `mapPath`. |
| `GameOver` | — | — | Triggers the Game Over sequence. |
| `AddTimer` | `interval: float`, `blocking: bool = false` | `Engine.AsyncOperation` | Latent. Branch: `TimeUp` (`true`). Waits for `interval` seconds. |
| `ShowEnemyBook` | — | — | Opens the monster handbook window when permitted. |
| `ShowTutorial` | `key: string` | `Engine.AsyncOperation` | Latent. Branch: `Finished` (`true`). Displays the configured tutorial for `key`. |
| `ShowMessageByTag` | `name: string`, `message: string`, `refActorTag: string = ""` | `Engine.AsyncOperation` | Latent. Branch: `FinishedDialogue` (`true`). Displays a dialogue message positioned relative to the Actor matching `refActorTag`. |
| `ShowMessage` | `name: string`, `message: string`, `actor: Engine.Actor` | `Engine.AsyncOperation` | Latent. Branch: `FinishedDialogue` (`true`). Displays a dialogue message positioned relative to `actor`. |
| `ShowVoiceMessageByTag` | `name: string`, `message: string`, `voiceFileName: string`, `refActorTag: string = ""` | `Engine.AsyncOperation` | Latent. Branch: `FinishedDialogue` (`true`). Path (`voiceFileName`): `/Game/Assets/Voices`. |
| `ShowVoiceMessage` | `name: string`, `message: string`, `voiceFileName: string`, `refActor: Engine.Actor`, `minDistance: float = 64` | `Engine.AsyncOperation` | Latent. Branch: `FinishedDialogue` (`true`). Path (`voiceFileName`): `/Game/Assets/Voices`. |
| `ShowSelection` | `name: string = ""`, `options: string[] = {}`, `refActorTag: string = ""`, `allowCancel: bool = true` | `Engine.AsyncOperation` | Latent. Branches: `Selected0` (`0`), `Selected1` (`1`), `Selected2` (`2`), `Selected3` (`3`), `Cancelled` (`-1`). |
| `ShowRefSelection` | `name: string = ""`, `options: string[] = {}`, `refActor: Engine.Actor`, `allowCancel: bool = true` | `Engine.AsyncOperation` | Latent. Branches: `Selected0` (`0`), `Selected1` (`1`), `Selected2` (`2`), `Selected3` (`3`), `Cancelled` (`-1`). |
| `LockCamera` | — | — | Locks the scene camera. |
| `UnlockCamera` | — | — | Unlocks the scene camera. |
| `AttachCamera` | `actor: Engine.Actor` | — | Attaches the scene camera to `actor`. |
| `MoveCamera` | `delta: sf.Vector2f` | — | Moves the scene camera by `delta`. |
| `RecordTelepoint` | `mapPath: string = ""`, `x: int = 0`, `y: int = 0`, `tag: string = ""` | — | Records a floor-teleporter destination on `mapPath`. |
| `CreateActorFromBPPath` | `bpPath: string = ""`, `layerName: string = "default"`, `position: sf.Vector2i`, `tag: string = ""`, `emitCreateEvent: bool = true` | `actor: Engine.Actor` | Blueprint class picker on `bpPath` (`BlueprintClassVars`). Spawns an Actor on `layerName`. |
| `CreateActorFromBPPathWithDefaults` | `bpPath: string = ""`, `defaults: any = {}`, `layerName: string = "default"`, `position: sf.Vector2i`, `tag: string = ""`, `emitCreateEvent: bool = true` | `actor: Engine.Actor` | Blueprint class picker on `bpPath` (`BlueprintClassVars`). Spawns an Actor with overridden defaults. |
| `DestroyTerrain` | `layerName: string`, `position: sf.Vector2i`, `tileID: any` | — | Destroys a terrain tile on `layerName` at `position`. |
| `DestroyTerrainList` | `layerName: string`, `positions: sf.Vector2i[] = {}`, `tileID: any` | — | Destroys terrain tiles on `layerName` at each position in `positions`. |
| `GetTerrainTile` | `layerName: string`, `position: sf.Vector2i` | `tileID: any` | Pure. Returns the tile ID on `layerName` at `position`. |
| `GetTerrainTilePositions` | `layerName: string`, `tileID: any` | `positions: sf.Vector2i[]` | Pure. Returns all positions on `layerName` matching `tileID`. |
| `RecordAddedActor` | `actor: Engine.Actor` | — | Records `actor` as added in map persistence. |
| `SelfRecordAdded` | — | — | Records the current graph owner Actor as added in map persistence. |
| `RecordActorPosition` | `actor: Engine.Actor` | — | Records `actor`'s current position in map persistence. |
| `SelfRecordActorPosition` | — | — | Records the current graph owner Actor's position in map persistence. |
| `RecordDestroyedActor` | `actor: Engine.Actor` | — | Records `actor` as destroyed in map persistence without destroying it. |
| `SelfRecordDestroyed` | — | — | Records the current graph owner Actor as destroyed in map persistence. |
| `RecordAndDestroyActor` | `actor: Engine.Actor` | — | Records `actor` as destroyed and destroys it. |
| `SelfRecordAndDestroy` | — | — | Records the current graph owner Actor as destroyed and destroys it. |
| `OpenPlayerName` | — | `Engine.AsyncOperation` | Latent. Branch: `Closed` (`true`). Opens the player rename window. |
| `OpenShop` | `items: string[] = {}`, `canSell: bool = true`, `actor: Engine.Actor = nil` | `Engine.AsyncOperation` | Latent. Branch: `Closed` (`true`). Opens an item shop window. |
| `OpenAttrShop` | `actor: Engine.Actor`, `shopName: string = ""`, `shopDescription: string = ""`, `abilities: any = {}`, `price: any = 0`, `priceIncrement: int = 1`, `moneyName: string = "GOLD"` | `Engine.AsyncOperation` | Latent. Branch: `Closed` (`true`). Opens an attribute shop window for `actor`. |
| `OpenAttrShopByTag` | `actorTag: string = ""`, `shopName: string = ""`, `shopDescription: string = ""`, `abilities: any = {}`, `price: any = 0`, `priceIncrement: int = 1`, `moneyName: string = "GOLD"` | `Engine.AsyncOperation` | Latent. Branch: `Closed` (`true`). Opens an attribute shop window for the Actor matching `actorTag`. |

### Lua waiting and results

Latent functions return `Engine.AsyncOperation`; see [Lua asynchronous tasks](<../../Getting Started/Ludork Lua Advanced.md>) for sequential waiting. Tutorials, messages, timers and window-close operations complete with `true`. Selections return a zero-based index or `-1` when the player cancels; that cancellation follows the Blueprint `Cancelled` output rather than cancelling the operation.

Messages and windows complete after their closing animation. A new message or selection cancels the previous dialogue operation; reopening a shop replaces its operation, whereas repeated requests made while the player-name window is open share one operation. Voice playback stops when its dialogue completes or is cancelled. Scene and map cleanup follows [SceneMap](<../Source Classes/SceneMap.md#script-operations-and-lifetime>).

### Shop Actors

`OpenShop(items, canSell, actor?)` uses its optional Actor to identify a shop visit. When omitted in a Blueprint, it uses the current graph owner if that owner is an Actor. Direct Lua callers supply the shop Actor as the third argument to record the opening. `OpenAttrShop` uses its `actor` argument, and `OpenAttrShopByTag` resolves that Actor on the current map. An Actor without a map identity does not create a record. These nodes complete with `Closed`; see [Visited shops](<../Default Gameplay/Windows Menus Input and Controls.md#visited-shops>) for recording and remote-access behaviour.

### GotoMap world destinations

`GotoMap` accepts ordinary maps, world manifests and placed children. Manifest positions are world coordinates, and child positions are local. `GotoMap` fails on unplaced children, invalid data and out-of-range positions. World entry waits for a complete target viewport. `blockTransition` skips only the visual mask, not the readiness checks.

### ShowEnemyBook

`ShowEnemyBook()` opens the current-map monster handbook when the player owns it and the scene allows an overlay to open.

### Tutorials

`ShowTutorial(key)` displays the configured guide and completes with `true` after confirmation. Pending requests for the same key share an operation, even if the key has already been recorded; otherwise, recorded keys complete immediately. Different keys queue in order, and unknown keys raise an error.

When a request reaches the front of the queue, empty localised text (`""`) skips presentation and recording and completes through `Finished`. Recording rules are described under [Saves](<../Default Gameplay/Runtime Data Configuration and Saves.md#saves>).

Blueprints choose tutorial keys and their order; see [Map introduction](<../Default Gameplay/Windows Menus Input and Controls.md#map-introduction>) for the `Map_01` configuration.

### OpenPlayerName

`OpenPlayerName()` opens the rename window of the primary Player. `Closed` resumes after the closing animation, whether the rename is confirmed or cancelled.
