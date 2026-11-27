# Global.GameMap

`Global.GameMap` declares the project map-level API for Player and Actor lookup, Actor creation and destruction, passability, camera binding, tilemaps, terrain tiles and the map environment. Instance method tables on this page omit the implicit `self` receiver (`Global.GameMap.GameMap`), which Blueprint nodes expose as the first target pin.

Metadata source: `Scripts/Global/GameMap_meta.lua`

## Map data and region builders

Shared map data declarations belong to `Global.GameMap.*` and `Global.WorldGameMap.*` in their matching stubs. These are Lua annotation types; they add no constructors and do not change the map file format.

| Namespace | Types |
|---|---|
| `Global.GameMap` | `MapDataBase`, `SerializedMapData`, `MapData`, `MapLayerData`, `MapEnvironmentData`, `SerializedMapFogSettings` |
| `Global.GameMap` | `SerializedTileCell`, `SerializedAutoTileCell`, `SerializedTileRow`, `SerializedAutoTileRow` |
| `Global.WorldGameMap` | `WorldMapData`, `WorldRegionData`, `WorldRegionEnvironmentData` |

`SerializedMapData` describes loaded map records, whereas `MapData` describes normalised Actor data and native environment values. The project module `Source.SceneComponents.MapBuilder.MapDataParser` provides shared parsing operations for ordinary maps and streamed child maps:

