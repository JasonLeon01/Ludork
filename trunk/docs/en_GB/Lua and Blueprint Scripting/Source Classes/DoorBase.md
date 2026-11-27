# Source.MapActors.DoorBase

`DoorBase` is a conditionally visible map Actor for animated doors, declaring collision, opening interval, gate sound and state properties alongside latent open and close methods.

**Metadata source:** `Scripts/Source/MapActors/DoorBase_meta.lua`

## DoorBase

**Inherits:** `Source.MapActors.ConditionalActor` (see [Source.MapActors.ConditionalActor](<ConditionalActor.md>) for inherited visibility conditions).

`openDoor()` and `closeDoor()` return `Engine.AsyncOperation`. Animation start emits `Enums.MovementLatentOutput.STARTED` (`0`); completion returns `FINISHED` (`1`), matching the Blueprint `Started` / `Finished` branches.

Repeated requests in the same direction share the pending operation; a request in the opposite direction cancels it. Opening an already-open door or closing an already-closed door completes immediately. Normal opening completes before the door destroys itself; other destruction cancels an unfinished operation.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `collisionEnabled` | `bool` | `true` | — |
| `openInterval` | `float` | `0.05` | Frame interval for the door animation. |
| `gateSE` | `string` | `""` | Path: `/Game/Assets/Sounds`; config fallback: `Audio.gateSE`. |
| `opening` | `bool` | `false` | — |
| `closing` | `bool` | `false` | — |

### Functions and events

Instance methods omit the implicit `self` receiver (`Source.MapActors.DoorBase`), which Blueprint nodes expose as the first target pin.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `openDoor` | — | `Engine.AsyncOperation` | Latent. Branches: `Started` (`0`), `Finished` (`1`). Opens the door and destroys it after normal completion. |
| `closeDoor` | — | `Engine.AsyncOperation` | Latent. Branches: `Started` (`0`), `Finished` (`1`). Closes the door. |
| `onTick` | `deltaTime: float` | — | Event. Default execution output. |
