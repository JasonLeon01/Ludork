# Engine: Animation Data and Graphics

This page documents `Engine` graphics helpers (`BuildPixelGridVertices`, `compressAnimation`), animation data records (`AnimationKeyFrame` through `AnimationData`) and rendering primitives (`RectBase`, `TileLayerGraphics`). Instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin; free functions take no receiver.

## Module graphics functions

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `BuildPixelGridVertices` | `origin: sf.Vector2f`, `size: sf.Vector2u` | `sf.VertexArray` | Lua only (no Blueprint node). Builds six triangle vertices per pixel using that pixel's centre texture coordinate. Zero-sized input returns an empty triangle array; vertex-count overflow raises an error. |
| `compressAnimation` | `source: Engine.AnimationSourceData \| nil`, `imageFormat: string = "png"` | `Engine.AnimationData` | Compresses the frame images listed in `source.assets`. Takes no asset-root argument. |

## AnimationKeyFrame

Defines one keyframe's timestamp and transform values inside an `AnimationSegment`.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `time` | `float` | `0` | — |
| `position` | `float[]` | — | — |
| `rotation` | `float` | `0` | — |
| `scale` | `float[]` | — | — |

## AnimationSegment

Represents one timed visual or audio segment on an `AnimationTimeline`.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `type` | `string` | `"frame"` | — |
| `asset` | `int` | `-1` | — |
| `startFrame` | `Engine.AnimationKeyFrame` | — | — |
| `endFrame` | `Engine.AnimationKeyFrame` | — | — |
| `flipX` | `bool` | `false` | — |
| `originalDuration` | `float` | — | — |

## AnimationTimeline

Holds an ordered list of `AnimationSegment` entries forming one timeline track.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `timeSegments` | `Engine.AnimationSegment[]` | — | — |

## AnimationTimeTag

Marks a named timestamp on an animation timeline.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `tag` | `string` | — | — |
| `time` | `float` | `0` | — |

## AnimationSourceData

Uncompressed authored animation record consumed by `Engine.compressAnimation`. Non-empty `assets` entries and the resulting `AnimationSoundEntry.asset` values use complete `/Game/Assets/...` paths with exact case.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `type` | `string` | — | — |
| `name` | `string` | — | — |
| `frameRate` | `int` | `0` | — |
| `frameCount` | `int` | `0` | — |
| `visualFrameCount` | `int` | — | — |
| `duration` | `float` | — | — |
| `visualDuration` | `float` | — | — |
| `timeTags` | `Engine.AnimationTimeTag[]` | — | — |
| `timeLines` | `Engine.AnimationTimeline[]` | — | — |
| `assets` | `string[]` | — | — |

## AnimationSoundEntry

Describes a frame-bound sound cue embedded in compressed `AnimationData`.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `asset` | `string` | — | Complete `/Game/Assets/...` path. |
| `startFrame` | `int` | `0` | — |
| `endFrame` | `int` | `0` | — |
| `originalDuration` | `float` | — | — |
| `stopAtEndFrame` | `bool` | — | — |

## AnimationData

Compressed runtime animation record played by `Engine.AnimSprite` and `GlobalCore.Animation`.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `type` | `string` | `"compressedAnimation"` | — |
| `name` | `string` | — | — |
| `frameRate` | `int` | `30` | — |
| `frameCount` | `int` | `0` | — |
| `visualFrameCount` | `int` | — | — |
| `duration` | `float` | — | — |
| `visualDuration` | `float` | — | — |
| `timeTags` | `Engine.AnimationTimeTag[]` | — | — |
| `frameEncoding` | `string` | `"base64+zlib"` | — |
| `frames` | `any[]` | — | — |
| `sounds` | `Engine.AnimationSoundEntry[]` | — | — |

## RectBase

Base rendering helper for nine-slice bordered rectangles drawn with a window skin.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `render` | `dst: sf.RenderTexture`, `edge: sf.RenderTexture`, `edgeSprite: sf.Sprite`, `backSprite: sf.Sprite`, `cachedCorners: sf.Texture[]`, `cachedEdges: sf.Texture[]`, `renderStates: sf.RenderStates` | — | — |

## TileLayerGraphics

Base graphical and cell-query interface for map tile layers, inherited by `Engine.TileLayer`.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setTileColor` | `x: int`, `y: int`, `color: sf.Color` | — | — |
| `resetTileColor` | `x: int`, `y: int` | — | — |
| `floodFillTransparent` | `startX: int`, `startY: int`, `color: sf.Color` | `sf.Vector2i[]` | — |
| `get` | `position: sf.Vector2i` | `int` | Pure. |
| `getAutoTileAt` | `position: sf.Vector2i` | `Engine.AutoTile` | Pure. |
| `isPassable` | `position: sf.Vector2i` | `bool` | Pure. |
| `getMaterial` | `position: sf.Vector2i` | `string?` | Pure. |
| `updateAutoTileAnimation` | `deltaTime: float`, `frameInterval: float` | — | — |