- `ValidateMapData(data)` checks a `Global.GameMap.SerializedMapData` record against the [map layer-order rules](<../../../Editor User Guide/Tilesets Autotiles and Maps.md#tilesets-and-autotiles>). It raises an error for invalid data without modifying the record.
- `NormaliseActorData(actorData)` returns a new `Global.Data.ActorData` record that preserves `bp` and `tag` and converts `position` to native unsigned cell coordinates (`sf.Vector2u`). Missing positions or coordinates default to zero.

`NormaliseMap` applies both operations to an ordinary map. A streamed region validates its map data when construction starts, then normalises each authored Actor incrementally rather than in one initial pass.

The world facade receives `WorldMapData` and an injected factory matching this signature:

```lua
---@param region Global.WorldGameMap.WorldRegionData
---@param data Global.GameMap.SerializedMapData
---@param priorityRect Global.WorldGeometry.CellRect | nil
---@return Global.WorldGameMap.RegionBuildState
```

`RegionBuildState` exposes build progress, the region payload, and `step`, `prepareActors`, `prepareRect`, `areActorsReady`, `isCellReady` and `isRectReady`. `Global` consumes this interface without importing the project's builder. `Source.SceneComponents.MapBuilder.WorldRegionBuildState.State` extends it with `layerBuildStates` (elements of type `Source.SceneComponents.WorldLayerBuildState`). Project Actor descriptors, persistence records, layer construction state and floor-preview caches stay in `Source`.

Region builders also implement the resource-lifetime contract:

| Member | Contract |
|---|---|
| `waitingForResources` | `true` when the last build step returned because requested resources were pending. A later step can retry; waiting does not mark the region ready. |
| `setResourcePriority(priority)` | Updates current pending subscriptions and the priority for dependencies discovered later. The world supplies `1` for Active or explicitly forced regions and `0` for Prepared regions. |
| `resolvePendingResources()` | Synchronously finishes already-discovered pending resource loads for an explicit drain. It does not advance layer construction, Actors or publication; those proceed through normal builder steps. |
| `cancel()` | Releases resource subscriptions and closes the construction coroutine. Safe to call repeatedly when publication is cancelled, the region is evicted or the world is disposed. A cancelled builder cannot advance again. |

Texture loading during map and region construction follows these rules:

- **Ordinary maps** queue distinct tileset and autotile textures across all layers before resolving them for layer construction.
- **Streamed regions** request tileset and autotile textures as dependency scanning discovers them, including autotiles introduced by saved terrain changes. The default builder collects dependencies across layers before constructing those layers, reuses duplicate requests and reads autotile frame counts only after the textures are ready. [TextureManager](<../Core Modules/GlobalCore/Resource Managers.md#texturemanager>) owns the request, error and memory semantics.
- **Background streaming budgets:** A resource wait ends the current builder step, and a waiting region is attempted at most once per logic update so other regions retain budget. Required texture failures propagate rather than producing a partial layer.
- **Synchronous drains:** `ensureRegionLoadedAt` and explicit viewport preparation resolve pending resources while draining the builder, whereas background preparation uses its normal time budget.

## GameMap

**Inherits:** `GlobalCore.GameMapBase`

A `getGameMap` output connects to parameters declared as `GlobalCore.GameMapBase`, including `Camera.setMap`. `setTerrainTile` and `setTerrainTiles` expose Blueprint execution pins because they modify terrain, and their result pins report the changes.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `HideDisconnectedRegions` | `bool` | `false` | — |
| `DefaultCoverAlpha` | `int` | — | — |
| `MapViewRect` | `sf.IntRect` | `[192, 32, 416, 416]` | — |

### Composite-world facade

For world manifests, `WorldGameMap` exposes world coordinates backed by a native sparse-region index that supplies passability, Dir4, materials and terrain for placed children. Holes are passable, and queries do not load distant children.

- `getTilemap()` returns sparse backing storage rather than a flattened world.
- `getActorByTag` performs no I/O and searches Active or previously activated resident roots.
- `getGameplayCellRect()` returns a copied cell rectangle without loading regions: full bounds for an ordinary map, or Active bounds for a world map (falling back to visible bounds until the Active area exists).
- `prepareViewportAt` queues the required camera neighbourhood's cold child maps together, reuses reads already in flight, completes their Actors and publication, and renders the destination before returning. Surrounding placements continue streaming asynchronously; failures in a required child, Actor or publication propagate.

### Rendering and fog

Ordinary maps, composite worlds and floor previews delegate map content to the shared native `MapRenderer`. The game adapter preserves Actor callbacks, particles, billboards, layer effects and ordinary-map player cover. World regions draw in global layer order, and their Actor lights and tile blockers share world coordinates so lighting crosses placement boundaries.

Map lights come from `Actor.lightComp`, while ambient light uses `getAmbientLight` and `setAmbientLight`.

Each `GameMap` owns a separate fog controller. The runtime Lua methods `getFogSettings()` and `setFogSettings(settings)` read a copy of or replace its `Engine.MapFogSettings`. `getRegionFogSettings(path)` returns a world child's settings, or `nil` when it has no resident configuration.

> **Note:** Mutating a settings value returned by `getFogSettings()` does not modify the map. Pass the updated value to `setFogSettings(settings)`.

### Actor visibility

`getAllActors` and tag lookups include hidden instances. Position and range queries, collision, overlap and pathfinding exclude Actors hidden by their own `visible` flag or by an ancestor's. Rendering also excludes their attached lights and light blocking.

The Game project filters hidden Actors from enemy lists and movement hazards. Visibility changes refresh cached danger before the next query without waiting for a Tick.

### Disconnected regions

`GameMap.HideDisconnectedRegions` enables native region hiding for ordinary finite maps. Configure its startup value with [`hideDisconnectedRegions` in Gameplay](<../../Default Gameplay/Runtime Data Configuration and Saves.md#shared-gameplay-configuration>), set the module default before constructing maps, or call `setHideDisconnectedRegions(enabled)` on an existing map. `GameMap` skips this default for `sparseWorldConfig`; calling the native setter with `true` on a sparse world raises an error.

Tile connectivity and replacement follow these rules:

- **Connectivity:** Regions use four-way connectivity based solely on tile passability, determined by the topmost visible layer with tile content in each cell. Map edges bound connectivity; doors and other Actors do not divide regions.
- **Hidden-cell replacement:** The player's region stays visible, while other passable cells display the tile layers of the nearest impassable cell using Manhattan distance (ties break top to bottom, then left to right), preserving source-cell texture, autotile shape and animation. Impassable cells keep their own display.
- **Missing observer:** Without a valid observer (no player, an out-of-bounds position or an impassable observer cell), all passable cells and all Actors are hidden while walls stay visible. Previews supply an explicit observer cell.

Actor visibility follows these geometric rules:

- **Passable anchors:** Actors anchored on passable cells follow their anchor cell's region.
- **Impassable anchors:** The Actor's local image rectangle is transformed by its position, translation, origin, rotation and scale; the resulting quadrilateral must overlap a visible passable cell with positive area (touching an edge is insufficient). Images entirely inside walls or with zero area stay hidden. This allows wall-mounted torches to follow the room their graphics enter.
- **Hierarchy and extent:** The test is geometric rather than per-pixel alpha, and an image spanning multiple regions draws in full whenever it overlaps the visible region. Every ancestor must independently pass the rule and its own `visible` flag; a hidden parent hides all descendants.

Entering a region reveals it and leaving hides it again without storing exploration state in saves. Rendering suppresses hidden-region Actors, their descendants, attached particles and lights, and damage text in hidden cells, without altering Actor `visible` flags, terrain data, collision, pathfinding or events. Enemy Book filters through `isActorVisibleOnMap(actor)` before grouping enemies.

Visibility queries refresh after terrain edits or observer movement without waiting for drawing. For the native observer and visibility APIs, see [GameMapBase](<../Core Modules/GlobalCore/Map and Fog.md#gamemapbase>).

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getPlayer` | — | `player: Engine.Actor` | Pure. |
| `setPlayer` | `player: Engine.Actor` | — | Exec (`default`). |
| `getAllActors` | — | `actors: Engine.Actor[]` | Pure. |
| `getActorsByPosition` | `position: sf.Vector2i` | `actors: Engine.Actor[]` | Pure. |
| `getActorByLayerAndPosition` | `layer: string`, `position: sf.Vector2i` | `actor: Engine.Actor` | Pure. |
| `getActorsByRange` | `position: sf.Vector2i`, `radius: int` | `actors: Engine.Actor[]` | Pure. |
| `getActorByTag` | `tag: string` | `actor: Engine.Actor` | Pure. |
| `isPassable` | `actor: Engine.Actor`, `targetPosition: sf.Vector2i` | `passable: bool` | Pure. |
| `spawnActor` | `actor: Engine.Actor`, `layer: string`, `emitCreateEvent: bool = true` | — | Exec (`default`). |
| `createActor` | `actorClass: Engine.Actor`, `layer: string`, `kwargs: any`, `emitCreateEvent: bool = true` | `actor: Engine.Actor` | Exec (`default`). |
| `destroyActor` | `actor: Engine.Actor` | — | Exec (`default`). |
| `getCamera` | — | `camera: GlobalCore.Camera` | Pure. |
| `setCamera` | `camera: GlobalCore.Camera` | — | Exec (`default`). |
| `getTilemap` | — | `tilemap: Engine.Tilemap` | Pure. |
| `getTerrainTile` | `layerName: string`, `position: sf.Vector2i` | `tileID: any` | Pure. |
| `getTerrainTilePositions` | `layerName: string`, `tileID: any` | `positions: sf.Vector2i[]` | Pure. |
| `setTerrainTile` | `layerName: string`, `position: sf.Vector2i`, `tileID: any` | `success: bool` | Exec (`default`). |
| `setTerrainTiles` | `layerName: string`, `positions: sf.Vector2i[]`, `tileID: any` | `positions: sf.Vector2i[]` | Exec (`default`). |
| `getAmbientLight` | — | `ambientLight: sf.Color` | Pure. |
| `setAmbientLight` | `ambientLight: sf.Color` | — | Exec (`default`). |
| `getSize` | — | `size: sf.Vector2u` | Pure. |
| `getTopMaterial` | `pos: sf.Vector2i` | `topMaterial: string?` | Pure. |
| `findPath` | `start: sf.Vector2i`, `goal: sf.Vector2i`, `actor: Engine.Actor`, `excludedAnchors: sf.Vector2i[] = {}` | `path: sf.Vector2i[]` | Pure. |
| `isPathfindingPassable` | `actor: Engine.Actor`, `targetPosition: sf.Vector2i` | `passable: bool` | Pure. |
| `hasPathBlockingOverlapActor` | `actor: Engine.Actor`, `targetPosition: sf.Vector2i` | `hasActor: bool` | Pure. |
| `getScene` | — | `scene: GlobalCore.SceneBase` | Pure. |
| `addCommonTip` | `text: string` | — | Exec (`default`). |
| `addDamageText` | `text: string`, `position: sf.Vector2f` | — | Exec (`default`). |
