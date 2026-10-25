# Official Random Map

Use the official Random Map plug-in as the reference for a map-item command that reads immutable editor snapshots, opens its own Avalonia workflow and replaces one layer with revision protection.

## Prerequisites

- Install [the official plug-ins](<../Installing and Managing Plug-ins.md#official-release-plug-ins>) and confirm Official Random Map is **Loaded**. Its repository source is `Plugins/OfficialRandomMap`.
- Open a project containing at least one map, layer and valid tileset.
- Save any unsaved edits in the target map, or accept that a successful plug-in write includes them in the direct save of that map.

## Registration contract

`OfficialRandomMapPlugin` registers one `PluginMapContextMenuCommand`, displayed when a map item is right-clicked.

The handler receives `PluginMapContextMenuContext`, checks for `IAvaloniaPluginUserInterface`, and opens the plug-in-owned `RandomMapWindow` with the host's real owner. The window uses only `IMapEditorHost`. It does not receive an editor view model, a control or a mutable JSON object.

## Steps

1. Right-click a map item and choose the Random Map command.
2. Select the target map and layer. The plug-in reads `PluginMapSnapshot` and `PluginTilesetSnapshot` values.
3. Select a valid wall tile. The preview starts with the full map selected. Choose **Select region** and drag between two cells to select a rectangle, or choose **Full map** to restore the whole map. Both endpoint cells are included; dragging beyond the map clamps to its edges. The displayed origin uses zero-based map coordinates.
4. Select **Next**, choose the layout and target wall density, then use **Place markers** to toggle required open cells within the region. Markers are optional and are not saved. Shrinking the region removes markers outside it. Switching layers retains the region and markers; switching maps resets to the full map.
5. Select **Generate and Save** to generate inside the region and save the selected layer. Outside the rectangle, both Tile and AutoTile values remain unchanged. Inside it, generated wall tiles or empty cells replace Tiles and clear AutoTiles. Other layers and all Actors remain unchanged. Local generation does not inspect or connect to paths outside the rectangle.
6. Review the saved result. **Generate Again** retains the region and generates another layout. The reported density is measured within that region. Generation locks the controls and shows an animated indicator in the status area until completion or failure; a revision conflict reloads the map and resets the workflow.

## Actor preview

The preview renders resolved [Actor snapshots](<../Registration and Hook Reference.md#map-item-context-menu-commands>) from Blueprint/script classes and instance overrides, with placeholders for missing resources. The reference defines transforms, opacity and cell-size scaling.

Actor positions are visual references: add a marker when an Actor's cell must remain open. Character sprites retain the first frame of their resolved direction. Native material previews include time-dependent graph inputs and vertex displacement without executing Actor gameplay.

## Layouts and density

Both layouts keep all empty cells connected by four-way movement, leave markers open and reject solid 2 × 2 wall blocks. The region's outermost cells remain available for paths and rooms; generation does not reserve a closed outer wall ring.

**Lattice** randomly scatters single-cell walls and straight wall segments of two or three cells. Their positions, directions and spacing vary across the region, while separate components do not touch orthogonally. The generator reserves local single-cell gateways with walls on two opposite sides and open cells in front and behind, providing positions for doors or blocking Actors. These supporting walls and open cells remain protected while the remaining walls are placed to reach the target density. Natural bends and areas of different spacing remain part of the layout. A gateway blocks its local passage, while other routes may still lead around it. Lattice does not carve rooms or build room rings, and rejects completely empty 4 × 4 areas.

**Rooms** combines small rooms, one-cell doorways and branching corridors. Room interiors contain at most nine cells, with shapes such as 2 × 2, 2 × 3, 2 × 4 and 3 × 3; adjacent rooms can share a one-cell wall. An 11 × 11 region contains four to seven rooms and one or two loops offering alternate routes around walls. Larger regions support more rooms and routes. At least half of the rooms have a single entrance: blocking that entrance disconnects the room from the main route. Other connections provide limited detours. Open space within a room and wider corridors do not count as extra routes.

Lattice requires a region of at least 7 × 7 cells; Rooms requires a short side of at least 7 cells and an area of at least 81 cells. Smaller rectangles can be selected but cannot be generated.

| Layout | Target wall density | Default |
| --- | --- | --- |
| Lattice | 20–35% | 30% |
| Rooms | 30–45% | 36% |

The setting targets the actual proportion of wall cells within the region. Switching layouts updates the slider range and restores that layout's default. The target wall count is the region's cell count multiplied by the percentage, rounded to the nearest whole cell. The allowed difference is 2% of the region's cell count, rounded up, with a minimum of one cell. Density participates in layout generation, and the saved result reports its actual wall percentage.

If the target density, markers and layout rules cannot be satisfied together, generation reports a failure without saving. The same dimensions, markers, mode, target density and seed reproduce the same result. `RandomMapGenerator.GetDensityProfile(mode)` supplies the minimum, maximum and default values; the `Generate` overload without a density argument uses the selected mode's default.

## Saving contract

The plug-in submits the generated layer through `PluginMapLayerWriteRequest`. Conflicting edits are rejected, and failed generation or saving does not apply a partial region. The full request and Undo rules are in [Map item context-menu commands](<../Registration and Hook Reference.md#map-item-context-menu-commands>).

## Limitations

`IMapEditorHost` cannot create maps, change dimensions, edit Actors, inject editor UI or bypass revision checks. Random Map has no packaging responsibility, so it must not use a Pack hook for map generation.

## Related pages

- [Avalonia UI, Secrets and Localisation](<../Avalonia UI Secrets and Localisation.md>)
- [Tilesets, Autotiles and Maps](<../../Editor User Guide/Tilesets Autotiles and Maps.md>)
