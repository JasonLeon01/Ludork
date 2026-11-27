# State

Defines persistent status effects, their stackability and their step- or hook-triggered ability graphs.

**Source:** `Data/General/State.json` · **Member events:** `onWalk`, `onHookTriggered`

## Schema

| Field | Type | Default | Description |
|---|---|---|---|
| `name` | `string` | `""` | Display name of this state. |
| `desc` | `string` | `""` | Display description of this state. |
| `icon` | `file` | `""` | Canonical logical icon path (picker root: `/Game/Assets/Icons`). |
| `stackable` | `bool` | `false` | Whether reapplying this state increments the existing stack count. |

## Editing notes

Applying a State creates an Infinite Gameplay Effect and grants its event abilities:

- **`onWalk`:** Listens for `Event.Movement.Step`.
- **`onHookTriggered`:** Triggered explicitly by `Event.State.Trigger.<stateID>` or via `GlobalFunctions.Gameplay.SendEvent` in a gameplay graph. Applying or stacking the State does not emit `onHookTriggered`.
