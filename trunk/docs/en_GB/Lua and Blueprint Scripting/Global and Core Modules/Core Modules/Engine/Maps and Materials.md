# Engine: Maps and Materials

This page documents map fog settings (`MapFogSettings`), tile layers and tilemaps (`TileLayer`, `Tilemap`, `TileLayerData`), tileset/autotile definitions (`AutoTile`, `Tileset`), and the 2D material graph and graphics cache preparation APIs. Instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin; constructors and static methods take no receiver.

Material references use complete `Data/Materials/...json` paths, while `AutoTile.fileName` and `Tileset.fileName` use `/Game/Assets/Autotiles/...` and `/Game/Assets/Tilesets/...` texture paths.

## MapFogSettings

`Engine.MapFogSettings.new(table)` creates the typed fog configuration consumed by maps and the native renderer. Omitted fields keep these defaults:

| Property | Type | Default | Description |
|---|---|---|---|
| `enabled` | `bool` | `false` | Enables the fog layer. |
| `colour` | `sf.Color` | `[255, 255, 255, 255]` | Fog colour and alpha (opaque white). |
| `density` | `float` | `0.5` | Density in `[0, 1]`; `0` has no visible effect. |
| `scale` | `float` | `256` | Positive noise scale in world pixels. |
| `velocity` | `sf.Vector2f` | `[15, 15]` | Drift velocity in world pixels per second. |
| `turbulence` | `float` | `0.5` | Noise warping strength in `[0, 1]`. |
| `seed` | `int` | `0` | Signed 32-bit deterministic seed. |

Numerical fields must be finite. Native Lua construction accepts `sf.Color` and `sf.Vector2f` values; serialised map and world JSON store a complete `fog` object containing all seven fields, with `colour` as four integer channels in `[0, 255]` and `velocity` as a two-number array. Unknown fields and the legacy texture fog format are rejected by the map data parser.

```lua
local settings = Engine.MapFogSettings.new({
    enabled = true,
    colour = sf.Color.new(210, 225, 240, 255),
    density = 0.4,
    scale = 256,
    velocity = sf.Vector2f.new(15, 15),
    turbulence = 0.5,
    seed = 42
})
gameMap:setFogSettings(settings)
```

## TileLayer

**Inherits:** `Engine.TileLayerGraphics`

