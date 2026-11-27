# Official Random Map

Official Random Map (`Plugins/OfficialRandomMap`) generates procedural terrain inside a selected map region and saves one layer under revision protection. It also serves as the reference implementation for a map-item context-menu command with a plug-in-owned Avalonia window.

## Requirements

- Install [Official release plug-ins](<../Installing and Managing Plug-ins.md#official-release-plug-ins>) and confirm Official Random Map reports **Loaded**.
- Open a project with at least one map, layer and valid tileset.
- Save any unsaved edits in the target map, or accept that a successful plug-in write includes them in its direct save of that map.

## Registration contract

`OfficialRandomMapPlugin` registers one `PluginMapContextMenuCommand` shown when right-clicking a map item. Its handler receives `PluginMapContextMenuContext`, casts `UserInterface` to `IAvaloniaPluginUserInterface` and opens `RandomMapWindow` with the host-supplied owner window. The window interacts solely through `IMapEditorHost` — never with editor view models, controls or mutable JSON.

## Workflow

1. Right-click a map item and select **Random Map Designer**.
2. Choose the target map and layer (read from immutable `PluginMapSnapshot` and `PluginTilesetSnapshot` objects).
3. Pick a wall tile. The preview defaults to the full map; select **Select region** and drag between two cells for a rectangle (both endpoints inclusive, clamped to map bounds, zero-based origin), or select **Full map** to reset.
4. Select **Next**, choose a layout mode and target wall density, and optionally switch to **Place markers** to toggle required open cells inside the region. Markers are temporary and never saved; shrinking the region drops markers outside it, switching layers preserves the region and markers, and switching maps resets to the full map.
5. Select **Generate and Save** to overwrite Tiles and clear AutoTiles inside the selected rectangle on that layer. Cells outside the rectangle, other layers and all Actors stay unchanged; local generation does not inspect or connect to terrain outside the rectangle.
6. Inspect the result (reported density covers the selected region). Select **Generate Again** to roll another layout for the same region. Controls lock with a status animation while generating; a revision conflict reloads the map and resets the workflow.

## Actor preview

The preview renders resolved [Actor snapshots](<../Registration and Hook Reference.md#map-item-context-menu-commands>) combining Blueprint/script classes and instance overrides, showing placeholders for missing assets and following the reference transform, opacity and `CellSize` scaling rules.

Actor positions are visual guides only — place a marker on any Actor cell that must stay open. Character sprites show the first frame of their resolved direction, and native material previews evaluate time-dependent graph inputs and vertex displacement without running Actor gameplay.

## Layouts and density

Both layout algorithms keep all empty cells four-way connected, leave markers open and forbid solid 2 × 2 wall blocks. Outermost cells of the region remain open to paths and rooms — generation does not enforce a closed perimeter ring.

- **Lattice:** Scatters single-cell walls and 2–3 cell straight segments with varied positions, orientations and spacing so separate wall components never touch orthogonally. Reserves single-cell gateways (walls on two opposite sides, open cells front and back) for doors or blocking Actors, protecting their supporting cells while filling towards the target density. Gateways block local passages while alternate routes may still bypass them. Lattice never carves rooms or room rings and rejects empty 4 × 4 areas. Minimum region size: **7 × 7 cells**.
- **Rooms:** Combines small rooms (interiors up to 9 cells, such as 2 × 2, 2 × 3, 2 × 4 and 3 × 3; adjacent rooms may share a one-cell wall), one-cell doorways and branching corridors. An 11 × 11 region produces 4–7 rooms and 1–2 loops for alternate routes, scaling up in larger regions. At least half of the rooms have a single entrance whose blockage disconnects the room from the main route; wide room interiors and wider corridors do not count as alternate routes. Minimum region size: **short side ≥ 7 cells and area ≥ 81 cells**.

Smaller rectangles can be selected in the UI but cannot be generated.

| Layout | Target wall density | Default |
|---|---|---|
| Lattice | 20–35% | 30% |
| Rooms | 30–45% | 36% |

Switching layouts updates the slider bounds and restores that layout's default. Target wall count equals the region cell count multiplied by the percentage (rounded to the nearest whole cell), with an allowed tolerance of 2% of region cells rounded up (minimum 1 cell). Density guides layout generation and the saved result reports the achieved wall percentage.

If the target density, markers and layout constraints cannot be met together, generation fails without saving. Identical dimensions, markers, layout mode, target density and seed reproduce the exact same layout. `RandomMapGenerator.GetDensityProfile(mode)` returns the min/max/default density values, and the `Generate` overload omitting density uses the mode's default.

## Saving contract

Generated layers are submitted through `PluginMapLayerWriteRequest`, rejecting conflicting edits and never applying partial writes on failure. See [Map item context-menu commands](<../Registration and Hook Reference.md#map-item-context-menu-commands>) for complete save and Undo semantics.

> **Note:** `IMapEditorHost` cannot create maps, resize maps, inject editor UI or bypass revision checks. Official Random Map edits terrain only (use [Official Mota Map Fill](<Mota Map Fill.md>) to populate Actors) and has no packaging responsibility, so it never registers a Pack hook.
