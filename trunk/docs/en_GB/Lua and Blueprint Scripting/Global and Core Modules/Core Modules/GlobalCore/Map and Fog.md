# GlobalCore: Map and Fog

This page documents native map services, fog and panorama controllers, pathfinding results, shared and static-preview map rendering, and composite-world streaming state. Instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin; static methods and constructors take no receiver.

## FogController

Construct one `GlobalCore.FogController` per map session with `GlobalCore.FogController.new()`. The controller owns the fog configuration while the shared renderer owns the GPU resources. The Game project attaches a separate controller to each `GameMap`, so configuring one map never overwrites another map's fog.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `new` | — | `GlobalCore.FogController` | Static. |
| `applyFromMapData` | `settings: Engine.MapFogSettings` | — | Replaces the base configuration while keeping existing world regions. |
| `applyWorldFromMapData` | `settings: Engine.MapFogSettings` | — | Lua only. Starts a world configuration by clearing prior regions. |
| `setWorldRegionFog` | `key: string`, `cellRect: sf.IntRect`, `settings: Engine.MapFogSettings` | — | Lua only. Sets regional fog clipped to `cellRect`. |
| `removeWorldRegionFog` | `key: string` | — | Lua only. |
| `clearFog` | — | — | Clears both base and regional fog configuration. |
| `getSettings` | — | `Engine.MapFogSettings` | Returns a copy of the base settings. |
| `getWorldRegionSettings` | `key: string` | `Engine.MapFogSettings \| nil` | Lua only. Returns a settings copy or `nil`. |

Fog has no global `update` or `drawOverlay` calls:

