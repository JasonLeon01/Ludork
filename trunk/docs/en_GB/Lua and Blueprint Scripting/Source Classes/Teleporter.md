# Source.MapActors.Teleporter

`Teleporter` is a conditionally visible map Actor for stairs and direct map transfers. It declares stair direction, arrival offset, sound effect, transition and timing properties alongside stair-transfer and chosen-map Blueprint methods.

**Metadata source:** `Scripts/Source/MapActors/Teleporter_meta.lua`

## Teleporter

**Inherits:** `Source.MapActors.ConditionalActor` (see [Source.MapActors.ConditionalActor](<ConditionalActor.md>) for inherited visibility conditions).

`goUpstairs` and `goDownstairs` record the `getTeleportPosition()` of the triggering stair (including `Offset`) and its raw `getMapTag()` after the transfer request is accepted. Arrival records the destination stair. For discovery and display rules, see [Windows, Menus, Input and Controls](<../Default Gameplay/Windows Menus Input and Controls.md>).

`stairDirection` uses the standalone `Enums.StairDirection` enum module and offers `None` (ordinary teleporter, the default), `Up` and `Down`, storing the matching string value. Configure every stair Blueprint, including custom variants, with its direction. Lua code loads `local StairDirection = require("Enums.StairDirection")` and uses `StairDirection.None`, `StairDirection.Up` or `StairDirection.Down`.

Going up selects the nearest visible, surviving `Down` stair on the next map; going down selects an `Up` stair. Distance is measured from the source stair's offset position to each candidate's map position using squared Euclidean distance. Equal distances keep Actor order, and arrival applies the selected stair's `Offset`. A missing counterpart follows the standard cancellation path, restores movement and releases the transition freeze; it does not select another direction or an ordinary teleporter.

`FindNearestTeleporter(actors, position, stairDirection?)` accepts the same enum. Omitting the argument searches all teleporters; passing `None` filters ordinary teleporters. `IsAsideOrOverlapping` and general nearest-teleporter transfers search all directions.

`goToMap(mapPath, position, record = true)` exposes a map and tile selector in Blueprint. It transfers directly to `position` without searching for a destination Teleporter. With recording enabled, the source stores this Actor's `getTeleportPosition()` and raw tag; the destination stores the chosen tile with an empty tag. Passing `false` adds neither endpoint and preserves existing records. Child-map destinations resolve to their world manifest and world coordinates.

Each endpoint is stored under its own map path. The source and destination maps belong to their respective configured regions, and the floor-teleporter window lists only visited maps in the current region. Stair transfers resolve the source map's region; map loading resolves the destination region. A map outside all configured regions clears the current region and shows no floor entries.

Transfer requests use [GameplayScene.requestFloorStep and requestMapTransfer](<Gameplay.md#gameplayscene>). Rejected requests do not change movement or records.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `Offset` | `sf.Vector2i` | `sf.Vector2i(0, 0)` | Arrival tile offset applied on transfer. |
| `stairDirection` | `Enums.StairDirection` | `"None"` | String enum selector (`None`, `Up`, `Down`). |
| `stairSE` | `string` | `""` | Path: `/Game/Assets/Sounds`; config fallback: `Audio.stairSE`. |
| `transitionName` | `string` | `""` | Path: `/Game/Assets/Transitions`. |
| `transitionTime` | `float` | `0.5` | Transition duration in seconds. |

### Functions and events

Instance methods omit the implicit `self` receiver (`Source.MapActors.Teleporter`), which Blueprint nodes expose as the first target pin. Leave the target pin unset to use the current Teleporter Blueprint owner, or connect another Teleporter instance.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `goUpstairs` | — | — | Transfers to the next floor up and records both endpoints on acceptance. Default execution output. |
| `goDownstairs` | — | — | Transfers to the next floor down and records both endpoints on acceptance. Default execution output. |
| `goToMap` | `mapPath: string`, `position: sf.Vector2i`, `record: bool = true` | — | Transfers directly to `position` on `mapPath`. `Transfer` metadata links `position` to `mapPath`. Default execution output. |
