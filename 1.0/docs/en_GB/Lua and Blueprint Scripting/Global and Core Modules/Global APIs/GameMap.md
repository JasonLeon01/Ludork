# Global.GameMap

`GameMap` declares the map-level API: Player and Actor lookup, Actor creation and destruction, passability, the camera, the tilemap, terrain tiles and the map environment.

Metadata source: `Scripts/Global/GameMap_meta.lua`

## Map data and region builders

Shared map data declarations belong to `Global.GameMap.*` and `Global.WorldGameMap.*`, in their matching stubs. These are Lua annotation types; their names do not add constructors or change the map file format.

| Namespace | Types |
|---|---|
| `Global.GameMap` | `MapDataBase`, `SerializedMapData`, `MapData`, `MapLayerData`, `MapEnvironmentData`, `SerializedMapFogSettings` |
| `Global.GameMap` | `SerializedTileCell`, `SerializedAutoTileCell`, `SerializedTileRow`, `SerializedAutoTileRow` |
| `Global.WorldGameMap` | `WorldMapData`, `WorldRegionData`, `WorldRegionEnvironmentData` |

`SerializedMapData` describes loaded map records; `MapData` describes normalised Actor data and native environment values. The project module `Source.SceneComponents.MapBuilder.MapDataParser` provides the same parsing operations for ordinary maps and streamed child maps:

