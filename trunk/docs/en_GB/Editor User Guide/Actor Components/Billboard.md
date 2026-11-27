# Billboard Component

Display a vertical stack of images and text — such as a name plate or interaction prompt — above an Actor when the player moves within range.

`Engine.Actor` and its derived classes expose `billboardComp` as an optional `Engine.BillboardComponent`. Manage it using the shared [component workflow](<Overview.md#configuration>).

## Configuration

1. Add or open `billboardComp` under **Components**.
2. Set `showRange` to the maximum player-to-Actor distance in map logical pixels (default `128`).
3. Use **+** and **-** in `items` to add or remove top-to-bottom entries, and click **...** beside an entry summary to edit it.
4. Choose `text` to configure text content, font size (default `12`) and colour, or `image` to select a picture via `path` (corresponding to the runtime API's `image` field). Switching item kinds preserves hidden fields.
5. Save the Blueprint or map.

**API reference:** [BillboardComponent and BillboardItem](<../../Lua and Blueprint Scripting/Global and Core Modules/Core Modules/Engine/Gameplay Types.md#billboardcomponent>)