- **Procedural rendering:** Map composition samples GPU procedural noise in world coordinates at the frame's absolute time, after material and light composition and before foreground weather and particles. No fog texture path is required.
- **World layering:** Base world fog composes first, followed by resident child fog clipped to its placement in stable placement order. Matching settings and time produce seamless noise across neighbouring placements.
- **Lighting:** Lit fog samples scene ambient and direct light; unlit fog renders its configured colour. See [Engine.MapFogSettings](<../Engine/Maps and Materials.md#mapfogsettings>) for fields and defaults.

## PanoramaController

Manages the panorama background underlay rendered behind the map. All methods are static.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `applyFromMapData` | `mapData: GlobalCore.MapPanoramaSettings` | — | Static. |
| `clear` | — | — | Static. |
| `isActive` | — | `bool` | Static. Pure. |

`MapPanoramaSettings` accepts a Lua table with a `panorama` string field:

- A missing or `nil` `panorama` defaults to `""`, which disables the underlay.
- Extra map fields are ignored; an invalid type raises an error.
- `applyFromMapData` and `applyWorldFromMapData` share this configuration. A non-blank path loads the texture immediately and propagates load failures.

`getPath()` returns the current panorama path. Panorama rendering runs through the shared compositor with Cover scaling and Camera-dependent scrolling.

## PathResult

Holds the output of `GameMapBase.findPathExt`.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `offsets` | `sf.Vector2i[]` | — | — |
| `points` | `sf.Vector2i[]` | — | — |
| `route` | `sf.Vector2i[]` | — | — |

## GameMapBase

Provides native passability caching, Actor occupancy tracking, pathfinding and disconnected-region visibility for maps.

- **Independent caches:** Terrain passability and active Actor occupancy are cached separately. Actor creation, destruction, view synchronisation and world sleep or wake update occupancy without rebuilding the terrain grid.
- **Incremental sync:** Synchronisation removes Actors leaving the active view and updates only added Actors or changed footprints, keeping unchanged occupancy and the relative order of retained Actors. Hidden Actors stay registered and apply their current visibility during queries.
- **Cache maintenance:** `rebuildPassabilityCache(size)` rebuilds terrain passability, refreshes Actor occupancy and returns the terrain boolean grid (or an empty grid for a sparse world). `invalidatePassabilityCache()` marks terrain passability and disconnected-region visibility for refresh without clearing Actor occupancy; the Game project's terrain-editing APIs call it automatically.
- **Manual occupancy updates:** After manually changing an active Actor's map position or bounds, call `updateActorOccupancy(actor)` to refresh its occupied cells. It does not register an Actor absent from the active view (including a sleeping world Actor). Lifecycle and batch updates flush pending occupancy before subsequent Actor queries.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `generateDataFromMap` | `size: sf.Vector2u`, `materialMap: any[][]`, `smooth: bool` | `sf.Texture` | — |
| `findPathExt` | `start: sf.Vector2i`, `goal: sf.Vector2i`, `size: sf.Vector2u`, `movingActor: Engine.Actor`, `excludedAnchors: sf.Vector2i[] = {}` | `GlobalCore.PathResult` | — |
| `rebuildPassabilityCache` | `size: sf.Vector2u` | `bool[][]` | — |
| `updateActorOccupancy` | `actor: Engine.Actor` | — | — |
| `getActorsAt` | `x: int`, `y: int` | `Engine.Actor[]` | — |
| `getActorsInRange` | `x: int`, `y: int`, `radius: int` | `Engine.Actor[]` | — |
| `getCollisionAt` | `x: int`, `y: int`, `selfActor: Engine.Actor` | `Engine.Actor[]` | — |
| `getOverlapsAt` | `x: int`, `y: int`, `selfActor: Engine.Actor` | `Engine.Actor[]` | — |
| `setTilemap` | `tilemap: Engine.Tilemap` | — | — |
| `syncActorsRef` | `actors: any` | — | — |
| `setHideDisconnectedRegions` | `enabled: bool` | — | — |
| `setVisibilityObserver` | `position: sf.Vector2i? = nil` | — | — |
| `isCellVisible` | `position: sf.Vector2i` | `bool` | Pure. |
| `isActorVisibleOnMap` | `actor: Engine.Actor` | `bool` | Pure. |
| `getVisibilityRevision` | — | `int` | Pure. |

Runtime-only methods (`metadata = false`) cover sparse-region readiness, the 1-based `getSparseWorldRegionIndexAt(position)`, and region installation and removal.

### Region visibility

For ordinary finite maps, the visibility APIs operate as follows:

- `setHideDisconnectedRegions(enabled)` enables or disables region hiding (`getHideDisconnectedRegions()` is a runtime-only getter).
- `setVisibilityObserver(position)` accepts an `sf.Vector2i` cell for previews; passing `nil` resumes following the player.
- `isCellVisible(position)` queries the tile display mask (impassable cells stay visible).
- `isActorVisibleOnMap(actor)` checks the Actor and every ancestor against their visibility flags and region rules: passable anchors use their region, while impassable anchors require the transformed image rectangle to overlap a visible passable cell.
- `getVisibilityRevision()` returns the current revision for display and preview caches, tracking terrain edits even while hiding is disabled. Visibility queries flush pending updates before returning.

Terrain edits rebuild connectivity and replacement sources. Moving within one region reuses the mask; crossing into another region updates rendering and lighting. The renderer uses separate display layers so the underlying tilemap stays intact. For selection rules and gameplay effects, see [Global.GameMap](<../../Global APIs/GameMap.md#disconnected-regions>).

## Shared map rendering

`GlobalCore.GameMapRenderer` adapts live map and Actor state to Core `MapRenderer`:

- `renderFrame(ambientLight, time, applyPlayerCover, materialRevision, drawLayerEffects, drawOverlay)` renders materials, lighting, reflection, panorama and fog.
- `drawLayerEffects(layerName, target, states)` runs once per layer per frame; when reflection is needed, the renderer captures and reuses its output. `drawOverlay()` runs once per frame for component overlays. Both callbacks draw only; gameplay advances during normal updates.
- `drawContent` preserves supplied render states for offscreen drawing.
- `prewarm(ambientLight, time)` warms the same render path; `setFogController(controller)` attaches the map fog controller and `setLayerOrder(names)` tracks world layer changes.

For the shared renderer and editor preview data flow, see [PreviewHost: Architecture and Data Flow](<../../../../PreviewHost/Architecture and Data Flow.md>). For editor display options, see [Map and World Previews](<../../../../PreviewHost/Map and World Previews.md>).

## Static preview rendering

`GlobalCore.PreviewSprite` and `GlobalCore.GameMapRenderer` are runtime-only Lua APIs (`metadata = false`) and do not create Blueprint nodes. `PreviewSprite.new(table)` accepts these fields:

| Field | Type | Default | Description |
|---|---|---|---|
| `layer` | `string` | — | Target layer name. |
| `texture` | `sf.Texture \| nil` | `nil` | Retained texture, or `nil` to draw nothing while preserving geometry and parent role. |
| `rect` | `sf.IntRect` | — | Source rectangle in texture pixels. |
| `position` | `sf.Vector2f` | — | Pixel anchor in map coordinates. |
| `mapPosition` | `sf.Vector2i` | — | Cell used for region-visibility checks. |
| `translation` | `sf.Vector2f` | — | Pixel display offset. |
| `rotation` | `float` | `0` | Rotation in degrees. |
| `scale` | `sf.Vector2f` | `[1, 1]` | Scale factors. |
| `origin` | `sf.Vector2f` | — | Pixel origin. |
| `visible` | `bool` | `true` | Record's own visibility flag. |
| `parentIndex` | `int` | `-1` | Zero-based index of an earlier parent record, or `-1` for a root. |
| `material` | `string` | `""` | Material resource path. |
| `materialParameters` | `Dict[string, any]` | `{}` | Per-instance material parameter overrides. |

Static preview installation and updates follow these rules:

- `renderer:setPreviewSprites(records)` requires a renderer created with `previewOnly = true`. It retains display resources and preserves record order within each configured layer. Parents must precede children; invalid parent indices fail without replacing the installed records. Passing an empty array draws no static sprites; before records are installed, the renderer uses its ordinary Actor rendering contract.
- `renderer:setPreviewVisibility(flags)` requires installed records and one boolean per record in matching order. It updates each record's own visibility, while effective visibility also applies ancestors and map region rules. Geometry is already resolved into map coordinates; `parentIndex` controls visibility rather than transform composition.
- Static material drawing uses the current frame, instance parameters and supplied absolute time (`0` for floor previews), combining sprite and caller transforms including world offsets. An invalid material raises an error. The native-only `GameMapBase::isSpriteVisibleOnMap(position, bounds, transform)` shares the geometry visibility test with live Actors and is not bound to Lua.

## WorldStreamingState

Tracks demand queues, Camera ordering, LRU cache accounting, eviction and streaming statistics for composite worlds using 1-based region indices. All methods are runtime-only (`metadata = false`); Blueprint uses contextual world-map nodes.

- **Enums:** `WorldRegionState` exposes `Unloaded`, `Reading`, `Prepared`, `Active` and `Dormant`. `WorldRegionDemand` exposes `None`, `Prepared` and `Active`.
- **Priority ordering:** Camera ordering places Active regions first, then orders by squared centre distance, breaking ties by region index.
- **Lua loop integration:** Lua calls `updateCameraCenter` and `updateDemand`, consumes `takeReadBatch` and `takePublishItem`, reports read and publish completion or cancellation, marks activation, deactivation and eviction, and queries `getEvictionList` and `getStats`. Files, builders and payloads stay Lua-owned.
