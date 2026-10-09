# GlobalCore: Map and Fog

## FogController

Direct metadata bases: —

### Properties

No editable properties are declared.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `applyFromMapData` | `function` | mapData: { "GlobalCore", "MapFogSettings" } | — | — |
| `clearFog` | `function` | — | — | — |
| `update` | `function` | deltaTime: float | — | — |
| `drawOverlay` | `function` | — | — | — |

`MapFogSettings` accepts a Lua table with a `fog` string and the numbers `fogPower`, `fogOx`, `fogOy` and `fogDistort`, each of which must be finite and inside the float range. A missing or nil `fog` defaults to `""`, and a missing or nil number defaults to `0`. Extra map fields are ignored. An invalid type or an out-of-range value raises an error. A blank `fog` string or a `fogPower` of `0` or less disables fog. `fogPower` and `fogDistort` are floored and clamped to `0..100`. `fogOx` and `fogOy` set the scroll speed. `applyFromMapData` and `applyWorldFromMapData` share this configuration.

## PanoramaController

Direct metadata bases: —

### Properties

No editable properties are declared.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `applyFromMapData` | `function` | mapData: { "GlobalCore", "MapPanoramaSettings" } | — | — |
| `clear` | `function` | — | — | — |
| `isActive` | `function` | — | return: bool | Pure |

`MapPanoramaSettings` accepts a Lua table with a `panorama` string. A missing or nil `panorama` defaults to `""`. Extra map fields are ignored. An invalid type raises an error. A blank `panorama` disables the underlay. `applyFromMapData` and `applyWorldFromMapData` share this configuration. A non-blank path loads the texture immediately, and a load failure propagates.

## PathResult

Direct metadata bases: —

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `offsets` | `sf.Vector2i[]` | — | — |
| `points` | `sf.Vector2i[]` | — | — |
| `route` | `sf.Vector2i[]` | — | — |

### Functions and events

No Blueprint functions or events are declared.

## GameMapBase

Direct metadata bases: —

### Properties

No editable properties are declared.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `generateDataFromMap` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; size: sf.Vector2u; materialMap: any[][]; smooth: bool | return: sf.Texture | — |
| `findPathExt` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; start: sf.Vector2i; goal: sf.Vector2i; size: sf.Vector2u; movingActor: { "Engine", "Actor" }; excludedAnchors: sf.Vector2i[] = {  } | return: { "GlobalCore", "PathResult" } | — |
| `getMaterialPropertyMapExt` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; width: int; height: int; propertyName: string; invalidValue: any | return: any[][] | — |
| `rebuildPassabilityCache` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; size: sf.Vector2u | return: bool[][] | — |
| `updateActorOccupancy` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; actor: { "Engine", "Actor" } | — | — |
| `getActorsAt` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; x: int; y: int | return: { "Engine", "Actor[]" } | — |
| `getActorsInRange` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; x: int; y: int; radius: int | return: { "Engine", "Actor[]" } | — |
| `getCollisionAt` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; x: int; y: int; selfActor: { "Engine", "Actor" } | return: { "Engine", "Actor[]" } | — |
| `getOverlapsAt` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; x: int; y: int; selfActor: { "Engine", "Actor" } | return: { "Engine", "Actor[]" } | — |
| `setTilemap` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; tilemap: { "Engine", "Tilemap" } | — | — |
| `syncActorsRef` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; actors: any | — | — |
| `setHideDisconnectedRegions` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; enabled: bool | — | — |
| `setVisibilityObserver` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; position: sf.Vector2i? = nil | — | — |
| `isCellVisible` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; position: sf.Vector2i | return: bool | Pure |
| `isActorVisibleOnMap` | `function` | self: { "GlobalCore", "GameMapBase" } = "self"; actor: { "Engine", "Actor" } | return: bool | Pure |
| `getVisibilityRevision` | `function` | self: { "GlobalCore", "GameMapBase" } = "self" | return: int | Pure |

