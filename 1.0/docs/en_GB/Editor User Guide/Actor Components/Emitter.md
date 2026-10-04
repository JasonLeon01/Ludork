# Emitter Component

## Purpose

Attach a Common Particle effect to an Actor, such as a flame, aura or sparks. Author the particle's tracks, textures and lifetime curves in the [Common Particle editor](<../Common Particles.md>).

## Applicability

`Engine.Actor` and its derived classes expose `emitterComp`, an optional `Engine.EmitterComponent`. For effects inside a UI, use `Engine.EmitterView` instead.

## Configuration

1. Create and preview a Common Particle resource under `Data/Particles`.
2. Add or open `emitterComp` through the [component workflow](<Overview.md#configuration>).
3. Select `resource`, using its extensionless key such as `Combat/Hit`. An empty resource disables the effect.
4. Set `anchor` to a normalized point in the Actor's local bounds; it defaults to `[0.5,0.5]`. Adjust `offset`, `rotation` and `scale` relative to the Actor to position the effect.
5. Leave `beforeActor` false to draw immediately after the owner, or enable it to draw immediately before. Ordinary maps and composite worlds preserve their Actor-list and layer order; this setting does not introduce Y sorting.
6. Save the Blueprint or map.

## References

- [Actor Components](<Overview.md>)
- [Common Particles](<../Common Particles.md>)
- [EmitterComponent API](<../../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Time Rendering and Particles.md#emittercomponent>)
- [EmitterView for UI](<../UI Controls in the Palette.md#engineemitterview>)
