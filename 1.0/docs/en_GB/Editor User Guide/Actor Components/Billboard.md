# Billboard Component

## Purpose

Show a stack of images and text above an Actor when the player approaches, such as a name or interaction hint.

## Applicability

`Engine.Actor` and its derived classes expose `billboardComp`, an optional `Engine.BillboardComponent`. Configure it through the [component workflow](<Overview.md#configuration>).

## Configuration

1. Add or open `billboardComp` in **Components**.
2. Set `showRange`, the maximum player-to-Actor distance in map logical pixels; its default is `128`.
3. Use **+** and **-** in `items` to add and remove entries. Entries appear from top to bottom. Click **...** beside an entry's summary to edit it.
4. Choose `text` to set content, font size (default `12`) and colour. Choose `image` to select a picture through `path`; this editor label corresponds to the API's `image` field. Switching kinds preserves the hidden values.
5. Save the Blueprint or map.

## Check the result

Run the map and move the player into and out of `showRange` while the Actor is in view. Check the item order, text readability and image size; images use their original pixels. The stack appears above the Actor and rises and fades on entry and exit. Full layout, visibility and animation rules are in the API reference.

## References

- [Actor Components](<Overview.md>)
- [BillboardComponent and BillboardItem API](<../../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Gameplay Types.md#billboardcomponent>)