Stores chunked tile and autotile geometry organised into `32×32`-cell chunks so only chunks intersecting the view draw. Constructing with `deferred = true` enables incremental runtime chunk building; sparse-world streaming queries chunk readiness via `isCellBuilt(position)` (`metadata = false`).

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `visible` | `bool` | `true` | — |
| `material` | `string` | — | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getName` | — | `name: string` | Pure. |
| `getTiles` | — | `tiles: int[][]` | Pure. |
| `getAutoTiles` | — | `autoTiles: any[][]` | Pure. |
| `getAutoTilePool` | — | `pool: Engine.AutoTile[]` | Pure. |
| `getVisible` | — | `bool` | Pure. |
| `setVisible` | `visible: bool` | — | — |
| `hasContent` | `position: sf.Vector2i` | `bool` | Pure. |
| `isDirectionPassable` | `position: sf.Vector2i`, `directionIndex: int` | `bool` | Pure. |
| `getGridSize` | — | `sf.Vector2u` | Pure. |
| `buildNextChunk` | — | `bool` | Lua only (`metadata = false`). |
| `buildChunk` | `chunkX: int`, `chunkY: int` | `bool` | Lua only (`metadata = false`). |
| `isChunkBuilt` | `chunkX: int`, `chunkY: int` | `bool` | Lua only (`metadata = false`). |
| `isBuildComplete` | — | `bool` | Lua only (`metadata = false`). |

C++-only renderer methods (`TileLayer::updateBlock` and `setAnimationTime(seconds, interval)`) are not bound to Lua: `updateBlock` updates a rectangular cell block and rebuilds affected built chunks including autotile neighbours, while `setAnimationTime` samples animation absolutely so rendering a thumbnail at time zero does not advance another view. Tile geometry, colour changes and actual animation frame changes invalidate rendering caches.

## Tilemap

Container of ordered `Engine.TileLayer` instances forming a map's tile geometry.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `addLayer` | `layer: Engine.TileLayer` | — | — |
| `getLayer` | `name: string` | `layer: Engine.TileLayer` | Pure. |
| `getTilesData` | — | `tiles: any` | Pure. |
| `getAllLayers` | — | `layers: any` | Pure. |
| `getLayerNameList` | — | `layerNames: string[]` | Pure. |
| `getAutoTilesData` | — | `autoTiles: any` | Pure. |
| `getSize` | — | `size: sf.Vector2u` | Pure. |
| `updateAutoTileAnimation` | `deltaTime: float`, `frameInterval: float` | — | — |

## AutoTile

Describes one autotile asset entry, its passability flag and optional base material.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `name` | `string` | `""` | — |
| `fileName` | `string` | `""` | `/Game/Assets/Autotiles/...` path. |
| `passable` | `bool` | `true` | — |
| `material` | `string` | `""` | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `asDict` | — | `any` | — |
| `fromData` | `data: any` | `Engine.AutoTile` | Static. |

## Material resources

Authored materials live under `Data/Materials/<name>.json` with `type: "material"` and `version: 1`.

- **Defaults and paths:** The shared standard resource is `Data/Materials/Global/Standard.json`, selected automatically for an empty Actor material reference. Effect examples and `Surfaces` presets live directly under `Data/Materials` and `Data/Materials/Surfaces`. An empty base material on a tile or autotile selects the built-in neutral surface; an invalid non-empty reference fails. Auxiliary textures use canonical `/Game/Assets/...` paths, while the host supplies sprite texture, current frame, transform and time.
- **Prepared runtime cache:** Initialisation generates shaders and runtime metadata in the user data directory, and rendering loads those prepared files (including encrypted forms). Preparation covers every individual material plus base/layer combinations discovered from saved maps and the editor's [additional combinations](<../../../../Editor User Guide/Materials.md#material-combinations>).
- **Dynamic combinations:** Register combinations in advance if they only appear in dynamically constructed maps or runtime layer-material changes (Actor/UI material switches and parameter edits need no extra entry). Drawing an unprepared combination fails rather than compiling during rendering.
- **Native C++ preview overrides:** `MaterialResource::load` loads prepared runtime resources, while `MaterialResource::fromData` builds a resource from a graph definition for cache generation or preview compilation. Preview integrations can keep a `MaterialResource::Overrides` snapshot and activate it per request via `ScopedOverrides` on the same preview thread (replace when documents change; scopes do not persist between requests). See [Runtime generation](<../../../../Editor User Guide/Materials.md#runtime-generation>).

| File section | Contents |
|---|---|
| `parameters` | Array of stable `id`, `type`, `default`, and optional `displayName`, `group`, `min` and `max`. |
| `graph.nodes` | Nodes with `id`, official `type`, `inputs` and `properties`. Each input is a literal or `{ "node": "id", "port": "output" }`. |
| `graph.output` | ID of the `Surface` output node. |
| `rendering.maxVertexOffset` | Maximum absolute displacement `[x, y]` in local pixels; defaults to `[0, 0]`. |
| `editor.positions` | Node ID to canvas position map; editor data only. |

Parameter types are `bool`, `float`, `vec2`, `vec3`, `vec4`, `color` and `texture` (vectors are numeric arrays; colours are four normalised channels in `[0, 1]`). Optional `displayName` changes only the editor label, leaving the stable `id` unchanged. Invalid types, unknown parameter IDs and out-of-range values fail. Material graphs are typed, acyclic data-flow graphs with no gameplay execution pins or custom GLSL nodes.

### Planning graphics caches

After setting the application name, call `Engine.planGraphicsCaches()` to inspect material outputs and default shader programs together. It returns a Lua-only `Engine.GraphicsCachePlan` (repeated calls in the same runtime share the active plan). Poll `getStatus()` across successive frames:

| Status | Action |
|---|---|
| `"planning"` | Wait for input and cache checks to finish. |
| `"planned"` | Read `needsMaterialExport()` and `needsShaderCompilation()` to select progress labels, then call `startMaterials()` even on a cache hit. |
| `"materials"` | Material publication is running. |
| `"materialsReady"` | Call `startShaders()` (cache hits still restore prepared shader programs). |
| `"shaders"` | Shader restoration or compilation is running. |
| `"ready"` | Call `getResult()` to finish. |
| `"failed"` or `"cancelled"` | `getResult()` raises the stored failure or cancellation state error. |

`getProgress()` returns the active stage's fraction in `0–1`. Start each stage only from its waiting state, and read both work flags only after planning succeeds.

> **Important:** The plan holds the graphics preparation lock while waiting in `"planned"` and `"materialsReady"`, blocking other preparations. Always call `cancel()` to stop the worker and release the lock if you abandon the plan or leave the scene early. For custom stage combinations, use [shader preparation](#shader-preparation-in-mrb).

### Material cache preparation

`Engine.prepareMaterialCache()` starts background material preparation and returns a Lua-only `Engine.MaterialCacheRequest` (no Blueprint nodes). Call it after setting the application name and finish before creating material-dependent graphics; it produces GLSL and runtime metadata without precompiling GPU programs.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getStatus` | — | `string` | Lua only. Returns `"pending"`, `"ready"`, `"failed"` or `"cancelled"` without waiting. |
| `getProgress` | — | `float` | Lua only. Returns preparation progress in `0–1`. |
| `getResult` | — | — | Lua only. Returns nothing on Ready, rethrows the stored error on Failed, or raises a state error on Pending/Cancelled. |
| `cancel` | — | — | Lua only. Requests cancellation and waits for the worker to finish. |

