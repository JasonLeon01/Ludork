# Source.MapActors.DoorBase

`DoorBase` declares the door collision, opening interval, gate sound and door state properties, together with the open and close nodes.

Metadata source: `Scripts/Source/MapActors/DoorBase_meta.lua`

`openDoor()` and `closeDoor()` return `Engine.AsyncOperation`. Animation start emits `Enums.MovementLatentOutput.STARTED` (`0`); completion returns `FINISHED` (`1`), matching the Blueprint `Started` / `Finished` branches.

Repeated requests in the same direction share the pending operation; reversing cancels it. Opening an already-open door or closing an already-closed door completes immediately. Normal opening completes before the door destroys itself; other destruction cancels an unfinished operation.

## DoorBase

Direct metadata bases: `{ "Source.MapActors.ConditionalActor", "ConditionalActor" }`

Inherited visibility conditions: [Source.MapActors.ConditionalActor](<ConditionalActor.md>).

Meta: `{ PathVars = { [1] = { [1] = "gateSE", [2] = "/Game/Assets/Sounds" } }, ConfigVars = { [1] = { [1] = "gateSE", [2] = "Audio", [3] = "gateSE" } } }`

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `collisionEnabled` | `bool` | `true` | — |
| `openInterval` | `float` | `0.05` | — |
| `gateSE` | `string` | `""` | Meta { PathVars = "/Game/Assets/Sounds", ConfigVars = { [1] = "Audio", [2] = "gateSE" } } |
| `opening` | `bool` | `false` | — |
| `closing` | `bool` | `false` | — |

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `openDoor` | `function` | self: { "Source.MapActors.DoorBase", "DoorBase" } = "self" | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Started", [2] = "Finished", Started = { [1] = 0 }, Finished = { [1] = 1 } } |
| `closeDoor` | `function` | self: { "Source.MapActors.DoorBase", "DoorBase" } = "self" | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Started", [2] = "Finished", Started = { [1] = 0 }, Finished = { [1] = 1 } } |
| `onTick` | `event` | deltaTime: float | — | ExecSplit { [1] = "default", default = nil } |
