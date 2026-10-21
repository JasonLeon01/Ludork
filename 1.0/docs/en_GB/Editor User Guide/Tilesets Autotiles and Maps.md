# Tilesets, Autotiles and Maps

## Tilesets and autotiles

Open **Database → Tilesets** (`F7`) to define a tileset image and its tile properties. Autotile data describes animated or connected tiles assembled from an autotile source. Keep images under `Assets/Tilesets` or `Assets/Autotiles`, and store the JSON definitions under the corresponding `Data` directory.

The window separates ordinary definitions and autotiles into the **Tilesets Data** and **AutoTiles** tabs. Select a definition from the list and edit its name. Then use the browse button beside **File Name** to choose its source image from the corresponding asset directory.

The map palette labels tileset tabs with `name`, while the JSON file name is the internal key. Changing the display name leaves references unchanged. Renaming the key updates every affected map's `layerTileset`, including unopened maps, following the shared [save and Undo rules](<Start Page Workspace and Shortcuts.md#editing-and-saving>).

![Tilesets Data tab showing per-tile passability](../../../images/editor/tilesets-data.png)

*In Passable mode, a circle marks a passable tile and a cross marks a blocked tile.*

![AutoTiles tab showing whole-asset passability](../../../images/editor/auto-tile.png)

*Autotile properties apply to the complete source rather than to each displayed source cell.*

Changing a tileset definition after maps have been authored can change what existing tile indices display.

Map JSON requires a `layerOrder` string array, which is the authoritative draw and Actor-processing sequence. `layers` and `actors` remain dictionaries keyed by those names, and the editor keeps the array synchronised. Runtime loading rejects missing, duplicate or unlisted layers. It never infers order from JSON object keys.

An ordinary tileset provides the **Passable**, **Material** and **Dir4** modes. Left-click toggles one tile's passability, selects its [material resource](<Materials.md>) or toggles the direction nearest the clicked edge. Shift+left-drag copies the pressed source tile's property through every crossed cell. The current mode chooses the copied value: Passable copies the boolean, Material copies the resource reference, and Dir4 copies all four directions. One drag is one Undo step. Clearing a tile material removes its sparse entry and restores the default appearance. Autotiles provide **Passable** and **Material** for the whole asset and do not support this batch brush.

![Map editor with layer tabs above the canvas and a tile palette on the right](../../../images/editor/tile-layers.png)

*Select a layer tab above the canvas, then paint from the palette on the right. The eye controls toggle each layer’s visibility.*

## Map list and composite worlds

The Map List contains ordinary maps and worlds. A world row displays the `worldName` from `_world.json`. Expanding the row shows direct children by their editable `mapName`, while `_world.json` stays hidden. **World Map Properties** edits the world display name, whereas **Rename World Map** changes the folder and the runtime paths. A child keeps the ordinary **Map Name** field, so its file remains usable as a standalone map. Selecting a world opens its composition panel, and the context menus create each asset type. Copying a child assigns fresh Actor map tags without placing the copy.

Drag an unplaced child onto the world canvas, and drag a placed child to move it. Delete, Backspace or the context menu of the placement removes it. Each placement edit is one Undo/Redo step. Placements may temporarily overlap while editing, including when resizing a child map, but Save fails until all overlaps are resolved. Bounds and layer-order violations are still rejected during editing.

The selected placement is drawn above the others within each layer and takes priority when clicking an overlap, so it can be dragged away again. Without a selected placement under the pointer, the last placement takes priority. An obscured child can also be moved by dragging it from the child-map list onto the canvas.

![World composition panel with child-map thumbnails and placed maps](../../../images/editor/world-map.png)

*Child-map thumbnails show placement coordinates; the canvas previews how the maps fit together.*

Scroll or pan the canvas. Use Ctrl+wheel on Windows/Linux or Command+wheel/pinch on macOS to zoom. For rendering, child-map thumbnails and display options, see [Map and World Previews](<../PreviewHost/Map and World Previews.md>).

Resizing cannot place a child outside the world. Before deleting a placed child file, remove its placement and save. Non-JSON regular files are ignored, and Actor tags must be unique across the world.

