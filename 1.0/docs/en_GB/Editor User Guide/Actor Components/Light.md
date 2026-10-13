# Light Component

## Purpose

Attach a light to an Actor so its position follows the Actor's transform, for example a carried lamp or wall torch.

## Applicability

`Engine.Actor` and its derived classes expose `lightComp`, an optional `Engine.LightComponent`. Map lighting is authored through these Actor components, using the ordinary Actor selection and inspector.

## Configuration

1. Add or open `lightComp` through the [component workflow](<Overview.md#configuration>).
2. Set `lightColour` and a positive `lightRadius`. New components start with white light and a radius of `16`.
3. Set `lightOffset` from the centre of the Actor's local bounds to the intended light source; it defaults to `[0,0]` and follows the complete Actor transform. For a wall torch, use a separate Actor and place the light source on the room side of the wall.
4. Edit per-instance values in **Actor Info**, then save the Blueprint or map. Choose **Lit** in the viewport menu to see the result, or enable **Show Light Bounds** in Unlit to inspect the read-only range. See [Lights and actors](<../Tilesets Autotiles and Maps.md#lights-and-actors>).

## References

- [Actor Components](<Overview.md>)
- [LightComponent API](<../../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Gameplay Types.md#lightcomponent>)
- [Map light editing](<../Tilesets Autotiles and Maps.md#lights-and-actors>)
