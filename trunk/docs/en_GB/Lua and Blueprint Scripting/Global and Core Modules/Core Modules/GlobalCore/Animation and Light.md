# GlobalCore: Animation and Light

This page documents `GlobalCore` frame-animation playback with synchronised sound cues (`Animation`, `AnimationPlayingSound`) and dynamic map lighting (`Light`). Instance method tables omit the implicit `self` receiver exposed as the first Blueprint target pin; static methods take no receiver.

## AnimationPlayingSound

Tracks one sound instance currently playing for an `Animation` and the frame at which it should stop.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `sound` | `sf.Sound` | — | — |
| `endFrame` | `int` | `-1` | — |

## Animation

**Inherits:** `Engine.AnimSprite`

Extends `Engine.AnimSprite` with frame-synchronised sound cue playback.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `soundEntries` | `Engine.AnimationSoundEntry[]` | — | — |
| `soundIndex` | `int` | `0` | — |
| `playingSounds` | `GlobalCore.AnimationPlayingSound[]` | — | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `setData` | `animationData: Engine.AnimationData` | — | — |
| `update` | `deltaTime: float` | — | — |
| `playSoundsUpToFrame` | `frameIndex: int` | — | — |
| `stopSoundsAtFrame` | `frameIndex: int` | — | — |

## Light

Represents a point light used in ordinary-map and Lua world rendering.

- **Shared budget:** `GlobalCore.Light.MAX_SHADER_LIGHTS` is a static, read-only `int` (`64`) giving the compiled light limit shared by ordinary-map rendering and Lua world rendering. Map lights and Actor lights share this budget.
- **Deterministic culling:** Every visible light is a candidate. When more than `MAX_SHADER_LIGHTS` lights are visible at once, the renderer keeps those whose centres lie nearest the view centre, breaking ties by position so selection never depends on Actor iteration order.

For constant definitions and rebuild rules, see [Build and Module Layout](<../../../../Native C++ Development/Build and Module Layout.md#shared-format-and-rendering-constants>).

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `MAX_SHADER_LIGHTS` | `int` | `64` | Static, read-only. |
| `position` | `sf.Vector2f` | `[0, 0]` | — |
| `colour` | `sf.Color` | `[255, 255, 255, 255]` | — |
| `radius` | `float` | `256` | — |
| `intensity` | `float` | `1` | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `asDict` | — | `any` | — |
| `fromDict` | `data: any` | `GlobalCore.Light` | Static. |
