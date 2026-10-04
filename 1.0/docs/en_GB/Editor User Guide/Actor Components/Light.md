# Light Component

## Purpose

Attach a light to an Actor so its position follows the Actor's transform, for example a carried lamp or wall torch.

## Applicability

`Engine.Actor` and its derived classes expose `lightComp`, an optional `Engine.LightComponent`. Fixed map lights are edited separately in Light mode.

## Configuration

1. Add or open `lightComp` through the [component workflow](<Overview.md#configuration>).
2. Set `lightColour` and a positive `lightRadius`. New components start with white light and a radius of `16`.
3. Set `lightOffset` from the centre of the Actor's local bounds to the intended light source; it defaults to `[0,0]` and follows the complete Actor transform. For a wall torch, use a separate Actor and place the light source on the room side of the wall.
4. Save the Blueprint or map. For per-instance selection, movement and radius shortcuts in Light mode, see [Lights and actors](<../Tilesets Autotiles and Maps.md#lights-and-actors>).

## Check the result

On an ordinary map, use Light mode to inspect the Actor's coloured range, then run the map to check illumination and shadows. The range is an editor aid and does not preview runtime lighting. If no range appears, check the Actor's visibility, layer visibility and positive radius.

## References

- [Actor Components](<Overview.md>)
- [LightComponent API](<../../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Gameplay Types.md#lightcomponent>)
- [Map light editing](<../Tilesets Autotiles and Maps.md#lights-and-actors>)
