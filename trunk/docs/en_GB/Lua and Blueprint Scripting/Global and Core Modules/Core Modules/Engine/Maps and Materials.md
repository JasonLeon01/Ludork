# Engine: Maps and Materials

Material references use complete `Data/Materials/...json` paths. `AutoTile.fileName` and `Tileset.fileName` continue to use their `/Game/Assets/Autotiles/...` and `/Game/Assets/Tilesets/...` texture paths.

## MapFogSettings

`Engine.MapFogSettings.new(table)` creates the typed fog configuration used by maps and the native renderer. Omitted constructor fields keep these defaults:

| Field | Type | Default | Meaning |
|---|---|---|---|
| `enabled` | `bool` | `false` | Enable the fog layer. |
| `colour` | `sf.Color` | white `(255, 255, 255, 255)` | Fog colour and alpha. |
| `density` | `float` | `0.5` | Density in `[0, 1]`; zero has no visible effect. |
| `scale` | `float` | `256` | Positive noise scale in world pixels. |
| `velocity` | `sf.Vector2f` | `(15, 15)` | Motion in world pixels per second. |
| `turbulence` | `float` | `0.5` | Noise warping strength in `[0, 1]`. |
| `seed` | `int` | `0` | Signed 32-bit deterministic seed. |

Numerical fields must be finite. Native Lua construction uses `sf.Color` and `sf.Vector2f` values. Serialized map and world data store a complete `fog` object with all seven fields; `colour` is four integer channels in `[0, 255]`, and `velocity` is a two-number array. Unknown fields and the old texture fog format are not accepted by the map data parser.

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

Direct metadata bases: `{ "Engine", "TileLayerGraphics" }`

Tile geometry is organised into 32×32-cell chunks, and only the chunks intersecting the view are drawn. `deferred = true` enables incremental runtime construction.

Sparse-world streaming reuses Tile graphics chunk readiness through `isCellBuilt(position)` (`metadata = false`).

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `visible` | `bool` | `true` | — |
| `material` | `string` | — | — |

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `getName` | `function` | self: { "Engine", "TileLayer" } = "self" | name: string | Pure |
| `getTiles` | `function` | self: { "Engine", "TileLayer" } = "self" | tiles: int[][] | Pure |
| `getAutoTiles` | `function` | self: { "Engine", "TileLayer" } = "self" | autoTiles: any[][] | Pure |
| `getAutoTilePool` | `function` | self: { "Engine", "TileLayer" } = "self" | pool: { "Engine", "AutoTile[]" } | Pure |
| `getVisible` | `function` | self: { "Engine", "TileLayer" } = "self" | return: bool | Pure |
| `setVisible` | `function` | self: { "Engine", "TileLayer" } = "self"; visible: bool | — | — |
| `hasContent` | `function` | self: { "Engine", "TileLayer" } = "self"; position: sf.Vector2i | return: bool | Pure |
| `isDirectionPassable` | `function` | self: { "Engine", "TileLayer" } = "self"; position: sf.Vector2i; directionIndex: int | return: bool | Pure |
| `getGridSize` | `function` | self: { "Engine", "TileLayer" } = "self" | return: sf.Vector2u | Pure |
| `buildNextChunk` | `function` | — | return: bool | Runtime only |
| `buildChunk` | `function` | chunkX: int; chunkY: int | return: bool | Runtime only |
| `isChunkBuilt` | `function` | chunkX: int; chunkY: int | return: bool | Runtime only |
| `isBuildComplete` | `function` | — | return: bool | Runtime only |

The native-only `TileLayer::updateBlock` updates an existing rectangular block and rebuilds affected built chunks, including autotile neighbours. `setAnimationTime(seconds, interval)` samples animation absolutely, so rendering a thumbnail at time zero does not advance another view. These are C++ renderer APIs, not Lua bindings. Tile geometry, colour changes and actual animation frame changes invalidate the relevant rendering caches.

