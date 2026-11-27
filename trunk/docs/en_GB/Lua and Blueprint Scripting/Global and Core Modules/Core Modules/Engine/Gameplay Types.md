# Engine: Gameplay Types

This page documents core gameplay object types under `Engine`: `Actor`, `BPBase`, `Character`, `Component`, `BillboardComponent` and `LightComponent`. Instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin; static methods and events take no receiver.

## Actor

Base map entity owning a transform, sprite, material, movement route, hierarchy, Blueprint graph and attached components.

- **Materials:** `material` references a formal 2D material resource and `materialParameters` stores per-Actor overrides. Switching to another material clears old overrides, and parameter writes validate against the selected resource; see [Material resources](<Maps and Materials.md#material-resources>) for paths, defaults, lighting and masking.
- **Material bounds:** The Lua-only `getMaterialBounds()` (`metadata = false`) returns world-space visual bounds including the material's declared maximum vertex displacement and the Actor transform. Capture effects use these bounds to avoid clipping displaced pixels without altering collision bounds or occupied map cells.
- **Hierarchy and Blueprint graph:** `collectTree()` (`metadata = false`) returns a root-first BFS snapshot preserving child order and ownership, raising an error on duplicates or cycles. `setGraph` stores an `Engine.Graph` instance (returned by `getGraph`), and passing `nil` clears it.
- **Visibility:** `visible` defaults to `true` (`getVisible()` reads it; `isVisibleInHierarchy()` also checks every ancestor). An invisible Actor keeps its instance, lifecycle and Tick behaviour but skips drawing, attached lights and light blocking, collision, overlap and pathfinding obstruction.
- **Hierarchy visibility writes:** `setVisible(value, applyToChildren = true)` updates descendants' own flags by default; pass `false` to preserve them while still gating the subtree by the parent's visibility. Direct assignment updates only the Actor's own flag.
- **Footprint queries:** `getOccupiedMapCellsAtMapPosition(mapPosition)` computes the Actor footprint placed at `mapPosition * CellSize`, keeping current origin, translation, rotation and scale while excluding in-cell movement offset (empty bounds return the requested cell). `getOccupiedMapCells()` with no argument reports the live footprint including cells crossed mid-step.
- **Texture path in editor:** `texturePath` stores a `/Game/Assets/...` path, opening `Assets/Characters` by default. The **From Tileset** checkbox tracks whether the path sits under `/Game/Assets/Tilesets`: checking it rewrites `/Game/Assets/Characters/...` to the matching relative path under `/Game/Assets/Tilesets`, clearing it restores `/Game/Assets/Characters`, and reopening the Actor restores the checkbox from the stored path.

### Routes and operation lifetime

`actor:setRoute(route)` returns an `Engine.AsyncOperation` that emits `0` on start and completes with `1` when the route and current movement finish. The native `setRoute` Blueprint node continues through `default` immediately; use [Movement nodes](<../../../Node Functions/Movement.md>) for `Started` / `Finished` branches.

- **Cancellation triggers:** Starting a new route, calling `stop()`, disabling movement, destroying the Actor or changing its map cancels any unfinished route operation.
- **Interrupted steps:** `stop()` and `setMoveEnabled(false)` snap to the nearest map cell. If an interrupted step lands in a different cell from departure, movement clears and occupancy updates before the arrival sequence runs once: the moving Actor's `onOverlap`, each remaining valid contact's `onOverlap`, then `_onArrivedAtMapCell()`. Stopping in the departure cell, stopping an unstarted route or repeating `stop()` emits no arrival notification.
- **Re-entrant callbacks:** Normal and interrupted arrivals stop dispatching old-cell notifications if a callback destroys or hides the moving Actor, changes its map or moves it away from the arrival cell (normal movement also discards remaining frame time). `stop()` clears the old movement before cancellation callbacks run, so a new route started from a callback is preserved.

These Lua-only methods manage additional Actor-scoped operations (such as door animations):

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `trackAsyncOperation` | `operation: Engine.AsyncOperation` | — | Lua only. Registers an operation to cancel when the Actor is destroyed, released or changes map; cancels immediately if the Actor is already destroyed. |
| `cancelAsyncOperations` | — | — | Lua only. Cancels route and registered operations without stopping their underlying animations. |

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `scriptMixin` | `bool` | `false` | Blueprint only. |
| `scriptPath` | `string` | `""` | Blueprint only. Visible when `scriptMixin == true`; project path under `Scripts/Mixins` (`*.lua`). |
| `tag` | `string` | `""` | — |
| `switchInterval` | `float` | `0.2` | — |
| `animatable` | `bool` | `false` | — |
| `visible` | `bool` | `true` | — |
| `material` | `string` | `"Data/Materials/Global/Standard.json"` | `Data/Materials/*.json` path. |
| `materialParameters` | `Dict[string, any]` | `{}` | — |
| `collisionEnabled` | `bool` | `false` | — |
| `tickable` | `bool` | `false` | — |
| `speed` | `float` | `64` | — |
| `autoSound` | `string` | `""` | Path under `/Game/Assets/Sounds`. |
| `autoSoundInterval` | `float` | `0` | Active when `autoSound ~= ""`. |
| `autoSoundParams` | `Engine.AutoSoundParams` | `{ volume = 100, minDistance = 64, attenuation = 1, loop = false, maxDistance = 0 }` | Active when `autoSound ~= ""`. |
| `texturePath` | `string` | `""` | Path under `/Game/Assets/Characters` (or `/Game/Assets/Tilesets`). |
| `defaultRect` | `sf.IntRect` | `[0, 0, 32, 32]` | Bounds validated against `texturePath`. |
| `defaultTranslation` | `sf.Vector2f` | `[0, 0]` | — |
| `defaultRotation` | `float` | `0` | — |
| `defaultScale` | `sf.Vector2f` | `[1, 1]` | — |
| `defaultOrigin` | `sf.Vector2f` | `[0, 0]` | — |
| `lightComp` | `Engine.LightComponent` | — | Component. |
| `emitterComp` | `Engine.EmitterComponent` | — | Component; see [GPU particle playback](<Time Rendering and Particles.md#emittercomponent>). |
| `billboardComp` | `Engine.BillboardComponent` | — | Component; see [BillboardComponent](#billboardcomponent). |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getCollisionEnabled` | — | `bool` | Pure. |
| `setCollisionEnabled` | `enabled: bool` | — | Exec (`default`). |
| `getPathfindingBlocks` | — | `bool` | Pure. |
| `setPathfindingBlocks` | `blocks: bool` | — | — |
| `isDestroyed` | — | `bool` | Pure. |
| `markDestroyed` | `destroyed: bool` | — | — |
| `update` | `deltaTime: float` | — | — |
| `lateUpdate` | `deltaTime: float` | — | — |
| `fixedUpdate` | `fixedDelta: float` | — | — |
| `getPosition` | — | `pos: sf.Vector2f` | Pure. |
| `getRelativePosition` | — | `pos: sf.Vector2f` | Pure. |
| `getMapPosition` | — | `pos: sf.Vector2i` | Pure. |
| `getOccupiedMapCells` | `worldPosition: sf.Vector2f = nil` | `cells: sf.Vector2i[]` | Pure. |
| `getOccupiedMapCellsAtMapPosition` | `mapPosition: sf.Vector2i` | `cells: sf.Vector2i[]` | Pure. |
| `getRelativeMapPosition` | — | `pos: sf.Vector2i` | Pure. |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `getGlobalBounds` | — | `sf.FloatRect` | Pure. |
| `setPosition` | `position: sf.Vector2f` | — | Exec (`default`). |
| `setRelativePosition` | `position: sf.Vector2f` | — | Exec (`default`). |
| `setMapPosition` | `position: sf.Vector2u` | — | Exec (`default`). |
| `setRelativeMapPosition` | `position: sf.Vector2u` | — | Exec (`default`). |
| `move` | `offset: sf.Vector2f` | — | Exec (`default`). |
| `getRotation` | — | `angle: sf.Angle` | Pure. |
| `getRelativeRotation` | — | `angle: sf.Angle` | Pure. |
| `setRotation` | `angle: sf.Angle` | — | Exec (`default`). |
| `rotate` | `angle: sf.Angle` | — | Exec (`default`). |
| `setRelativeRotation` | `angle: sf.Angle` | — | Exec (`default`). |
| `getScale` | — | `scale: sf.Vector2f` | Pure. |
| `getRelativeScale` | — | `scale: sf.Vector2f` | Pure. |
| `setScale` | `factors: sf.Vector2f` | — | Exec (`default`). |
| `scale` | `factor: sf.Vector2f` | — | Exec (`default`). |
| `setRelativeScale` | `scale: sf.Vector2f` | — | Exec (`default`). |
| `getOrigin` | — | `origin: sf.Vector2f` | Pure. |
| `setOrigin` | `origin: sf.Vector2f` | — | Exec (`default`). |
| `getTranslation` | — | `translation: sf.Vector2f` | Pure. |
| `setTranslation` | `translation: sf.Vector2f` | — | Exec (`default`). |
| `setAlignment` | `alignment: sf.Vector2f` | — | Exec (`default`). |
| `getMap` | — | `map_: Engine.ActorMapService` | Pure. |
| `setMap` | `inMap: Engine.ActorMapService` | — | Exec (`default`). |
| `getParent` | — | `Engine.Actor` | Pure. |
| `setParent` | `parent: Engine.Actor` | — | — |
| `getChildren` | — | `Engine.Actor[]` | Pure. |
| `addChild` | `child: Engine.Actor` | — | Exec (`default`). |
| `removeChild` | `child: Engine.Actor` | — | — |
| `getVisible` | — | `visible: bool` | Pure. |
| `isVisibleInHierarchy` | — | `bool` | Pure. |
| `setVisible` | `visible: bool = nil`, `applyToChildren: bool = true` | — | Exec (`default`). |
| `getAnimatable` | — | `animatable: bool` | Pure. |
| `setAnimatable` | `animate: bool = nil`, `applyToChildren: bool = true` | — | Exec (`default`). |
| `getSpriteTexture` | — | `texture: sf.Texture` | Pure. |
| `getTexture` | — | `texture: sf.Texture` | Pure. |
| `setSpriteTexture` | `texture: sf.Texture = nil`, `resetRect: bool = false` | — | Exec (`default`). |
| `setTexture` | `texture: sf.Texture = nil`, `resetRect: bool = false` | — | Exec (`default`). |
| `setTextureRect` | `rectangle: sf.IntRect` | — | — |
| `getTextureRect` | — | `sf.IntRect` | Pure. |
| `getMaterial` | — | `material: string` | Pure. |
| `setMaterial` | `material: string` | — | Exec (`default`). |
| `getMaterialParameter` | `id: string` | `any` | Pure. |
| `setMaterialParameter` | `id: string`, `value: any` | — | — |
| `resetMaterialParameter` | `id: string` | — | — |
| `setGraph` | `graph: Engine.Graph` | — | Exec (`default`). |
| `getGraph` | — | `Engine.Graph` | Pure. |
| `hasGraph` | — | `bool` | Pure. |
| `getMapTag` | — | `string` | Pure. |
| `setMapTag` | `value: string` | — | — |
| `ensureMapTag` | — | — | — |
| `onCreate` | — | — | Event. Exec (`default`). |
| `onTick` | `deltaTime: float` | — | Event. Exec (`default`). |
| `onLateTick` | `deltaTime: float` | — | Event. Exec (`default`). |
| `onFixedTick` | `fixedDelta: float` | — | Event. Exec (`default`). |
| `onDestroy` | — | — | Event. Exec (`default`). |
| `onWorldSleep` | — | — | Event. Exec (`default`). |
| `onWorldWake` | `elapsedSeconds: float` | — | Event. Exec (`default`). |
| `onCollision` | `other: Engine.Actor[]` | — | Event. Exec (`default`). |
| `onOverlap` | `other: Engine.Actor[]` | — | Event. Exec (`default`). |
| `destroy` | — | — | Exec (`default`). |
| `MapMove` | `offset: sf.Vector2i` | `bool` | Exec branches: `success` (`true`), `fail` (`false`). |
| `getTickable` | — | `tickable: bool` | Pure. |
| `setTickable` | `tickable: bool = nil`, `applyToChildren: bool = true` | — | Exec (`default`). |
| `intersects` | `other: Engine.Actor` | `intersects: bool` | Pure. |
| `isMoving` | — | `isMoving: bool` | Pure. |
| `isInRoute` | — | `isInRoute: bool` | Pure. |
| `setRoute` | `route: sf.Vector2i[] = {}` | `Engine.AsyncOperation` | Exec (`default`). Editor metadata: `MoveRouteVars = { "route" }`. |
| `getRoute` | — | `route: sf.Vector2i[]` | Pure. |
| `getMoveEnabled` | — | `moveEnabled: bool` | Pure. |
| `setMoveEnabled` | `enabled: bool` | — | Exec (`default`). |
| `stop` | — | — | Exec (`default`). |
| `getVelocity` | — | `velocity: sf.Vector2f` | Pure. |
| `BlueprintEvent` | `object: any`, `objectType: any`, `eventName: string`, `keywordArguments: any = nil`, `onComplete: any = nil` | — | Static. |
| `HasBlueprintEvent` | `object: any`, `eventName: string` | `bool` | Static. |
| `IsBlueprintEventEmpty` | `object: any`, `eventName: string` | `bool` | Static. |
| `GenActor` | `actorModel: any`, `texture: any = nil`, `textureRect: any = nil`, `tag: string = nil` | `any` | Static. |

`onWorldSleep` fires when world streaming removes an Actor from Active simulation; `onWorldWake` fires when it returns to Active simulation with the elapsed dormant duration. Both base implementations are empty, and ordinary maps emit neither event.

## BPBase

Static Blueprint event and native graph execution helpers.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `BlueprintEvent` | `object: any`, `objectType: any`, `eventName: string`, `keywordArguments: any = nil`, `onComplete: any = nil` | — | Static. |
| `HasBlueprintEvent` | `object: any`, `eventName: string` | `bool` | Static. |
| `IsBlueprintEventEmpty` | `object: any`, `eventName: string` | `bool` | Static. |
| `ExecuteParentEvent` | `object: any`, `classType: any`, `eventName: string`, `arguments: any = nil`, `keywordArguments: any = nil`, `localGraph: any = nil`, `onComplete: any = nil` | `bool` | Static. |
| `ExecuteGraph` | `graph: Engine.Graph`, `eventName: string`, `keywordArguments: any = nil`, `localGraph: any = nil`, `onComplete: any = nil` | `bool` | Static. Lua only (`metadata = false`). |

`ExecuteGraph` runs a native `Engine.Graph` event; passing `nil` or a non-`Graph` raises a type error. Re-entering while the same graph and event are locked or have pending latent work is rejected.

## Character

**Inherits:** `Engine.Actor`

Directional four-frame animated Actor (`defaultRect` is excluded from editable Blueprint properties via `InvalidVars: { "defaultRect" }`).

The read-only class constants `Engine.Character.SPRITE_FRAME_COLUMNS` and `SPRITE_DIRECTION_ROWS` are both `4`, counting animation columns and directional rows respectively. The default frame rectangle divides texture width and height by these constants; custom frame rectangles remain supported.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `direction` | `Engine.Direction` | `0` | Blueprint enum: `Enums.Engine.Direction`. |
| `directionFix` | `bool` | `false` | — |
| `animateWithoutMoving` | `bool` | `false` | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setSpriteTexture` | `texture: sf.Texture = nil`, `resetRect: bool = false` | — | Exec (`default`). |
| `setTexture` | `texture: sf.Texture = nil`, `resetRect: bool = false` | — | Exec (`default`). |
| `setTextureRect` | `rectangle: sf.IntRect` | — | Exec (`default`). |
| `MapMove` | `offset: sf.Vector2i` | `bool` | Exec branches: `success` (`true`), `fail` (`false`). |
| `update` | `deltaTime: float` | — | — |
| `GenActor` | `actorModel: any`, `texture: any = nil`, `textureRect: any = nil`, `tag: string = nil` | `any` | Static. |

## Component

Base class for attachable Actor components.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `onAttach` | `owner: any` | `any[]` | — |

## BillboardComponent

**Inherits:** `Engine.Component`

Attached to `Actor.billboardComp` to display stacked upright text and image labels above an Actor's head. For editor configuration, see [Billboard Component](<../../../../Editor User Guide/Actor Components/Billboard.md>).

| Property | Type | Default | Description |
|---|---|---|---|
| `items` | `Engine.BillboardItem[]` | `[]` | Image and text items ordered from top to bottom. |
| `showRange` | `float` | `128` | Maximum player-to-Actor world-position distance in map logical pixels (inclusive). |

### BillboardItem

| Property | Type | Default | Description |
|---|---|---|---|
| `kind` | `string` | `"text"` | `"text"` or `"image"`. |
| `text` | `string` | `""` | Text content. |
| `fontSize` | `uint` | `12` | Text font size. |
| `color` | `sf.Color` | `[255, 255, 255, 255]` | Text RGBA colour. |
| `image` | `string` | `""` | Image resource path. |

Text renders with the system default font and images render at native pixel size. Items align along the Actor head's horizontal centreline (including each line of multiline text) with no extra spacing; at rest, the bottom item meets the top centre of the Actor's image bounds. The stack stays upright regardless of Actor rotation or flipping.

### Visibility and animation

Display requires the player to be within `showRange` and the Actor bounds to intersect the Camera view, subject to Actor/ancestor visibility, map region activity and layer visibility (`tickable` does not affect display). The stack draws after its Actor and attached emitter — allowing later Actors and layers to cover it — and clips to the map window.

Entrance and exit rise and fade over `0.35` seconds using four system curves (default keys use linear interpolation and constant extrapolation):

| Curve resource | Value over `0–0.35` s |
|---|---|
| `Global/BillboardYIn` | `16` → `0` logical pixels |
| `Global/BillboardAlphaIn` | `0` → `255` |
| `Global/BillboardYOut` | `0` → `-16` logical pixels |
| `Global/BillboardAlphaOut` | `255` → `0` |

Animation opacity multiplies item alpha. Reversing a transition continues smoothly from its current position and opacity over a new `0.35` seconds; leaving the Camera view, hiding or sleeping resets the display so returning starts a fresh entrance.

## LightComponent

**Inherits:** `Engine.Component`

Attached to `Actor.lightComp` to emit dynamic point light from an Actor. For editor configuration, see [Light Component](<../../../../Editor User Guide/Actor Components/Light.md>).

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `lightColour` | `sf.Color` | `[255, 255, 255, 255]` | — |
| `lightRadius` | `float` | `16` | — |
| `lightOffset` | `sf.Vector2f` | `[0, 0]` | Measured from the centre of the Actor's local bounds; follows the full Actor transform. |

> **Note:** A wall-mounted emitter needs a separate Actor whose `lightOffset` places the light on the room side, because the renderer does not search for a neighbouring passable cell.
