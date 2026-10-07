# GlobalFunctions.Movement

`Movement` declares the nodes that toggle movement, set a movement route and start automatic pathing.

Metadata source: `Scripts/GlobalFunctions/Movement_meta.lua`

Route and pathfinding functions return `Engine.AsyncOperation`. They emit `Enums.MovementLatentOutput.STARTED` (`0`) and complete with `FINISHED` (`1`), matching the Blueprint `Started` / `Finished` branches. Route replacement and cancellation follow [Actor.setRoute](<../Global and Core Modules/Core Modules/Engine/Gameplay Types.md#routes-and-operation-lifetime>).

## Movement

Direct metadata bases: —

### Properties

No editable properties are declared.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `SetMoveEnabledByTag` | `function` | tag: string; enabled: bool = true | — | ExecSplit { [1] = "default", default = nil } |
| `SetMoveRoute` | `function` | actor: { "Engine", "Actor" }; route: sf.Vector2i[] = {  } | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Started", [2] = "Finished", Started = { [1] = 0 }, Finished = { [1] = 1 } }; Meta { MoveRouteVars = { [1] = "route" } } |
| `SetAutoPathToDestination` | `function` | actor: { "Engine", "Actor" }; destination: sf.Vector2i = { [1] = 0, [2] = 0 } | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Started", [2] = "Finished", Started = { [1] = 0 }, Finished = { [1] = 1 } } |
| `SetAutoPathToDestinationByTag` | `function` | tag: string; destination: sf.Vector2i = { [1] = 0, [2] = 0 } | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Started", [2] = "Finished", Started = { [1] = 0 }, Finished = { [1] = 1 } } |