Poll across frames so the initialisation screen can update, call `getResult()` on Ready or Failed to propagate errors, and cancel the request when leaving the initialisation scene. See [Runtime generation](<../../../../Editor User Guide/Materials.md#runtime-generation>) for cache policy and encryption settings.

### Shader preparation in MRB

`Engine.prepareShaderCache(additionalPrograms = {})` returns a Lua-only `Engine.ShaderCacheRequest` (no Blueprint node) sharing the `getStatus`, `getProgress`, `getResult` and `cancel` contract from [Material cache preparation](#material-cache-preparation). Call it after setting the application name and completing the material cache, before loading game shaders or constructing graphics that need them (the default `SceneInit` uses [graphics cache planning](#planning-graphics-caches)).

- **Program list format:** `additionalPrograms` is an array of path arrays, each declaring one program with `.vert`, `.geom` and `.frag` `/Game/Assets/...` paths (keeping source suffixes for encrypted assets).
- **Automatic vs explicit entries:** Default preparation covers engine programs, material passes, standalone assets and matching-name vertex/fragment pairs. Register non-matching pairs, complete geometry programs, or any stage that must also load independently of its matching-name pair. Explicit entries exclude their paths from automatic pairing and single-stage inference, so list every full or standalone program you intend to load.
- **Deduplication and conflicts:** Duplicate entries are deduplicated. While a request is pending, another call with the same program set (regardless of order or duplicates) reuses it, whereas a different program list is rejected rather than merged. Unsupported geometry stages fail on the target backend.

```lua
local request = Engine.prepareShaderCache({
    { "/Game/Assets/Shaders/Custom/Shared.vert", "/Game/Assets/Shaders/Custom/Tint.frag" }
})
```

Once Ready and `getResult()` succeeds, matching `ShaderManager.loadFull` calls can load that program; unprepared combinations fail rather than compiling during rendering. See [MRB rendering](<../../../../Native C++ Development/Build and Module Layout.md#mrb-rendering>) for platform formats, encryption and cache ownership.

### Surface outputs

| Output | Meaning and default |
|---|---|
| `BaseColor` | RGB sampled from the current sprite. |
| `Alpha` | Sprite transparency controlling the final visible silhouette. |
| `Emission` | Local LDR RGB emission (default zero); does not illuminate other objects. |
| `LightingWeight` | `1` accepts scene light, `0` draws unlit, intermediate values blend. |
| `LightBlock` | Per-pixel light blocking (default `0`), combined with final `Alpha` for transmission through the visible silhouette. |
| `ReflectionStrength` | Strength of cell-mirror reflection (default `0`). |
| `VertexOffset` | Local 2D displacement (default zero). |

Graph evaluation and node rules:

- **Surface composition:** Unconnected outputs keep their Surface input (by default the sprite with full `LightingWeight` and zero emission, blocking and reflection). Host tint and presentation opacity apply once in the renderer; lighting, emission and reflection compose before standard transparent blending so foreground objects cover emissive surfaces. HDR, Bloom, normal maps and PBR are not supported.
- **Official nodes:** `Constant`, `Parameter` (`properties.id` selects an exposed parameter), `Time`, `LocalUV`, `LocalPosition`, `WorldPosition`, `FrameSize`, `SurfaceInput`, `SampleSprite`, `SampleTexture`, scalar/vector arithmetic, `Dot`, `Abs`, `Floor`, `Fract`, `Sin`, `Clamp`, `Smoothstep`, `Lerp`, `Compose`, `Swizzle` and `HueShift` (degrees).
- **Sprite sampling:** `SampleSprite` takes frame-local UV and returns `Color`, `Alpha` and `RGBA`, returning transparent outside the current frame. Divide pixel offsets by `FrameSize` before adding to `LocalUV`; autotile sampling spans the assembled full cell across half-cell seams.
- **Vertex displacement and layer stacking:** The `VertexOffset` branch may use positions, time, parameters and maths, but cannot sample textures; output clamps to `maxVertexOffset` and culling accounts for that margin. Tile materials run before layer materials, with both offsets computed from original coordinates and summed. Layer materials alter only connected outputs and never affect Actors on that layer (so `WaterWind` displaces tiles while preserving each tile's surface properties).

### Actor parameters

`actor.material` and `actor:getMaterial()` return the effective resource path; assigning `""` or calling `actor:setMaterial("")` selects `Standard`. `Actor.materialParameters` is a JSON-compatible dictionary of instance overrides.

- Switching to another material clears old overrides, so set the material path before setting parameters.
- `getMaterialParameter(id)` returns the effective value; `setMaterialParameter(id, value)` overrides that Actor only; `resetMaterialParameter(id)` restores the resource default. Replace the whole override set by assigning the dictionary directly (mutating a returned copy does not update the Actor).

```lua
actor:setMaterial("Data/Materials/Frozen.json")
actor:setMaterialParameter("opacity", 0.65)
actor:setMaterialParameter("hue", 20)
actor:resetMaterialParameter("hue")
```

Blueprint defaults and map-instance overrides persist material paths and parameter dictionaries, while temporary runtime edits must be recorded explicitly if they should survive saves. Movement speed belongs to Actor gameplay state, not material parameters.

The default project ships `Standard`, `Rainbow`, `Frozen`, `Burning`, `Petrify`, `Ghost`, `Poison`, `Crystallize`, `Hologram` and `WaterWind` node-graph materials; standard and character examples expose `opacity`, `hue`, `lightBlock`, `reflectionStrength` and `lightingWeight`. Shader generation and binding are engine-owned, and Actors do not expose raw shader paths or handles.

## TileLayerData

Builder data record for constructing an `Engine.TileLayer`. The incremental constructor accepts map dimensions, after which `writeBlock` populates zero-based rectangular blocks; mismatched, ragged or out-of-bounds data fails.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `layerName` | `string` | `""` | — |
| `layerTileset` | `Engine.Tileset` | — | — |
| `tiles` | `int[][]` | — | — |
| `autoTiles` | `any[][]` | — | — |
| `autoTilePool` | `Engine.AutoTile[]` | — | — |
| `autoTileKeys` | `string[]` | — | — |
| `layerTilesetKey` | `string` | `""` | — |
| `material` | `string` | `""` | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `writeBlock` | `x: int`, `y: int`, `tileBlock: int[][]`, `autoTileBlock: any[][]` | — | Lua only (`metadata = false`). |

## Tileset

Describes a tileset texture, per-tile passability, four-way directional passability (`dir4`) and sparse per-tile material overrides.

`materials` is a sparse map keyed by zero-based tile index (stored as decimal strings in JSON and saved in tile-number order by the editor). Store only non-empty explicit references; omit or remove a key to use the built-in default material. Tile count derives from atlas dimensions independently of map size and can be queried via `layer:getTilesetTileCount()`.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `name` | `string` | `""` | — |
| `fileName` | `string` | `""` | `/Game/Assets/Tilesets/...` path. |
| `passable` | `bool[]` | — | — |
| `materials` | `Dict[int, string]` | `{}` | Sparse zero-based tile index to material path map. |
| `dir4` | `bool[][]` | — | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `asDict` | — | `any` | — |
| `fromData` | `data: any` | `Engine.Tileset` | Static. |
