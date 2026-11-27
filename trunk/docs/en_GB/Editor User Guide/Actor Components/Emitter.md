# Emitter Component

Attach a GPU Common Particle effect — such as flames, auras or sparks — directly to an Actor. Author particle tracks, textures and lifetime curves in [Common Particles](<../Common Particles.md>).

`Engine.Actor` and its derived classes expose `emitterComp` as an optional `Engine.EmitterComponent`. For effects inside UI layouts, use [Engine.EmitterView](<../UI Controls in the Palette.md#engineemitterview>) instead.

## Configuration

1. Create and preview a Common Particle asset under `Data/Particles`.
2. Add or open `emitterComp` via the [component workflow](<Overview.md#configuration>).
3. Choose `resource` from the live `Enums.Particle` string enum (extensionless key such as `Combat/Hit`; empty disables the effect).
4. Set `anchor` to a normalised point within the Actor's local bounds (default `[0.5, 0.5]`) and tune `offset`, `rotation` and `scale` relative to the Actor.
5. Leave `beforeActor` set to `false` to draw immediately after the owner, or enable `true` to draw immediately before it. Ordinary maps and composite worlds preserve their Actor-list and layer order; this option does not add Y sorting.
6. Save the Blueprint or map.

**API reference:** [EmitterComponent](<../../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Time Rendering and Particles.md#emittercomponent>)