The runtime-only methods of `GameMapBase` (`metadata = false`) cover sparse-region readiness, the 1-based `getSparseWorldRegionIndexAt(position)`, static-light occupancy and batched occlusion.

### Region visibility

For ordinary finite maps, `setHideDisconnectedRegions(enabled)` enables or disables region hiding. `setVisibilityObserver(position)` accepts an `sf.Vector2i` cell for previews; passing `nil` resumes following the player. `isCellVisible(position)` queries the tile display mask, where impassable cells remain visible. `isActorVisibleOnMap(actor)` checks the Actor and each ancestor using their visibility flags and region rules: passable anchors use their region, while impassable anchors require the transformed image rectangle to overlap a visible passable cell. `getHideDisconnectedRegions()` is a runtime-only getter for the current switch. `getVisibilityRevision()` returns the current revision for display and preview caches, tracking terrain changes even while hiding is disabled. Visibility queries update pending changes before returning.

Changing terrain rebuilds connectivity and replacement sources. Moving within one region reuses the mask; moving to another region updates rendering and lighting. The renderer uses separate display layers, keeping the original tilemap intact. The selection rule and gameplay effects are documented in [Global.GameMap](<../../Global APIs/GameMap.md#disconnected-regions>).

## Static preview rendering

`GlobalCore.PreviewSprite` and `GlobalCore.GameMapRenderer` are runtime-only Lua APIs (`metadata = false`), not Blueprint nodes. `PreviewSprite.new(table)` accepts these display fields:

| Fields | Meaning and defaults |
|---|---|
| `layer`, `texture`, `rect` | Layer name, retained `sf.Texture` or `nil`, and `sf.IntRect` source rectangle. |
| `position`, `mapPosition`, `translation` | Pixel anchor, region-visibility cell, and pixel display offset. |
| `rotation`, `scale`, `origin` | Degrees (default `0`), scale (default `{1, 1}`), and pixel origin. |
| `visible`, `parentIndex` | Own visibility (default `true`); zero-based earlier record index, or `-1` for a root. |
| `hue`, `shaderPath` | Hue in degrees (default `0`) and fragment shader path (default empty). |

`renderer:setPreviewSprites(records)` requires a renderer constructed with `previewOnly = true`. It retains the display resources and preserves record order within each configured layer. Parents must precede children; invalid parent indices fail without replacing the installed records. Explicitly installing an empty array draws no static sprites. Until records are installed, the renderer retains its ordinary Actor rendering contract.

`renderer:setPreviewVisibility(flags)` requires installed records and exactly one boolean per record, in the same order. It updates own visibility; effective visibility also includes ancestors and the map's region rules. A `nil` texture draws nothing but retains the record's geometry and parent role. Geometry is already resolved into map coordinates; `parentIndex` controls visibility, not transform composition.

Static shader drawing supports hue, uses `texture`, `textureSize`, `textureRect` and `time = 0`, and combines sprite transforms with the caller's render states, including world offsets. A shader-load failure reports the error and draws the image in magenta. The native-only `GameMapBase::isSpriteVisibleOnMap(position, bounds, transform)` shares the geometry visibility test with live Actors; it is not a Lua binding.

## WorldStreamingState

`WorldRegionState` exposes `Unloaded`, `Reading`, `Prepared`, `Active` and `Dormant`. `WorldRegionDemand` exposes `None`, `Prepared` and `Active`. `WorldStreamingState` uses 1-based indices and owns the demand queues, Camera ordering, last-used and cache accounting, eviction and statistics. Camera ordering places Active regions first, then orders by squared centre distance and breaks ties by region index. The methods of `WorldStreamingState` are `metadata = false`. Blueprint uses contextual world-map nodes.

Lua calls `updateCameraCenter` and `updateDemand`, and consumes `takeReadBatch` and `takePublishItem`. Lua reports read and publish completion or cancellation, marks activation, deactivation and eviction, and reads `getEvictionList` and `getStats`. Files, builders and payloads remain Lua-owned.
