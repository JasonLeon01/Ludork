# Official Random Map

Use the official Random Map plug-in as the reference for a map-item command that reads immutable editor snapshots, opens its own Avalonia workflow and replaces one layer with revision protection.

## Prerequisites

- Install the editor release. On macOS, also run **Install Official Plugins** from the DMG. Restart Ludork and confirm that Official Random Map is **Loaded**. `Plugins/OfficialRandomMap` is its source directory in a repository checkout.
- Open a project containing at least one map, layer and valid tileset.
- Save any unsaved edits in the target map, or accept that a successful plug-in write includes them in the direct save of that map.

## Registration contract

`OfficialRandomMapPlugin` registers one `PluginMapContextMenuCommand`, displayed when a map item is right-clicked.

The handler receives `PluginMapContextMenuContext`, checks for `IAvaloniaPluginUserInterface`, and opens the plug-in-owned `RandomMapWindow` with the host's real owner. The window uses only `IMapEditorHost`. It does not receive an editor view model, a control or a mutable JSON object.

## Steps

1. Right-click a map item and choose the Random Map command.
2. Select the target map and layer. The plug-in reads `PluginMapSnapshot` and `PluginTilesetSnapshot` values.
3. Select a valid wall tile. The preview starts with the full map selected. Choose **Select region** and drag between two cells to select a rectangle, or choose **Full map** to restore the whole map. Both endpoint cells are included; dragging beyond the map clamps to its edges. The displayed origin uses zero-based map coordinates.
4. Select **Next**, choose the layout and wall density limit, then use **Place markers** to toggle required open cells within the region. Markers are optional and are not saved. Shrinking the region removes markers outside it. Switching layers retains the region and markers; switching maps resets to the full map.
5. Select **Generate and Save** to generate inside the region and save the selected layer. Outside the rectangle, both Tile and AutoTile values remain unchanged. Inside it, generated wall tiles or empty cells replace Tiles and clear AutoTiles. Other layers and all Actors remain unchanged. Local generation does not inspect or connect to paths outside the rectangle.
6. Review the saved result. **Generate Again** retains the region and generates another layout. The reported density is measured within that region. Generation locks the controls until completion; a revision conflict reloads the map and resets the workflow.

## Actor preview

The preview draws static Actor images in their map layers using the editor's visual resolver for Blueprint and script classes, including per-instance overrides. Character direction selects the first frame of that direction. Texture rectangles, translation, scale, origin, rotation, material, parameter overrides and visibility opacity follow the editor's resolved values; missing resources display a placeholder. Actor dimensions use the host's source cell size, independent of the selected tileset.

Actor positions are visual references only. Add a marker explicitly when an Actor's cell must remain open. Character sprites retain their resolved first frame. Material effects use native preview rendering, including their time-dependent graph inputs and vertex displacement; the preview does not execute Actor gameplay.

## Layouts and density

**Lattice** creates separated single-cell walls and straight wall segments of two or three cells. Different components do not touch orthogonally, and the layout does not carve rooms or build room rings. All empty cells remain connected by four-way movement, markers remain open, and the generator rejects solid 2 × 2 wall blocks and completely empty 4 × 4 areas. Dense marker arrangements can make these requirements impossible; generation then reports a failure without saving.

**Rooms** retains its room-and-corridor generation rules. Lattice requires a region of at least 7 × 7 cells; Rooms requires a short side of at least 7 cells and an area of at least 81 cells. Smaller rectangles can be selected but cannot be generated.

The 35–75% setting is a wall density upper limit. The generator approaches it only while preserving the selected style and connectivity. Lattice layouts may be substantially below the limit because their wall components must stay short and separate. The same dimensions, markers, mode, limit and seed reproduce the same result.

## Saving contract

The plug-in submits the generated layer through `PluginMapLayerWriteRequest`. Conflicting edits are rejected, and failed generation or saving does not apply a partial region. The full request and Undo rules are in [Map item context-menu commands](<../Registration and Hook Reference.md#map-item-context-menu-commands>).

## Limitations

`IMapEditorHost` cannot create maps, change dimensions, edit Actors, inject editor UI or bypass revision checks. Random Map has no packaging responsibility, so it must not use a Pack hook for map generation.

## Related pages

- [Registration and Hook Reference](<../Registration and Hook Reference.md>)
- [Avalonia UI, Secrets and Localisation](<../Avalonia UI Secrets and Localisation.md>)
- [Tilesets, Autotiles and Maps](<../../Editor User Guide/Tilesets Autotiles and Maps.md>)