## Tilemap

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `addLayer` | `function` | self: { "Engine", "Tilemap" } = "self"; layer: { "Engine", "TileLayer" } | — | — |
| `getLayer` | `function` | self: { "Engine", "Tilemap" } = "self"; name: string | layer: { "Engine", "TileLayer" } | Pure |
| `getTilesData` | `function` | self: { "Engine", "Tilemap" } = "self" | tiles: any | Pure |
| `getAllLayers` | `function` | self: { "Engine", "Tilemap" } = "self" | layers: any | Pure |
| `getLayerNameList` | `function` | self: { "Engine", "Tilemap" } = "self" | layerNames: string[] | Pure |
| `getAutoTilesData` | `function` | self: { "Engine", "Tilemap" } = "self" | autoTiles: any | Pure |
| `getSize` | `function` | self: { "Engine", "Tilemap" } = "self" | size: sf.Vector2u | Pure |
| `updateAutoTileAnimation` | `function` | self: { "Engine", "Tilemap" } = "self"; deltaTime: float; frameInterval: float | — | — |

## AutoTile

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `name` | `string` | `""` | — |
| `fileName` | `string` | `""` | — |
| `passable` | `bool` | `true` | — |
| `material` | `string` | `""` | — |

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `asDict` | `function` | self: { "Engine", "AutoTile" } = "self" | return: any | — |
| `fromData` | `function` | data: any | return: { "Engine", "AutoTile" } | — |

## Material resources

The shared standard resource is `Data/Materials/Global/Standard.json`. Effect examples and `Surfaces` presets remain directly under `Data/Materials` and its `Surfaces` directory.

An authored material is a `Data/Materials/<name>.json` graph with `type: "material"` and `version: 1`. Actors, autotiles and layers retain this string reference. The empty reference selects the built-in neutral material, separately from `Global/Standard.json`; an invalid non-empty reference fails. Export maps these references to shaders and runtime metadata under `Assets/Shaders/MaterialGenerated/Materials/<name>/`, or `Assets/Shaders/MaterialGenerated/Builtin/Default/` for the empty reference. Runtime loading uses those generated files, including encrypted data/shaders and `Assets.ldpak`; it does not read the authored graph. Missing, damaged or unsupported outputs fail. Auxiliary textures retain canonical `/Game/Assets/...` paths, and the host supplies the sprite texture, current frame, transform and time.

