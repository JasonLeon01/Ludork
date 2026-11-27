# Engine: Time, Rendering and Particles

This page documents the frame clock (`Clock`), render state helpers (`RenderStates`), CPU sprite and text particles (`Particle`, `ParticleInfo`, `ParticleBase`, `ParticleSystem`, `TextParticle`) and the GPU particle playback and configuration API (`Emitter`, `EmitterConfiguration`, `EmitterTrack`, `EmitterCurves`, `EmitterBurst`, `EmitterStatistics`, `EmitterComponent`, `EmitterView`).

Instance method tables omit the implicit `self` receiver, which Blueprint nodes expose as the first target pin (defaulting to the graph owner when unset); constructors and static methods take no receiver.

## Clock

Lightweight elapsed-time clock returning seconds as `float`.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `v_reset` | — | `float` | — |
| `v_restart` | — | `float` | — |

## RenderStates

Wrapper for SFML render states.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `Default` | — | `Engine.RenderStates` | Static. Pure. |

## Particle

**Inherits:** `Engine.ParticleBase`

CPU sprite particle owned by an `Engine.ParticleSystem`.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `resourcePath` | `string` | — | — |
| `info` | `Engine.ParticleInfo` | — | — |

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `onTick` | `deltaTime: float` | — | — |
| `onLateTick` | `deltaTime: float` | — | — |
| `onFixedTick` | `fixedDelta: float` | — | — |
| `destroy` | — | — | — |

## ParticleInfo

Per-particle transform and tint record stored in `Particle.info`.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `position` | `sf.Vector2f` | — | — |
| `color` | `sf.Color` | — | — |
| `rotation` | `sf.Angle` | — | — |
| `scale` | `sf.Vector2f` | — | — |

## ParticleBase

Base class shared by `Engine.Particle` and `Engine.TextParticle`.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `onTick` | `deltaTime: float` | — | — |
| `onLateTick` | `deltaTime: float` | — | — |
| `onFixedTick` | `fixedDelta: float` | — | — |
| `getCountTime` | — | `float` | — |
| `getParent` | — | `Engine.ParticleSystem` | — |

## ParticleSystem

Manages and batches CPU `Particle` and `TextParticle` instances.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `addParticle` | `particle: Engine.Particle` | — | — |
| `addText` | `text: Engine.TextParticle` | — | — |
| `clear` | — | — | — |
| `removeParticle` | `particle: Engine.Particle` | — | — |
| `removeText` | `text: Engine.TextParticle` | — | — |
| `removeParticleAt` | `resourcePath: string`, `index: int` | — | — |
| `addUpdateFlag` | `particle: Engine.Particle` | — | — |
| `updateParticlesInfo` | — | — | — |
| `onTick` | `deltaTime: float` | — | — |
| `onLateTick` | `deltaTime: float` | — | — |
| `onFixedTick` | `fixedDelta: float` | — | — |

`clear()` removes all sprite and text particles, detaches owned particles, and releases texture, geometry, UV and pending-update caches; the system accepts new particles afterwards. Calling `clear()` from a particle tick callback is safe (remaining detached particles in that update snapshot are skipped).

