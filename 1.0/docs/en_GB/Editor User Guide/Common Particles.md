# Common Particles

Common Particle assets contain ordered GPU emitter tracks. Use them on an Actor through `EmitterComponent`, or in a UI through `Engine.EmitterView`.

## Authoring

Choose **Game → New Common Particle** or create a particle in the project file explorer. **Database → Common Particle Overview** (`F6`) lists the resources under `Data/Particles`. Select a resource to edit its tracks, properties and lifetime curves. Right-click the track list to add a track; right-click a track to select it and enable Duplicate Track and Remove Track for that item. Those two actions are disabled when opening the menu on empty space. Track order also determines drawing order within an emitter.

![Common particle editor showing a resident-loop effect](../../../images/editor/common-particle-editor.png)

*The track list is on the left, the preview and lifetime curve are in the centre, and properties are on the right.*

Textures can be PNG (`.png`) or JPEG (`.jpg`); Texture Rect selects a region of the image. For playback, track isolation and seeking, see [Particle previews](<../PreviewHost/UI Actor Particle and Subtitle Previews.md#particle-previews>).

Resource edits follow the shared [save and Undo rules](<Start Page Workspace and Shortcuts.md#editing-and-saving>).

## Resource identity and schema

Store definitions in `Data/Particles/<key>.json`. References use the exact-case key with `/` separators and no directory prefix or extension, for example `Combat/Hit`. Textures use complete `/Game/Assets/...` logical paths. Definitions have no version field.

The following JSON format uses colour components in 0–255. For the corresponding runtime configuration types, see the [Emitter configuration API](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Time Rendering and Particles.md#emittertrack>).

```json
{
  "type": "particle",
  "simulationRate": 60,
  "seed": 1,
  "tracks": [
    {
      "name": "Glow",
      "mode": "resident",
      "capacity": 64,
      "count": 32,
      "loop": true,
      "shape": "disk",
      "radius": 24,
      "lifetime": [0.8, 1.6],
      "speed": [4, 12],
      "texture": "/Game/Assets/Particles/Glow.png",
      "blend": "add",
      "curves": {
        "alpha": {
          "keys": [
            { "time": 0, "value": 0 },
            { "time": 0.2, "value": 1 },
            { "time": 1, "value": 0 }
          ]
        }
      }
    }
  ]
}
```

This example requires the named texture in the project. An empty texture is transparent. `simulationRate` is an integer from 1 to 240, default 60; `seed` is an integer from 0 to 16,777,215, default 1. `tracks` is an ordered array with unique track names. A fixed seed supports repeatable authoring on the same runtime and GPU; floating-point GPU simulation is not a cross-device determinism guarantee.

| Track fields | Default and meaning |
|---|---|
| `name`, `enabled` | `"Track"`, `true`; names identify tracks for manual emission. |
| `mode` | `"emission"` or `"resident"`; see the lifecycle rules below. |
| `duration`, `delay`, `loop`, `prewarm` | `2`, `0`, `true`, `false`; timing uses seconds. `duration` is positive and `delay` is non-negative. |
| `capacity`, `count` | `1024`, `min(32, capacity)`; capacity is 1–1,000,000, and resident count is 0–capacity. |
| `rate`, `distanceRate`, `bursts` | `30`, `0`, `[]`; particles per second, particles per logical pixel of host motion, and scheduled bursts. |
| `shape`, `extent`, `radius`, `innerRadius` | `"point"`, `[32,32]`, `16`, `8`; shapes are `point`, `line`, `rectangle`, `disk`, `ring`. A line uses `extent.x`; rectangles use both extents. Radii are non-negative, and a ring's inner radius cannot exceed its outer radius. |
| `direction`, `spread` | `-90`, `30` degrees; initial direction is sampled within `direction ± spread`. Positive Y points down. |
| `lifetime`, `speed` | `[1,2]` seconds, `[20,40]` logical pixels/second; arrays contain minimum and maximum values. Lifetime is positive. |
| `sizeMin`, `sizeMax` | `[8,8]`, `[16,16]` logical pixels; non-negative minimum/maximum size for each axis. |
| `rotation`, `angularVelocity` | `[0,360]` degrees, `[0,0]` degrees/second. |
| `colourMin`, `colourMax` | Both `[255,255,255,255]`; minimum/maximum RGBA components in 0–255. |
| `gravity`, `radialAcceleration`, `tangentialAcceleration`, `damping` | `[0,0]`, `0`, `0`, `0`; accelerations use logical pixels/second², and non-negative damping applies exponential velocity decay. |
| `texture`, `textureRect` | `""`, `[0,0,0,0]`; rectangle is integer `[x,y,width,height]`. Zero width and height select the entire image; otherwise it must be positive and inside the image. |
| `columns`, `rows`, `frameCount`, `frameRate` | `1`, `1`, `1`, `0`; grid dimensions are 1–4096, count is 1–`columns × rows`, and frame rate is non-negative frames/second. |
| `randomStartFrame`, `frameLoop` | `false`, `true`; frames run left-to-right, then top-to-bottom. Zero frame rate holds the initial frame. A non-looping sequence holds its last frame. |
| `space`, `scaleMode`, `blend` | `"local"`, `"hierarchy"`, `"alpha"`; alternatives are described below. Blend also accepts `"add"`. |
| `offset`, `rotationOffset`, `scale` | `[0,0]`, `0` degrees, `[1,1]`; the track's transform relative to its host. |
| `curves` | Optional lifetime channels `speed`, `sizeX`, `sizeY`, `rotation`, `red`, `green`, `blue`, `alpha`. |

Each burst is `{ "time": 0, "count": 16, "cycles": 1, "interval": 0 }`. Its first time is within `[0,duration)`, count is 0–1,000,000, cycles is 1–10,000, and interval is non-negative seconds, strictly positive for repeated bursts. Numeric inputs must be finite and minimum/maximum ranges ordered.

Curves accept an inline scalar [Curve](<Animations and Curves.md>) or an extensionless key under `Data/Curves`. Their key times use normalised particle age `[0,1]`. Speed, size and RGBA channels multiply their base values and default to 1; rotation adds degrees and defaults to 0. Inline curves support `defaultValue`, `preInfinity`, `postInfinity` and `keys` containing `time`, `value`, `interpolation`, `arriveTangent` and `leaveTangent`. The runtime samples each channel into a 256-entry GPU lookup table, so rendered curves are an approximation of the source curve.

## Emission, space and ownership

`emission` combines time emission, host-distance emission, bursts and `emit(trackName, count)`. `loop` repeats its duration; disabling it ends automatic emission after the duration while existing particles finish. Fixed GPU slots form a ring: births reuse the next slots and overwrite the oldest slots when capacity is exhausted. Capacity is a simulation budget, not a guarantee that every requested particle survives its full lifetime.

`resident` creates `count` particles after its delay. With `loop = true`, each particle repeats its own lifetime without further emission. With looping disabled, the initial set expires once. Time/distance emission, bursts and manual `emit` do not apply to resident tracks. Atlas animation is independent of this lifecycle mode.

`prewarm` applies only to looping tracks. Before normal playback, the GPU replays complete emission cycles covering the maximum particle lifetime with the host held at its current position. Work is limited to 120 fixed steps per frame. Public playback time remains zero while warming, and restarting repeats this process.

In `local` space, existing particles follow the current host transform. In `world` space, particles retain their birth position and transform after the host moves. For an Actor the domain is map coordinates; for an `EmitterView` it is its containing Canvas's logical coordinates. UI particles do not use the map View. Canvas scaling is applied once when composing the UI, and moving a view to another Canvas resets its particles because the coordinate domain changes.

`hierarchy` applies the full host and track scale to shape, motion and particle size. `local` scale mode removes host scale while retaining track scale. `shape` applies the combined scale only to the spawn shape, retaining unscaled particle size and motion.

To attach a particle resource to an Actor, follow [Emitter Component](<Actor Components/Emitter.md>). It covers resource selection, placement relative to the Actor and drawing order.

For a UI effect, add [Engine.EmitterView](<UI Controls in the Palette.md#engineemitterview>) to the asset or construct it in code. Its `ControlBase` and parent own layout and clipping; `getEmitter()` returns the playback object.

Particles pause while their Actor is invisible or dormant, or their UI is hidden or unmounted. Showing the owner resumes its state; movement while hidden does not produce distance-based emission. A manually paused emitter stays paused. Destroying the owner clears its particles, which are not included in map persistence.

See [Emitter, EmitterComponent and EmitterView APIs](<../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Time Rendering and Particles.md#emitter>) for playback controls and performance statistics.

## GPU requirements

Common Particles require a compatible GPU; mobile builds require OpenGL ES 3. Missing GPU capabilities produce an error, with no CPU fallback.

## Limitations

Collision, trails, noise and sub-emitters are outside this particle feature.

## Related pages

- [Project Settings and Asset Layout](<Project Settings and Asset Layout.md>)
- [Resources, Audio, Video and Effects](<../Lua and Blueprint Scripting/Default Gameplay/Resources Audio Video and Effects.md>)
