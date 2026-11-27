# Light Component

Attach a dynamic point light to an Actor — such as a carried lantern or wall torch — so its emitter follows the Actor's transform.

`Engine.Actor` and its derived classes expose `lightComp` as an optional `Engine.LightComponent`. Author map lighting through Actor components and edit per-instance overrides in **Actor Info**.

## Configuration

1. Add or open `lightComp` via the [component workflow](<Overview.md#configuration>).
2. Set `lightColour` and a positive `lightRadius` (defaults: white light, radius `16`).
3. Set `lightOffset` from the centre of the Actor's local bounds to the light source (default `[0, 0]`, transformed with Actor rotation and scale). For wall torches, use a separate torch Actor and position `lightOffset` on the room side of the wall.
4. Edit per-instance values in **Actor Info** and save the Blueprint or map. Inspect lighting and range overlays via [Actor and layer assistance](<../../PreviewHost/Map and World Previews.md#actor-and-layer-assistance>).

**Related guides and APIs:** [LightComponent](<../../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Gameplay Types.md#lightcomponent>) · [Map light editing](<../Tilesets Autotiles and Maps.md#lights-and-actors>)