For native C++ callers, `MaterialResource::load` loads the exported runtime resource. `MaterialResource::fromData` creates a resource with an editing graph definition for Export and preview compilation. Preview integrations can retain a `MaterialResource::Overrides` snapshot to reuse parsed resources, activating it for each request with `ScopedOverrides`. Keep the snapshot alive for the entire scope and use it on the same preview thread; replace it when its documents or source resources change. The scope does not remain active between requests. See [Material export](<../../../../Editor User Guide/Materials.md#export-for-runtime>) for generation, combinations and file layout.

All individual materials are exported, while base/layer combinations come from saved map usage and the editor's [additional combinations](<../../../../Editor User Guide/Materials.md#material-combinations>). Register a combination there before using it only through dynamically constructed maps or runtime layer-material changes. Actor/UI material switches and parameter changes need no additional entry. A missing combination fails at runtime; the game does not generate it from the graph.

| File section | Contents |
|---|---|
| `parameters` | Array of stable `id`, `type`, `default`, optional `displayName`, `group`, `min` and `max`. |
| `graph.nodes` | Nodes with `id`, official `type`, `inputs` and `properties`. An input is a literal or `{ "node": "id", "port": "output" }`. |
| `graph.output` | The ID of the Surface output node. |
| `rendering.maxVertexOffset` | Maximum absolute displacement `[x, y]` in local pixels; defaults to `[0, 0]`. |
| `editor.positions` | Node ID to canvas position; editing data only. |

Parameter types are `bool`, `float`, `vec2`, `vec3`, `vec4`, `color` and `texture`. Vectors are numeric arrays; colors are four normalized channels in `[0, 1]`. The optional `displayName` changes the label shown in the editor; the stable `id` used by parameter nodes and instance overrides does not change. Invalid types, unknown parameter IDs and values outside declared ranges fail. The graph is a typed, acyclic data-flow graph; it has no gameplay execution pins or custom GLSL node.

### Surface outputs

| Output | Meaning and default |
|---|---|
| `BaseColor` | RGB sampled from the current sprite. |
| `Alpha` | Sprite transparency; controls the final visible silhouette. |
| `Emission` | Local RGB emission, default zero; does not illuminate other objects. |
| `LightingWeight` | `1` accepts scene light, `0` draws unlit, intermediate values blend between them. |
| `LightBlock` | Per-pixel light blocking, default `0`; combined with final Alpha to determine transmission through the visible silhouette. |
| `ReflectionStrength` | Strength of the existing cell-mirror reflection, default `0`. |
| `VertexOffset` | Local two-dimensional displacement, default zero. |

Unconnected outputs preserve their Surface input. The ordinary root input is the sprite with no emission, blocking or reflection and full lighting weight. Host tint and presentation opacity are applied once by the renderer. Surface lighting, emission and reflection are composed before normal transparent blending, so foreground objects also cover emissive surfaces. Emission is LDR; HDR, Bloom, normal maps and PBR are not part of this format.

Official nodes include Constant, Parameter, Time, LocalUV, LocalPosition, WorldPosition, FrameSize, SurfaceInput, SampleSprite, SampleTexture, scalar/vector arithmetic, Dot, Abs, Floor, Fract, Sin, Clamp, Smoothstep, Lerp, Compose, Swizzle and HueShift. `Parameter.properties.id` selects an exposed parameter. `HueShift` takes degrees. `SampleSprite` accepts frame-local UV and returns Color, Alpha and RGBA; samples outside the current frame return transparent and cannot reach another frame. For pixel offsets, divide by FrameSize before adding to LocalUV. Autotile sampling addresses the assembled complete cell, including across its half-cell seams.

The VertexOffset dependency branch may use positions, time, parameters and mathematics, but cannot sample textures. Its result is clamped to `maxVertexOffset`, and culling includes that declared displacement. Tile material executes first, followed by the layer material using its Surface result. Their offsets are computed from the original coordinates and added. A layer material changes only its connected outputs and does not affect Actors on that layer. `WaterWind` therefore changes positions while preserving each tile's surface properties.

### Actor parameters

`Actor.material` is the shared resource reference; `Actor.materialParameters` is a JSON-compatible dictionary of instance overrides. Changing material clears old overrides, so apply the path before parameters. `getMaterialParameter(id)` returns the effective value; `setMaterialParameter(id, value)` changes only that Actor; `resetMaterialParameter(id)` restores its resource default. Assign the entire dictionary to replace the override set. Editing a returned dictionary alone does not update the Actor.

```lua
actor:setMaterial("Data/Materials/Frozen.json")
actor:setMaterialParameter("opacity", 0.65)
actor:setMaterialParameter("hue", 20)
actor:resetMaterialParameter("hue")
```

Blueprint defaults and map-instance overrides can store the reference and parameter dictionary. Temporary runtime changes are not automatically saved; use the existing explicit state-recording workflow. Movement speed is Actor gameplay state and is not a material parameter contract.

The project includes Standard, Rainbow, Frozen, Burning, Petrify, Ghost, Poison, Crystallize, Hologram and WaterWind materials. These are ordinary node graphs, not special Shader modes. The standard and character examples expose `opacity`, `hue`, `lightBlock`, `reflectionStrength` and `lightingWeight`. Shader program generation and binding are engine-owned; Actors do not expose a shader path or mutable shader handle.

## TileLayerData

The incremental overload accepts dimensions, and `writeBlock` then fills zero-based rectangular blocks. Mismatched, ragged or out-of-bounds data fails.

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `layerName` | `string` | `""` | — |
| `layerTileset` | `{ "Engine", "Tileset" }` | — | — |
| `tiles` | `int[][]` | — | — |
| `autoTiles` | `any[][]` | — | — |
| `autoTilePool` | `{ "Engine", "AutoTile[]" }` | — | — |
| `autoTileKeys` | `string[]` | — | — |
| `layerTilesetKey` | `string` | `""` | — |
| `material` | `string` | `""` | — |

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `writeBlock` | `function` | x: int; y: int; tileBlock: int[][]; autoTileBlock: any[][] | — | Runtime only |

## Tileset

`materials` is a sparse map indexed by the existing zero-based tile number; JSON keys are decimal strings. Store only non-empty explicit references. An absent entry selects the standard default material; clear a material by removing its key. The editor saves keys in tile-number order. Tile count comes from the atlas dimensions, independently of the map size; `layer:getTilesetTileCount()` returns that valid tile count.

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `name` | `string` | `""` | — |
| `fileName` | `string` | `""` | — |
| `passable` | `bool[]` | — | — |
| `materials` | `Dict[int, string]` | `{}` | — |
| `dir4` | `bool[][]` | — | — |

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `asDict` | `function` | self: { "Engine", "Tileset" } = "self" | return: any | — |
| `fromData` | `function` | data: any | return: { "Engine", "Tileset" } | — |
