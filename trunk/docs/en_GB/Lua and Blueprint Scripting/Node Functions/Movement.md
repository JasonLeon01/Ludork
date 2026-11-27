# GlobalFunctions.Movement

`GlobalFunctions.Movement` provides Blueprint nodes that toggle Actor movement, assign explicit movement routes and start automatic pathfinding.

**Metadata source:** `Scripts/GlobalFunctions/Movement_meta.lua`

Route and pathfinding functions return `Engine.AsyncOperation`. They emit `Enums.MovementLatentOutput.STARTED` (`0`) and complete with `FINISHED` (`1`), matching the Blueprint `Started` / `Finished` branches. Route replacement and cancellation follow [Actor.setRoute](<../Global and Core Modules/Core Modules/Engine/Gameplay Types.md#routes-and-operation-lifetime>).

## Movement

### Functions and events

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `SetMoveEnabledByTag` | `tag: string`, `enabled: bool = true` | — | Enables or disables movement on the Actor matching `tag`. |
| `SetMoveRoute` | `actor: Engine.Actor`, `route: sf.Vector2i[] = {}` | `Engine.AsyncOperation` | Latent. Branches: `Started` (`0`), `Finished` (`1`). Move-route editor on `route` (`MoveRouteVars`). |
| `SetAutoPathToDestination` | `actor: Engine.Actor`, `destination: sf.Vector2i = sf.Vector2i(0, 0)` | `Engine.AsyncOperation` | Latent. Branches: `Started` (`0`), `Finished` (`1`). Pathfinds `actor` to `destination`. |
| `SetAutoPathToDestinationByTag` | `tag: string`, `destination: sf.Vector2i = sf.Vector2i(0, 0)` | `Engine.AsyncOperation` | Latent. Branches: `Started` (`0`), `Finished` (`1`). Pathfinds the Actor matching `tag` to `destination`. |
