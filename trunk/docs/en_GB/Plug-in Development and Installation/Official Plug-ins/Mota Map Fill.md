# Official Mota Map Fill

Official Mota Map Fill (`Plugins/OfficialMotaMapFill`) populates an existing map region with doors, enemies and rewards by analysing current terrain connectivity and ranking enemy battle damage against a saved player state. Install it via [Official release plug-ins](<../Installing and Managing Plug-ins.md#official-release-plug-ins>).

The project must keep the Default Gameplay `Player`, `Enemy`, `DoorBase`, `Teleporter` and `MotaBattleAbility` contracts. The plug-in launches the project's game executable to restore the player, prepares the project's user material cache automatically (reporting any errors in the status area) and computes battle damage. Save Blueprint, script and other non-map edits before opening a reference save — C++ Source projects also require an up-to-date **Construct**. The plug-in does not save those documents.

## Choose the region and reference player

1. Right-click a map in the map list and select **Mota Actor Fill**.
2. Choose a visible layer, then drag across the map preview to select a rectangle or choose **Full map**. Coordinates are zero-based and include both dragged endpoint cells.
3. Choose a `.json` or `.ldc` game save. The file picker opens in the previously chosen directory or, initially, in the project's user save directory `~/<APP_NAME>/Save` (falling back to the nearest existing parent without creating directories; errors resolving `APP_NAME` from `Scripts/Entry.lua` appear in the status area).
4. Pick a player from the **Reference player** drop-down (listed in `playerKeys` order, defaulting to the first player). Displayed HP, ATK and DEF include restored equipment, item abilities and states. Switching players recalculates enemy damage; **Reload** re-reads the save and project definitions.

The reference save supplies player state only — terrain always comes from the map currently open in the editor, and map edits stored inside the save are ignored.

## Choose Actor Blueprints

Select **Next**, then **Choose Blueprints…** to pick one or more `.json` Blueprint files under `Data/Blueprints`. The list starts empty; subsequent picks append new entries without duplicates, and cancelling leaves the list unchanged. If any file in a batch is invalid or uses an unsupported Actor type, the entire batch is rejected and the previous selection stays intact.

The **Doors**, **Enemies** and **Items** tabs display only your selected Blueprints, with a preview shown below when selecting or hovering over an entry. Right-click an entry and select **Remove**, or highlight it and press **Delete** or **Backspace**, to exclude it. Damage evaluation covers both the selected Blueprints and existing Actor references on the map:

- **Doors:** Display their inherited key ID and required count. Yellow doors (`KEY_Y`) and blue doors (`KEY_B`) are supported; other door types are not placed.
- **Enemies:** Display single-battle damage calculated by the project's `MotaBattleAbility` (including combat specials). Enemies that cannot be defeated or cost more than half of the reference player's current HP are marked for the largest-room guardian role. An enemy that deals zero damage may legitimately display `0`, while an undefeatable enemy stays marked as undefeatable even when its damage field is `0`.
- **Items:** Include Item and Equip Blueprints, consumables (such as gems and bottles) and other Actor Blueprints that are neither players, doors, enemies nor teleporters. Select only pickups intended as rewards; teleporters and their subclasses are excluded.

## Placement rules

### Terrain and room analysis

The plug-in reads terrain passability across visible layers, including AutoTiles and directional movement flags. It analyses the full map so selection edges do not act as artificial walls, while restricting placement strictly to the chosen layer and region. Existing Actors on other layers and preserved teleporters reserve their cells.

Wide room interiors consist of cells belonging to at least one fully traversable 2 × 2 block, grouped via four-way traversable connections. Single-cell straight passages, bends and junctions do not count towards wide room area.

### Room guards and doors

- **One enemy per room:** Given suitable enemies and free cells, each room receives one enemy across its interior and direct entrances (preferring an interior cell near an entrance). An enemy already guarding a shared entrance covers both adjacent rooms without adding a second enemy, while independent main corridors outside those entrances may receive their own guards. Rooms with no usable doorway can still receive an ordinary enemy on a free interior cell.
- **Independent doors:** Doors are placed at usable entrances independently and do not count towards a room's one-enemy limit.
- **Room size and enemy tiers:** Large rooms (at least 9 interior cells) favour blue doors and stronger enemies; smaller rooms favour yellow doors and weaker enemies. Among defeatable enemies costing at most half the player's HP, the lower half sorted by damage (rounded up) supplies weaker guards and the upper half supplies stronger guards. When only one eligible enemy is selected, it serves both roles.

### High-risk guardian

At most one high-risk enemy is placed per generation, guarding one of the largest large rooms wholly inside the selection that has a usable entrance (ties break randomly). High-risk candidates include enemies costing more than half the player's HP, undefeatable enemies and enemies whose counterattack would be lethal. High-risk enemies are never placed into ordinary rooms, pockets or unguarded corridors.

### Terminal pockets

- **Definition:** A terminal pocket contains 1–8 cells separated from the main route by a single narrow entrance (L-shaped and T-shaped interiors are supported), contains no wide room or teleporter, and connects externally to a wide room, teleporter or junction. Pockets do not contribute to large-room area.
- **Guards, doors and rewards:** Pockets follow the same one-enemy rule across interior and entrance, reusing an existing entrance guard when present. With enough free interior space, a pocket favours a weaker enemy inside plus a reward, and may place a yellow door at its entrance. When only one interior cell is available for a reward, its guard may stand at the entrance. Ordinary corridors receive no items.

### Rewards and fallbacks

- **Minimum and target count:** Rooms and pockets prioritise at least one reward whenever item candidates and a legal cell exist (a retained item satisfies this minimum; otherwise one reward cell is reserved before placing guards, with a pocket's first reward favouring deeper interior cells). New rewards target 40% of the remaining free interior cells, rounded up.
- **Spatial pattern:** Placement prefers a randomly chosen checkerboard parity (such as `1010` / `0101`) and rejects any horizontal or vertical run of three consecutive item cells (counting retained and already-reserved rewards), which can reduce the final count.
- **Missing candidates and reporting:** Missing candidate categories skip independently (for example, omitting blue doors still allows strong guards). Generation reports rooms or pockets that could not receive an enemy, guard or reward without violating occupied cells, selection bounds or high-risk rules. Results list placed counts, combined room/pocket count and the random seed; **Generate Again** rolls a new layout with the same selections.

## Replace and save

> **Warning:** **Generate and Save** removes every existing Actor inside the selected layer and region except `Source.MapActors.Teleporter` instances and subclasses — including events, decorations, doors, enemies and items — along with their instance overrides. Preserved teleporters keep their tags and overrides, and Tiles, AutoTiles, other layers and Actors outside the rectangle remain untouched. If no placements can be generated, the map is neither cleared nor saved.

Saving performs one revision-checked direct map save and pushes one Undo step (including any existing unsaved edits on that map). A conflicting edit, unresolved Actor class or failed save leaves the map untouched; a revision conflict reloads the map and requires reloading the reference save. See [Map item context-menu commands](<../Registration and Hook Reference.md#map-item-context-menu-commands>) for full write and Undo rules.

> **Note:** Damage values are single-battle estimates at the chosen save state. Generation does not simulate collecting rewards, spending keys, multi-battle attrition or spatial movement damage (such as Domain and Flank), and does not guarantee map solvability.
