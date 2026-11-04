# Engine: Animation Data and Graphics

## Module graphics functions

| Name | Parameters | Returns | Purpose |
|---|---|---|---|
| `BuildPixelGridVertices` | origin: sf.Vector2f; size: sf.Vector2u | sf.VertexArray | Lua-only helper that builds six triangle vertices per pixel and does not create a Blueprint node. All six vertices of a pixel use that pixel's centre texture coordinate. Zero-sized input returns an empty triangle array, and vertex-count overflow reports an error. |
| `compressAnimation` | source: Engine.AnimationSourceData or nil; imageFormat: string = "png" | Engine.AnimationData | Compresses the frame images named by `source.assets`. The function takes no asset-root argument. |

## AnimationKeyFrame

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `time` | `float` | `0` | — |
| `position` | `float[]` | — | — |
| `rotation` | `float` | `0` | — |
| `scale` | `float[]` | — | — |

## AnimationSegment

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `type` | `string` | `"frame"` | — |
| `asset` | `int` | `-1` | — |
| `startFrame` | `{ "Engine", "AnimationKeyFrame" }` | — | — |
| `endFrame` | `{ "Engine", "AnimationKeyFrame" }` | — | — |
| `flipX` | `bool` | `false` | — |
| `originalDuration` | `float` | — | — |

## AnimationTimeline

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `timeSegments` | `{ "Engine", "AnimationSegment[]" }` | — | — |

## AnimationTimeTag

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `tag` | `string` | — | — |
| `time` | `float` | `0` | — |

## AnimationSourceData

Non-empty `assets` entries and the resulting `AnimationSoundEntry.asset` values retain complete `/Game/Assets/...` paths with exact casing.

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `type` | `string` | — | — |
| `name` | `string` | — | — |
| `frameRate` | `int` | `0` | — |
| `frameCount` | `int` | `0` | — |
| `visualFrameCount` | `int` | — | — |
| `duration` | `float` | — | — |
| `visualDuration` | `float` | — | — |
| `timeTags` | `{ "Engine", "AnimationTimeTag[]" }` | — | — |
| `timeLines` | `{ "Engine", "AnimationTimeline[]" }` | — | — |
| `assets` | `string[]` | — | — |

## AnimationSoundEntry

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `asset` | `string` | — | — |
| `startFrame` | `int` | `0` | — |
| `endFrame` | `int` | `0` | — |
| `originalDuration` | `float` | — | — |
| `stopAtEndFrame` | `bool` | — | — |

## AnimationData

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `type` | `string` | `"compressedAnimation"` | — |
| `name` | `string` | — | — |
| `frameRate` | `int` | `30` | — |
| `frameCount` | `int` | `0` | — |
| `visualFrameCount` | `int` | — | — |
| `duration` | `float` | — | — |
| `visualDuration` | `float` | — | — |
| `timeTags` | `{ "Engine", "AnimationTimeTag[]" }` | — | — |
| `frameEncoding` | `string` | `"base64+zlib"` | — |
| `frames` | `any[]` | — | — |
| `sounds` | `{ "Engine", "AnimationSoundEntry[]" }` | — | — |

## RectBase

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `render` | `function` | self: { "Engine", "RectBase" } = "self"; dst: sf.RenderTexture; edge: sf.RenderTexture; edgeSprite: sf.Sprite; backSprite: sf.Sprite; cachedCorners: sf.Texture[]; cachedEdges: sf.Texture[]; renderStates: sf.RenderStates | — | — |

## TileLayerGraphics

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `setTileColor` | `function` | self: { "Engine", "TileLayerGraphics" } = "self"; x: int; y: int; color: sf.Color | — | — |
| `resetTileColor` | `function` | self: { "Engine", "TileLayerGraphics" } = "self"; x: int; y: int | — | — |
| `floodFillTransparent` | `function` | self: { "Engine", "TileLayerGraphics" } = "self"; startX: int; startY: int; color: sf.Color | return: sf.Vector2i[] | — |
| `get` | `function` | self: { "Engine", "TileLayerGraphics" } = "self"; position: sf.Vector2i | return: int | Pure |
| `getAutoTileAt` | `function` | self: { "Engine", "TileLayerGraphics" } = "self"; position: sf.Vector2i | return: { "Engine", "AutoTile" } | Pure |
| `isPassable` | `function` | self: { "Engine", "TileLayerGraphics" } = "self"; position: sf.Vector2i | return: bool | Pure |
| `getMaterial` | `function` | self: { "Engine", "TileLayerGraphics" } = "self"; position: sf.Vector2i | return: string? | Pure |
| `updateAutoTileAnimation` | `function` | self: { "Engine", "TileLayerGraphics" } = "self"; deltaTime: float; frameInterval: float | — | — |
