# Source.MapActors.Teleporter

`Teleporter` declares the stair direction, offset, sound, transition and timing properties, together with stair-transfer and chosen-map nodes. All three Blueprint methods keep a typed `self` input: leave it unset to use the current Teleporter Blueprint owner, or connect another Teleporter instance.

Metadata source: `Scripts/Source/MapActors/Teleporter_meta.lua`

## Teleporter

Direct metadata bases: `{ "Source.MapActors.ConditionalActor", "ConditionalActor" }`

Inherited visibility conditions: [Source.MapActors.ConditionalActor](<ConditionalActor.md>).

`goUpstairs` and `goDownstairs` record the `getTeleportPosition()` of the triggering stair (including `Offset`) and its raw `getMapTag()` after the transfer request is accepted. Arrival records the destination stair. For discovery and display rules, see [Windows, Menus, Input and Controls](<../Default Gameplay/Windows Menus Input and Controls.md>).

`stairDirection` uses the `Teleporter.StairDirection` string enum. The Blueprint property dropdown offers `None` (ordinary teleporter, the default), `Up` and `Down`. Configure every stair Blueprint, including custom variants, with its direction. Lua code uses `Teleporter.StairDirection.None`, `.Up` or `.Down` from the existing Teleporter class.

Going up selects the nearest visible, surviving `Down` stair on the next map; going down selects an `Up` stair. Distance is measured from the source stair's offset position to each candidate's map position using squared Euclidean distance. Equal distances retain Actor order, and arrival applies the selected stair's `Offset`. A missing counterpart follows the existing cancellation path, restores movement and releases the transition freeze; it does not select another direction or an ordinary teleporter.

`FindNearestTeleporter(actors, position, stairDirection?)` accepts the same enum. Omitting the argument searches all teleporters; passing `None` filters ordinary teleporters. `IsAsideOrOverlapping` and general nearest-teleporter transfers continue to search all directions.

`goToMap(mapPath, position, record = true)` exposes a map and tile selector in Blueprint. It transfers directly to `position` without searching for a destination Teleporter. With recording enabled, the source stores this actor's `getTeleportPosition()` and raw tag; the destination stores the chosen tile with an empty tag. Passing `false` adds neither endpoint and preserves existing records. Child-map destinations resolve to their world manifest and world coordinates.

Each endpoint is stored under its own map path. The source and destination maps belong to their respective configured regions, and the floor-teleporter window lists only visited maps in the current region. Stair transfers resolve the source map's region; map loading resolves the destination region. A map outside all configured regions clears the current region and shows no floor entries.

Transfer requests use [GameplayScene.requestFloorStep and requestMapTransfer](<Gameplay.md#gameplayscene>). Rejected requests do not change movement or records.

Meta: `{ PathVars = { [1] = { [1] = "stairSE", [2] = "/Game/Assets/Sounds" }, [2] = { [1] = "transitionName", [2] = "/Game/Assets/Transitions" } }, ConfigVars = { [1] = { [1] = "stairSE", [2] = "Audio", [3] = "stairSE" } } }`

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `Offset` | `sf.Vector2i` | `{ [1] = 0, [2] = 0 }` | — |
| `stairDirection` | `string` enum | `"None"` | Meta { DropBox = { "None", "Up", "Down" } } |
| `stairSE` | `string` | `""` | Meta { PathVars = "/Game/Assets/Sounds", ConfigVars = { [1] = "Audio", [2] = "stairSE" } } |
| `transitionName` | `string` | `""` | Meta { PathVars = "/Game/Assets/Transitions" } |
| `transitionTime` | `float` | `0.5` | — |

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `goUpstairs` | `function` | self: { "Source.MapActors.Teleporter", "Teleporter" } = "self" | — | ExecSplit { [1] = "default", default = nil } |
| `goDownstairs` | `function` | self: { "Source.MapActors.Teleporter", "Teleporter" } = "self" | — | ExecSplit { [1] = "default", default = nil } |
| `goToMap` | `function` | self: { "Source.MapActors.Teleporter", "Teleporter" } = "self"; `mapPath: string`, `position: sf.Vector2i`, `record: bool = true` | — | Default execution output; `Transfer` links `position` to `mapPath` |
