# Official Mota Map Fill

Official Mota Map Fill places doors, enemies and rewards on an existing map. It uses the current terrain to find narrow passages and rooms, and a saved player's state to rank enemy battle damage. Its source is `Plugins/OfficialMotaMapFill`; install it through the [official plug-in installation](<../Installing and Managing Plug-ins.md#official-release-plug-ins>).

The project must retain the Default Gameplay Player, Enemy, DoorBase, Teleporter and MotaBattleAbility contracts. The plug-in runs the current project's game executable to restore the player and calculate damage. Save Blueprint, script and other non-map data changes before opening a reference save; source projects also need a current **Construct**. The plug-in does not save those other documents for you.

## Choose the region and reference player

Right-click a map in the map list and choose **Mota Actor Fill**. Select a visible layer, then drag on the map preview to select a rectangular region, or choose **Full map**. Coordinates are zero-based and both dragged endpoint cells belong to the selection.

Choose a `.json` or `.ldc` game save. The picker starts in the previously selected file's folder, or otherwise the current project's user save directory, `~/<APP_NAME>/Save`. If that directory does not exist, it opens the nearest existing parent without creating directories. Errors resolving `APP_NAME` from `Scripts/Entry.lua` appear in the status area.

The **Reference player** drop-down lists the save's `players` in `playerKeys` order and initially selects the first player. The displayed HP, ATK and DEF include restored equipment, item abilities and states. Selecting another player recalculates enemy damage. **Reload** rereads the save and project definitions.

The reference save supplies player state only. The terrain comes from the map currently open in the editor; saved map changes are not applied.

## Choose Actor Blueprints

Select **Next**, then **Choose Blueprints…** to open the system file picker. Select one or more `.json` Blueprint files under the current project's `Data/Blueprints` directory. The selection starts empty; choosing more files appends new entries without duplicates, and cancelling the picker leaves it unchanged. If any chosen file is invalid or an unsupported Actor type, the whole new batch is rejected and the existing selection is retained.

The Doors, Enemies and Items tabs below display only the selected Blueprints. Select or hover over an entry to show its preview underneath. Right-click an entry and choose **Remove**, or highlight it and press **Delete** or **Backspace**, to exclude it from generation. Damage evaluation uses the selected Blueprints and the current map's existing Actor references.

Door entries display their inherited key ID and required count. Yellow doors use `KEY_Y`, and blue doors use `KEY_B`; other door types are not placed by this workflow. Enemy entries display damage calculated by the project's own `MotaBattleAbility`, including its combat specials. Enemies that cannot be defeated or would cost more than half the reference player's current HP are labelled for the largest-room guardian position. An enemy that cannot damage the player may correctly show zero damage; an enemy the player cannot defeat remains marked as such even when its damage field is zero.

The Items group includes Item and Equip Blueprints, consumables such as gems and bottles, and other Actor Blueprints that are not players, doors, enemies or teleporters. Choose only the pickups intended as rewards. Teleporters and their subclasses are excluded from generation candidates.

## Placement rules

The plug-in reads terrain passability across visible layers, including AutoTiles and directional movement restrictions. Wide room interiors consist of cells belonging to at least one fully traversable 2 × 2 block, grouped through four-way traversable connections. Single-cell-wide straight passages, bends and junctions do not contribute to that area. It analyses the full map so that the selection boundary does not become an artificial wall, but places Actors only inside the selected layer and region. Existing Actors on other layers and preserved teleporters reserve their cells.

- With suitable selected enemies and available cells, each room receives one enemy across its interior and direct entrances, preferably inside near an entrance. An enemy already at a shared entrance can guard both adjacent rooms without adding a second enemy for either room. Independent main corridors outside those entrances can have their own guards. A room with no usable doorway can still receive an ordinary enemy in a free interior cell. Doors are placed independently at usable entrances and do not count toward the room's one enemy. Rooms with at least nine interior cells favour blue doors and stronger enemies; smaller rooms favour yellow doors and weaker enemies. Among enemies that can be defeated for at most half the player's HP, the lower half by damage, rounded up, supplies weaker guards; the upper half supplies stronger guards. With only one eligible enemy, it can serve both roles.
- At most one high-risk enemy is placed. It guards one of the largest large rooms entirely inside the selection, with a usable entrance; ties are chosen randomly. High-risk candidates include enemies costing more than half the player's HP, enemies that cannot be defeated and enemies whose counterattack would be lethal. They are not used to fill ordinary rooms, pockets or corridors that lack a guard.
- A terminal pocket contains one to eight cells separated from the main route by a single narrow entrance, including L-shaped or T-shaped interiors. The pocket contains no wide room or teleporter, and the outside route must lead to a wide room, teleporter or junction. These pockets do not increase the area used to select a large room. The same one-enemy rule applies to a pocket's interior and entrance. With enough free interior space, it favours a weaker enemy inside and a reward; its entrance can also receive a yellow door. An enemy already guarding its entrance is reused instead of adding another inside. When only one interior cell is available for a reward, its guard can stand at the entrance. Ordinary corridors receive no items.
- Rooms and pockets prioritise at least one reward when item candidates and a legal position are available. An existing retained item satisfies this minimum; otherwise a reward cell is reserved before guards are placed. A pocket's first reward prefers a cell deeper inside. New rewards target 40% of the remaining available interior cells, rounded up. A randomly chosen checkerboard colour is preferred, producing arrangements such as `1010` / `0101`. Placement rejects any horizontal or vertical run of three consecutive item cells, including retained items and other reserved rewards. Constraints can reduce the final count.

Missing candidate types are skipped independently: for example, selecting no blue door does not prevent an otherwise valid strong guard. Generation reports rooms or pockets that could not receive an enemy, guard or reward, rather than overriding occupied cells, the selection bounds or the high-risk restrictions. The result reports the placed counts, the combined room and pocket count, and the random seed; **Generate Again** chooses a new layout using the current selections.

## Replace and save

**Generate and Save** replaces every Actor inside the selected layer and region except `Source.MapActors.Teleporter` instances and subclasses. This includes existing events, decorations, doors, enemies and items. Preserved teleporters retain their tags and instance overrides; removed Actors' overrides are removed with them. Tiles, AutoTiles, other layers and Actors outside the rectangle remain unchanged. If no placements can be generated, the map is not cleared or saved.

The replacement is one revision-checked map save and one Undo step. Existing unsaved edits on that same map are included in the save. A conflicting map edit, unresolved Actor class or failed save leaves the map unchanged; a revision conflict reloads the map and requires reloading the reference save. The [map plug-in write contract](<../Registration and Hook Reference.md#map-item-context-menu-commands>) describes saving and Undo in full.

Damage is a single-battle estimate at the selected saved state. Generation does not simulate collecting rewards, spending keys, successive battles or spatial movement damage such as Domain and Flank, and does not guarantee that the resulting map is solvable.