For UI particles, use [Canvas.getParticleSystem](<Runtime Values and Functional Controls.md#cpu-particles-in-a-canvas>) (Canvas drives updates and rendering, whereas standalone systems need an explicit host). Common GPU particle assets and `EmitterView` use the separate GPU playback pipeline below.

## TextParticle

**Inherits:** `Engine.ParticleBase`

CPU floating-text particle constructed with `TextParticle.new(parent, moveFunction, countTime, text, config, logicalCoordinates = false)`. `moveFunction(deltaTime, countTime, particle)` receives the `Engine.TextParticle` and may update its text or transform and pass it to `parent:removeText(particle)` even after the original Lua wrapper is garbage-collected, as long as the particle system owns it.

### Functions and events

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getConfig` | — | `Engine.PlainTextConfig` | Pure. |
| `setString` | `text: string` | — | — |
| `getString` | — | `string` | Pure. |
| `setColour` | `colour: sf.Color` | — | — |
| `getColour` | — | `sf.Color` | Pure. |
| `getLocalBounds` | — | `sf.FloatRect` | Pure. |
| `getGlobalBounds` | — | `sf.FloatRect` | Pure. |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## Emitter

`Engine.Emitter` drives GPU particle playback for a [Common Particle](<../../../../Editor User Guide/Common Particles.md>) asset independently of `ParticleSystem`. Actor components and UI views own instances; constructing an `Emitter` alone does not attach it to a scene.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `new` | `resourceKey: string = ""` | `Engine.Emitter` | Static. Optional extensionless key under `Data/Particles`; non-empty keys load and start immediately. |
| `load` | `resourceKey: string` | — | Validates and replaces the resource, clears prior GPU state and restarts. |
| `setConfiguration` | `configuration: Engine.EmitterConfiguration` | — | Lua/native only (`metadata = false`). Validates tracks and curves, bakes curve samples, replaces configuration and restarts playback. |
| `getConfiguration` | — | `Engine.EmitterConfiguration` | Lua/native only (`metadata = false`). Returns an independent snapshot (`EmitterConfiguration` by value in C++, field table in Lua); apply edits via `setConfiguration`. |
| `play` | — | — | Starts from the beginning when stopped or paused; leaves an already playing instance running. |
| `pause` | — | — | Pauses simulation while preserving live particles. |
| `resume` | — | — | Resumes the existing timeline and resets host-motion history. |
| `restart` | — | — | Clears playback state, resets time to `0` and replays configured prewarm. |
| `stop` | `clear: bool = true` | — | Clears and stops when `true`; `false` stops further emission and recycling while letting live particles finish if advancing. |
| `emit` | `trackName: string`, `count: int` | — | Queues a non-negative manual count on a named emission track and enables playback. Invalid names and resident tracks raise an error; counts clamp to track capacity. |
| `setSpeed` | `speed: float` | — | Sets finite playback multiplier in `[0, 100]` (`0` freezes timeline advancement). |
| `getSpeed` | — | `float` | — |
| `setColour` | `colour: sf.Color` | — | Multiplies rendered RGBA by `colour` (`EmitterView` supplies UI presentation colour on draw). |
| `getTime` | — | `float` | Returns simulated seconds. |
| `isPlaying` | — | `bool` | Returns playback flag (a hidden host may keep `true` while scene scheduling is frozen). |
| `getCapacity` | — | `int` | Returns total enabled-track capacity (not live particle count). |
| `setProfiling` | `enabled: bool` | — | Lua/native only. Enables or disables asynchronous GPU timing. |
| `getStatistics` | — | `Engine.EmitterStatistics` | Lua/native only. Returns a statistics snapshot where unavailable optional values are `nil`, not `0`. |

To edit a scalar field on an active emitter's configuration in Lua, read a snapshot, modify it and write it back:

```lua
local configuration = emitter:getConfiguration()
configuration.seed = 42
emitter:setConfiguration(configuration)
```

- **Native host API:** C++ hosts use `tick(float deltaTime)`, `draw(sf::RenderTarget&, sf::RenderStates = {})`, `setHostTransform(const sf::Transform&)`, `resetHostMotion()`, `shutdown() noexcept` and static `collectGarbage() noexcept` (`deltaTime` must be finite and non-negative).
- **Scene scheduling:** `EmitterScheduler` calls `beginFrame()`, `registerEmitter(shared_ptr<Emitter>, transform)` for active hosts (weak, deduplicated; re-entry resets distance history), and `advance(deltaTime)` once per scene frame. `shutdown()` stops known instances and collects retired graphics resources with an active context. Runtime scripts should control emitters through their owning Component or View rather than ticking them twice.

## EmitterConfiguration

Copyable configuration record (`metadata = false`, omitted from Blueprint metadata). C++ passes the struct by value (`setConfiguration(const EmitterConfiguration&)` / `getConfiguration() const`); Lua getters return ordinary field tables, and `setConfiguration` accepts either a native value or a Lua table.

| Property | Type | Default | Description |
|---|---|---|---|
| `name` | `string` | `""` | Authored configuration name. |
| `simulationRate` | `int` | `60` | Integer simulation rate in `1–240`. |
| `seed` | `int` | `1` | Integer PRNG seed in `0–16,777,215`. |
| `tracks` | `Engine.EmitterTrack[]` | `{}` | Ordered tracks with unique names. |

This structure has no `type` property (`"type": "particle"` exists only in resource JSON). Loading a resource resolves curve keys into values, so `getConfiguration()` never retains unresolved curve resource keys.

## EmitterTrack

**Inherits:** `Engine.EmitterTrackParameters`

Extends `Engine.EmitterTrackParameters` (`ludork::runtime::graphics::EmitterTrackParameters` in C++) with `curves: Engine.EmitterCurves`. Timing, forces, texture-sheet settings and transforms keep the names, units and limits documented in [Common Particles](<../../../../Editor User Guide/Common Particles.md#resource-identity-and-schema>), with these typed representation differences:

| Resource JSON | Typed property |
|---|---|
| `mode: "emission"` / `"resident"` | `resident: false` / `true` |
| `space: "local"` / `"world"` | `world: false` / `true` |
| `blend: "alpha"` / `"add"` | `additive: false` / `true` |
| `shape: "point"`, `"line"`, `"rectangle"`, `"disk"`, `"ring"` | `Engine.EmitterShape`: `Point = 0`, `Line = 1`, `Rectangle = 2`, `Disk = 3`, `Ring = 4` |
| `scaleMode: "hierarchy"`, `"local"`, `"shape"` | `Engine.EmitterScaleMode`: `Hierarchy = 0`, `Local = 1`, `Shape = 2` |
| `colourMin`, `colourMax`: four RGBA values in `[0, 255]` | Four floats in `[0, 1]` (each JSON channel divided by `255`); both default to `{ 1, 1, 1, 1 }`. |
| Two-component vectors and min/max ranges | `sf.Vector2f` (range minimum is `x`, maximum is `y`). |
| `textureRect: [x, y, width, height]` | `sf.IntRect` with `position` and `size`. |
| `bursts` array | `Engine.EmitterBurst[]` |
| Inline curves or curve resource keys | Resolved `Engine.EmitterCurves` containing eight `Engine.CurveData` values. |

Use the native enum members (or `Enums.Engine.EmitterShape` and `Enums.Engine.EmitterScaleMode`) in Lua. Typed defaults are `resident = world = additive = false`, `shape = Engine.EmitterShape.Point`, `scaleMode = Engine.EmitterScaleMode.Hierarchy` and `count = 32`.

Lowering `capacity` below `32` requires setting a valid `count` explicitly; the JSON loader separately defaults an omitted `count` to `min(32, capacity)`. Validation runs when applying the complete configuration: numeric ranges must be finite and ordered, colours normalised and enum integers valid.

## EmitterCurves

Holds eight `Engine.CurveData` (`Curve::CurveData` in C++) properties sampled over normalised particle lifetime `[0, 1]`:

- **Multiplier channels (default constant `1`):** `speed`, `sizeX`, `sizeY`, `red`, `green`, `blue`, `alpha`.
- **Additive rotation (default constant `0`):** `rotation` (adds degrees).

`setConfiguration` validates and applies curves and restarts playback.

## EmitterBurst

Fixed value record defining scheduled burst emissions:

| Property | Type | Default | Description |
|---|---|---|---|
| `time` | `float` | `0` | First burst time in seconds within `[0, duration)`. |
| `count` | `int` | `0` | Particles per burst (`0–1,000,000`). |
| `cycles` | `int` | `1` | Number of burst cycles (`1–10,000`). |
| `interval` | `float` | `0` | Seconds between cycles (must be positive when `cycles > 1`). |

## EmitterStatistics

Snapshot returned by `Emitter:getStatistics()` (`ludork::runtime::graphics::EmitterStatistics` by value in C++, ordinary field table in Lua). Unavailable renderer, count or timing fields stay `nil` rather than `""` or `0`, and repeated polling may return the same completed sample.

| Property | Type (C++ / Lua) | Description |
|---|---|---|
| `capacity` | `int` / `integer` | Always present; total enabled-track capacity (not live count). |
| `time` | `double` / `number` | Always present; playback time in seconds. |
| `renderer` | `std::optional<std::string>` / `string \| nil` | Renderer identity when available. |
| `aliveCount` | `std::optional<std::int64_t>` / `integer \| nil` | Delayed GPU live count at `sampledTime`. |
| `sampledTime` | `std::optional<double>` / `number \| nil` | Simulation time of the live-count sample. |
| `gpuSimulationMs`, `gpuDrawMs` | `std::optional<double>` / `number \| nil` | Latest available asynchronous GPU timings in milliseconds. |
| `simulationSample`, `drawSample` | `std::optional<std::int64_t>` / `integer \| nil` | Timing sample IDs; deduplicate each channel and pair matching IDs before summing simulation and draw time. |

## EmitterComponent

**Inherits:** `Engine.Component`

Attached to `Actor.emitterComp` to play a GPU particle effect anchored to an Actor (accepts only an `Actor` owner via the standard Component attachment protocol). For editor configuration, see [Emitter Component](<../../../../Editor User Guide/Actor Components/Emitter.md>).

| Property | Type | Default | Description |
|---|---|---|---|
| `resource` | `string` | `""` | `{ enum = "Enums.Particle", valueType = "string" }`; extensionless key under `Data/Particles` selected from the live project catalogue. Empty disables the effect. |
| `anchor` | `sf.Vector2f` | `[0.5, 0.5]` | Normalised point in the Actor's local bounds. |
| `offset` | `sf.Vector2f` | `[0, 0]` | Additional local offset in logical pixels. |
| `rotation` | `float` | `0` | Local rotation in degrees. |
| `scale` | `sf.Vector2f` | `[1, 1]` | Local scale. |
| `beforeActor` | `bool` | `false` | Draws immediately before the Actor when `true`, or immediately after when `false`. |

Ownership and lifecycle rules:

- `getEmitter()` lazily returns its `Engine.Emitter`; changing or clearing `resource` releases the previous instance. Runtime particle state is not serialised.
- Replacing `actor.emitterComp` or assigning `nil` clears the previous component's playback immediately, even if the Actor is dormant or another reference retains the old component or emitter. Normal map spawning invokes `onAttach(actor)`; components installed after spawning must also be attached through the Component protocol to join scene playback.
- Map rendering keeps normal Actor-list and layer order. Invisible and dormant Actors freeze playback without changing its pause flag; destruction, region eviction, map replacement and scene shutdown clear and release playback. The component holds a weak Actor reference and owns only its playback instance.

## EmitterView

**Inherits:** `Engine.ControlBase`

Childless visual UI control for playing a GPU particle effect inside a `Canvas` or `ListView`, following standard Slot layout, parent ordering and clipping.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `new` | `particle: string = ""`, `size: sf.Vector2f = sf.Vector2f.new(100, 100)`, `anchor: sf.Vector2f = sf.Vector2f.new(0.5, 0.5)`, `autoPlay: bool = true` | `Engine.EmitterView` | Static. `size` sets layout bounds in UI logical pixels; `anchor` locates the emitter within them. |
| `getEmitter` | — | `Engine.Emitter` | Returns the shared `Emitter` playback object. |
| `getParticle` | — | `string` | — |
| `setParticle` | `key: string` | — | Changes the particle resource key and clears the previous effect. |
| `getSize` | — | `sf.Vector2f` | — |
| `setSize` | `size: sf.Vector2f` | — | — |
| `getAnchor` | — | `sf.Vector2f` | — |
| `setAnchor` | `anchor: sf.Vector2f` | — | — |
| `getAutoPlay` | — | `bool` | — |
| `setAutoPlay` | `value: bool` | — | Sets automatic playback policy used on resource load or Canvas change; does not start or pause immediately. |
| `dispose` | — | — | Idempotently releases playback and stops future participation. |

Its `world` particle domain is the containing Canvas's logical space (separate from the map View), and UI composition applies global scaling once. Moving to another Canvas resets the effect; hiding or unmounting freezes and retains particles while preserving manual pause on restore. Generated UI Views expose authored particle nodes via typed references (for example `view.sparkNode:getEmitter():pause()`).
