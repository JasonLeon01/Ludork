# GlobalFunctions.Mota

`GlobalFunctions.Mota` provides Blueprint nodes for opening the monster handbook and floor teleporter, managing the active region and running direct floor or centre-symmetric teleports.

**Metadata source:** `Scripts/GlobalFunctions/Mota_meta.lua`

## Mota

### Functions and events

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `OpenMonsterBook` | — | — | Opens the monster handbook window on the active `SceneMap`. |
| `OpenFloorTeleporter` | — | — | Opens the floor-teleporter window on the active `SceneMap`. |
| `GetCurrentRegion` | — | `region: string` | Pure. Returns the active region identifier. |
| `SetCurrentRegion` | `region: string` | — | Sets the active region identifier on the current `GameInstance`. |
| `CenterSymmetricTeleport` | — | `int` | Execution branches: `Success` (`0`), `Failed` (`1`). Attempts a centre-symmetric teleport on the current map. |
| `GoUpstairsSamePos` | — | `int` | Execution branches: `Success` (`0`), `Failed` (`1`). Teleports one floor up at the same map tile. |
| `GoDownstairsSamePos` | — | `int` | Execution branches: `Success` (`0`), `Failed` (`1`). Teleports one floor down at the same map tile. |