A rename updates the recognised references listed in [Project Settings and Asset Layout](<Project Settings and Asset Layout.md>). The world manifest and its child maps have separate histories, and automatic reference, placement and layer-order corrections create Undo barriers. The file explorer preserves the `Data/Maps` hierarchy, so root maps, world folders and children cannot move between levels or worlds. Create and delete them through the Map List.

Creating an ordinary map or a world child pre-fills Map Properties from the most recently modified ordinary map or world child in the project. This includes dimensions, ambient light, BGM/BGS and their filters, fog and all its parameters, and panorama. The file name and map name receive new defaults. Current-session edits, including unsaved changes and Undo/Redo, take priority; when no map has been edited in the session, the source is chosen by file modification time. Opening or saving a map does not change the session's edit order. With no existing map, the standard defaults apply. These settings are independent copies, so changing the new map's defaults leaves the source map unchanged.

The Ambient Light button shows a colour swatch and its ARGB value; a checkerboard indicates transparency. Click it to choose a colour.

Use the browse button beside BGM, BGS or **Panorama** to select a resource; choose **Clear** to remove it, then confirm the properties dialog. Panorama images conventionally live in `Assets/Panoramas`. World Map Properties selects a global panorama in the same way. For scaling, camera cropping, lighting and child-map behaviour, see [Panorama rendering](<../Lua and Blueprint Scripting/Default Gameplay/Resources Audio Video and Effects.md#shaders-fog-and-transitions>).

## Fog

Map Properties and World Map Properties provide the same **Fog** settings. Enable fog to reveal its controls. The GPU generates the fog pattern in world coordinates; no fog image is required. Panning or zooming does not attach the pattern to the screen. A world can combine its global fog with fog inside each child region, using the same world coordinates across region boundaries.

| Setting | Meaning | Standard default |
|---|---|---|
| Enabled | Store whether the map renders fog. | Off |
| Colour | RGBA tint and alpha. | White, fully opaque |
| Density | Fog coverage from 0 to 1. | 0.5 |
| Scale | Pattern size in map pixels. | 256 |
| Velocity X / Y | Pattern movement in map pixels per second. | 15 / 15 |
| Turbulence | Pattern variation from 0 to 1. | 0.5 |
| Seed | Non-negative integer selecting a repeatable pattern. | 0 |

The noise pattern has distinct sparse and dense areas. Density controls coverage, colour alpha controls opacity, and a smaller Scale produces smaller cloud patches. Black fog darkens the scene while retaining those variations. The supplied `Map_02` uses black `(0, 0, 0, 128)` fog with Density `0.5`, Scale `128`, Velocity `[15, 15]` and Turbulence `0.8`; the standard Scale default remains `256`.

Preview visibility and lighting are described in [Viewport options](<../PreviewHost/Map and World Previews.md#viewport-options>). Fog is saved as the `fog` object shown in [Map and world-map assets](<Project Settings and Asset Layout.md#map-and-world-map-assets>).

## Layers and painting

Tile mode supports layer creation, rename, reorder, copy, paste and deletion. A layer may reference a [material](<Materials.md#create-and-assign>). Selecting a tileset tab changes the active layer's tileset immediately, and its existing tile indices are then interpreted against that tileset. Select a tile or autotile in the palette to paint on the active layer. When a single ordinary tile is selected, clicking it again in the palette clears the selection. With no tile or autotile selected, the brush erases. Zoom and pan do not alter map data.

Each layer tab shows its name on the left and its visibility control on the right. Visibility is authored map data, so hidden layers are not rendered and are read-only. Layers and their Actors are drawn in the map's layer order. **Overview** shows all visible layers; selecting a layer dims the others without changing their stacking order.

The map uses one tile brush. Left-drag paints the selected tile or autotile continuously, Shift+left-drag paints a filled rectangle, and right-click samples the clicked cell. Alt+left-click (Option+left-click on macOS) fills the four-directionally connected region matching the clicked tile and autotile values. A multi-tile selection repeats from the clicked cell across that region; autotile selection and erase use the same fill shortcut. One brush stroke, rectangle or fill is recorded as one map edit for Undo. Hidden layers reject every editing route and show read-only feedback. The first time painting with a single ordinary tile changes the map, releasing the left mouse button shows an animated reminder that outlines the selected tile and explains deselection and erasing. This reminder appears once per editor user; click anywhere to continue.

Saving during a stroke starts a new Undo step for subsequent movement. Undo and Redo end the active stroke, and pressing again resumes painting.

## Lights and actors

For Blueprint light defaults and component fields, see [Light Component](<Actor Components/Light.md>).

Use Actor mode to place Blueprint instances, move them and edit their exposed attributes. Instance overrides leave Blueprint defaults unchanged. Add lighting through `lightComp` and edit it in **Actor Info**.

### Selecting and editing Actors

Project editing supports one or several selected Actors in the current layer of an ordinary map or an independently opened world child. Live Debug keeps single selection.

| Location and gesture | Selection |
|---|---|
| World Outliner: plain click | Replace the selection with that Actor. |
| World Outliner: Ctrl-click on Windows/Linux or Command-click on macOS | Toggle that Actor. |
| World Outliner: Shift-click | Select the range from the selection anchor; Ctrl/Command+Shift adds that range. |
| World Outliner: click another layer's Actor or a layer row | Switch layers and select that Actor, or clear the Actor selection for a layer row. |
| Map: plain click on a selected Actor | Keep the selection, ready to drag the group. |
| Map: plain click on an unselected Actor | Replace the selection with that Actor. |
| Map: Shift-click / Ctrl/Command-click | Add the Actor / toggle that Actor. |

Map modifier clicks take effect on release. Moving at least 4 device-independent screen pixels on either axis starts a drag and cancels the click action. A modifier drag must start on an Actor that was already selected before pressing; otherwise the whole gesture, including release, does nothing. With several Actors selected, a plain click on empty map space clears the selection and consumes that click without placing an instance. Otherwise normal instance placement remains available. Selecting another map or leaving Actor mode clears the Actor selection.

**Actor Info** shows only properties shared by every selected Actor, including common children of structures, components, lists and dictionaries. Matching properties must have compatible types and editing rules. Values include each Actor's inherited defaults and instance overrides; differing values display **multiple choices**. Editing one child, such as a vector's X, changes only that child on each Actor and retains its other values. A dependent property is editable only when its condition holds for every selected Actor.

With multiple selection, Tag is read-only. Blueprint shows a shared reference with its Open and Locate actions, or **multiple choices** when references differ. Container add/remove and type-switching controls are unavailable. Resetting a property or all overrides restores each Actor's own defaults for the common editable values, preserving other values. Batch edits are applied to the complete selection or rejected without a partial change.

### Moving and transforming Actors

Drag a selected Actor to move the group by the same grid delta. The group stops at map edges while preserving relative positions. If the entire group cannot fit in the map, movement is rejected.

Hold Ctrl on Windows/Linux or Command on macOS while left-dragging a selected Actor to adjust every selected Actor's translation continuously in map pixels, without changing grid positions. Alt/Option+left-drag adjusts each Actor's own origin in local coordinates, accounting for its rotation and scale; translation stays unchanged, so the image moves opposite the drag. Alt/Option takes precedence when both modifiers are held. A transform with a zero scale axis cannot be used for origin dragging. These operations write the instances' `defaultTranslation` and `defaultOrigin` overrides.

Ctrl/Command+wheel, or Command+trackpad pinch on macOS, scales every selected Actor. Scaling writes `defaultScale`, preserving each Actor's axis ratio and signs, including zero axes. Each wheel unit multiplies the value by 1.1; pinch changes it continuously. With no eligible editable selection, the gesture zooms the map instead. Pinching without modifiers still zooms the map.

Alt/Option+wheel or Option+trackpad pinch adjusts each selected Actor's `defaultRotation` around its own origin, leaving grid positions unchanged. Positive wheel movement and pinching outward rotate clockwise; negative movement and pinching inward rotate counterclockwise. Each wheel unit adds 15 degrees, and each 0.1 pinch increment adds 18 degrees. Alt/Option takes precedence over Ctrl/Command for rotation. Without an eligible editable selection, these gestures retain their viewport behaviour. These property shortcuts do not apply in Live Debug.

A complete group drag, batch property change, reset, delete or paste is one Undo step. Consecutive wheel or pinch changes to the same property and selection are grouped. Changing selection, map, layer or mode ends the interaction; saving starts a new Undo step for subsequent changes.

### Copying and deleting Actors

Copy or delete the selection through either the World Outliner or map context menu, or use Ctrl/Command+C and Delete/Backspace. Right-clicking a selected Actor keeps the group; right-clicking an unselected Actor selects it alone before opening the menu.

Paste through the map context menu or Ctrl/Command+V at the pointer's grid cell. The primary Actor is the paste anchor: its cell moves to that position while the other copied Actors retain their relative grid offsets. Paste targets the current layer, preserves instance overrides, assigns fresh tags unique within the map or world, and selects the new group. If any target is outside the map, already occupied, or occupied by another Actor in the copied group, the entire paste fails with feedback and leaves the map unchanged.

For invisible Actors, layer dimming and light-range overlays, see [Actor and layer assistance](<../PreviewHost/Map and World Previews.md#actor-and-layer-assistance>).

![Actor mode with the World Outliner and material parameter overrides in Actor Info](../../../images/editor/actor-mode.png)

*Select the placed instance in **World Outliner** and edit it in **Actor Info**. The example overrides the Poison material’s `lightBlock` parameter for this Actor.*

An Actor light uses `LightComponent.lightOffset` relative to the centre of the Actor's local bounds. The complete Actor transform is applied to the offset, so the emitter follows rotation and scale. For a wall-mounted torch, place the offset at the visible flame on the room side of the wall, because the renderer does not move an emitter to a nearby passable cell.

`Surface.LightBlock` in a material graph is an opacity in the inclusive range `[0, 1]` for one `CellSize` of penetration. `0` is fully transmissive, `1` blocks direct light completely, and intermediate values compound by physical path length. For example, a value of `0.5` transmits approximately `0.707`, `0.5`, and `0.25` through half, one and two cells respectively.

An emitting Actor is excluded only from the dynamic occluders of its own light. It still blocks every other light, while tiles and other Actors at the same position remain occluders. Do not combine a wall and its torch emitter into one Actor. Use a wall occluder Actor and a separate torch Actor, because excluding the emitter also excludes the wall.

### Map tags in a world

Creating or pasting an Actor in a world child prefixes its default tag with the child file stem, and the editor enforces uniqueness across the world. Renaming a child or a world does not rewrite existing or custom tags. Copying a child rewrites the copied tags to `<new-child-stem>.<old-tag-body>` and updates their matching `BPClassVarChanged` keys.

## Map targets

The `startMap` file field and the Blueprint Transfer picker use the same map tree. Selecting a world stores `<WorldFolder>/_world.json`, and its position is a world-cell coordinate. Selecting a placed child stores `<WorldFolder>/<ChildFile>.json`, and its position is local to that child and is converted to world coordinates at runtime. Unplaced children remain visible in the tree but cannot be selected as runtime destinations.

## Live Debug

With **Individual Window** Play and [Live Debug](<Run Debug and Package.md#live-debug>) enabled, this panel shows the player's current ordinary map or world child. Allowed tile and Actor operations, and the save boundary, are on that page.

## Limitations

Arbitrary strings, custom protocols and dynamically constructed paths are not rewritten. Moving a tileset likewise does not repair gameplay strings.

## Related pages

- [Blueprints, Common Functions and Actors](<Blueprints Common Functions and Actors.md>)
- [New Game, Title and Scene Flow](<../Lua and Blueprint Scripting/Default Gameplay/New Game Title and Scene Flow.md>)
- [Official Random Map](<../Plug-in Development and Installation/Official Plug-ins/Random Map.md>)