- `ValidateMapData(data)` checks a `Global.GameMap.SerializedMapData` record against the [map layer-order rules](<../../../Editor User Guide/Tilesets Autotiles and Maps.md#tilesets-and-autotiles>). It raises an error for invalid data without modifying the record.
- `NormaliseActorData(actorData)` returns a new `Global.Data.ActorData` record, retaining `bp` and `tag` and converting `position` to native unsigned cell coordinates (`sf.Vector2u`). A missing position or coordinate defaults to zero.

`NormaliseMap` uses both operations for an ordinary map. A streamed region validates its map data when construction starts, then normalises each authored Actor as part of its incremental work; it does not normalise every Actor in one initial pass.

The world facade receives `WorldMapData` and an injected factory with this contract:

```lua
---@param region Global.WorldGameMap.WorldRegionData
---@param data Global.GameMap.SerializedMapData
---@param priorityRect Global.WorldGeometry.CellRect | nil
---@return Global.WorldGameMap.RegionBuildState
```

`RegionBuildState` exposes build progress, the region payload, and `step`, `prepareActors`, `prepareRect`, `areActorsReady`, `isCellReady` and `isRectReady`. Global consumes this interface without importing the project's builder. `Source.SceneComponents.MapBuilder.WorldRegionBuildState.State` extends it with `layerBuildStates`, whose elements are `Source.SceneComponents.WorldLayerBuildState`. Project Actor descriptors, persistence records, layer construction state and floor-preview caches remain in Source.

## GameMap

Direct metadata bases: `{ "GlobalCore", "GameMapBase" }`

A `getGameMap` output can connect to parameters declared as `GlobalCore.GameMapBase`, including `Camera.setMap`. The methods below use an explicit GameMap receiver. `setTerrainTile` and `setTerrainTiles` have execution inputs and outputs because they modify terrain; their result pins report the changes.

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `HideDisconnectedRegions` | `bool` | `false` | — |
| `DefaultCoverAlpha` | `int` | — | — |
| `MapViewRect` | `sf.IntRect` | `{ [1] = { [1] = 192, [2] = 32, [3] = 416, [4] = 416 } }` | — |

### Composite-world facade

For world manifests, `WorldGameMap` exposes world coordinates. A native sparse-region index supplies passability, Dir4, materials and terrain for placed children. Holes are passable, and queries do not load distant children.

`getTilemap()` returns sparse backing storage rather than a flattened world. `getActorByTag` performs no I/O and searches the Active or previously activated resident roots.

`getGameplayCellRect()` returns a copied cell rectangle without loading regions. The result is the full bounds for an ordinary map. For a world map it is the Active bounds, and it falls back to the visible bounds until the Active area exists.

`prepareViewportAt` prepares and renders the required camera neighbourhood synchronously, while surrounding placements remain asynchronous. Failures of required children, Actors or publications propagate.

### Rendering and fog

Ordinary maps, composite worlds and floor previews delegate map content to the shared native `MapRenderer`. The game adapter preserves Actor callbacks, particles, billboards, layer effects and ordinary-map player cover. World regions are drawn in global layer order, and their Actor lights and tile blockers share world coordinates, so lighting crosses placement boundaries.

Map lights come from `Actor.lightComp`. Ambient light uses `getAmbientLight` and `setAmbientLight`.

Each `GameMap` owns a separate fog controller. The runtime Lua methods `getFogSettings()` and `setFogSettings(settings)` read a copy of, or replace, its `Engine.MapFogSettings`. `getRegionFogSettings(path)` returns a world child's settings, or `nil` when it has no resident configuration. Mutating a returned settings value does not change the map; pass the changed value to `setFogSettings`.

### Actor visibility

`getAllActors` and tag lookups retain hidden instances. Position and range queries, collision, overlap and pathfinding exclude Actors hidden by their own `visible` flag or by an ancestor's. Rendering also excludes their attached lights and light blocking. The Game project filters these Actors from enemy lists and movement hazards. A visibility change refreshes cached danger before the next query, without waiting for a Tick.

### Disconnected regions

`GameMap.HideDisconnectedRegions` enables native region hiding for ordinary finite maps. Set this module default during system initialisation, before constructing maps; `setHideDisconnectedRegions(enabled)` controls an existing map. `GameMap` skips this default for `sparseWorldConfig`; calling the native setter with `true` on a sparse world raises an error, so composite worlds retain their normal rendering.

Regions use four-way connectivity and only tile passability: the topmost visible layer with tile content determines each cell. Map edges bound connectivity; doors and other Actors do not divide regions. The player's region remains visible, and other passable cells display the nearest impassable cell's tile layers. Distance is Manhattan distance, with ties resolved from top to bottom, then left to right. Source texture, autotile shape and animation are preserved. Impassable cells retain their original display. If no valid observer exists (no player, an out-of-bounds position, or an impassable observer cell), all passable cells are hidden; walls remain visible. Previews supply an explicit observer cell.

Actors anchored on passable cells follow their anchor's region. For an Actor anchored on an impassable cell, its local image rectangle is transformed by its position, translation, origin, rotation and scale; the resulting quadrilateral must overlap a visible passable cell with positive area. This lets wall-mounted torches follow the room into which their images extend. Touching a cell boundary is insufficient. An image entirely within walls, or with no area, remains hidden. This is a geometric rectangle test, not a per-pixel alpha test. An image spanning multiple regions is drawn in full whenever it overlaps the current region. Each ancestor must independently pass the same rule and its visibility flag; a hidden parent also hides its descendants. Without a valid observer, all Actors are hidden.

Entering a region reveals it; leaving hides it again, without recording exploration in saves. Rendering suppresses hidden-region Actors and their descendants, attached particles and lights, and damage text in hidden cells. This display rule does not change Actor `visible` flags, terrain data, collision, pathfinding or events. Enemy Book filters through `isActorVisibleOnMap(actor)` before grouping enemies, so a visible instance still represents its enemy type.

Visibility queries refresh after terrain edits or observer movement without waiting for drawing. Native observer and visibility APIs are described under [GameMapBase](<../Core Modules/GlobalCore/Map and Fog.md#gamemapbase>).

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `getPlayer` | `function` | self: { "Global.GameMap", "GameMap" } = "self" | player: { "Engine", "Actor" } | Pure |
| `setPlayer` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; player: { "Engine", "Actor" } | — | ExecSplit { [1] = "default", default = nil } |
| `getAllActors` | `function` | self: { "Global.GameMap", "GameMap" } = "self" | actors: { "Engine", "Actor[]" } | Pure |
| `getActorsByPosition` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; position: sf.Vector2i | actors: { "Engine", "Actor[]" } | Pure |
| `getActorByLayerAndPosition` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; layer: string; position: sf.Vector2i | actor: { "Engine", "Actor" } | Pure |
| `getActorsByRange` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; position: sf.Vector2i; radius: int | actors: { "Engine", "Actor[]" } | Pure |
| `getActorByTag` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; tag: string | actor: { "Engine", "Actor" } | Pure |
| `isPassable` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; actor: { "Engine", "Actor" }; targetPosition: sf.Vector2i | passable: bool | Pure |
| `spawnActor` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; actor: { "Engine", "Actor" }; layer: string; emitCreateEvent: bool = true | — | ExecSplit { [1] = "default", default = nil } |
| `createActor` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; actorClass: { "Engine", "Actor" }; layer: string; kwargs: any; emitCreateEvent: bool = true | actor: { "Engine", "Actor" } | ExecSplit { [1] = "default", default = nil } |
| `destroyActor` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; actor: { "Engine", "Actor" } | — | ExecSplit { [1] = "default", default = nil } |
| `getCamera` | `function` | self: { "Global.GameMap", "GameMap" } = "self" | camera: { "GlobalCore", "Camera" } | Pure |
| `setCamera` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; camera: { "GlobalCore", "Camera" } | — | ExecSplit { [1] = "default", default = nil } |
| `getTilemap` | `function` | self: { "Global.GameMap", "GameMap" } = "self" | tilemap: { "Engine", "Tilemap" } | Pure |
| `getTerrainTile` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; layerName: string; position: sf.Vector2i | tileID: any | Pure |
| `getTerrainTilePositions` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; layerName: string; tileID: any | positions: sf.Vector2i[] | Pure |
| `setTerrainTile` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; layerName: string; position: sf.Vector2i; tileID: any | success: bool | ExecSplit { [1] = "default", default = nil } |
| `setTerrainTiles` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; layerName: string; positions: sf.Vector2i[]; tileID: any | positions: sf.Vector2i[] | ExecSplit { [1] = "default", default = nil } |
| `getAmbientLight` | `function` | self: { "Global.GameMap", "GameMap" } = "self" | ambientLight: sf.Color | Pure |
| `setAmbientLight` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; ambientLight: sf.Color | — | ExecSplit { [1] = "default", default = nil } |
| `getSize` | `function` | self: { "Global.GameMap", "GameMap" } = "self" | size: sf.Vector2u | Pure |
| `getTopMaterial` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; pos: sf.Vector2i | topMaterial: string? | Pure |
| `findPath` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; start: sf.Vector2i; goal: sf.Vector2i; actor: { "Engine", "Actor" }; excludedAnchors: sf.Vector2i[] = {  } | path: sf.Vector2i[] | Pure |
| `isPathfindingPassable` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; actor: { "Engine", "Actor" }; targetPosition: sf.Vector2i | passable: bool | Pure |
| `hasPathBlockingOverlapActor` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; actor: { "Engine", "Actor" }; targetPosition: sf.Vector2i | hasActor: bool | Pure |
| `getScene` | `function` | self: { "Global.GameMap", "GameMap" } = "self" | scene: { "GlobalCore", "SceneBase" } | Pure |
| `addCommonTip` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; text: string | — | ExecSplit { [1] = "default", default = nil } |
| `addDamageText` | `function` | self: { "Global.GameMap", "GameMap" } = "self"; text: string; position: sf.Vector2f | — | ExecSplit { [1] = "default", default = nil } |
