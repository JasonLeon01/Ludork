# State

Source: `Data/General/State.json`

| Property | Value |
|---|---|
| Member events | `onWalk`, `onHookTriggered` |

## Schema

| Field | Type | Default | Description | Reference |
|---|---|---|---|---|
| `name` | `string` | `""` | The display name of this state. | `—` |
| `desc` | `string` | `""` | The display description of this state. | `—` |
| `icon` | `file` | `""` | Full logical icon path. The selector root is `/Game/Assets/Icons`. | `—` |
| `stackable` | `bool` | `false` | Whether adding this state again increases the existing state's stack count. | `—` |

## Editing notes

Applying a State creates an Infinite Gameplay Effect and grants the State's event abilities. `onWalk` listens for `Event.Movement.Step`. Trigger `onHookTriggered` explicitly with `Event.State.Trigger.<stateID>`, or call `GlobalFunctions.Gameplay.SendEvent` inside a gameplay graph. Adding or stacking the State does not emit `onHookTriggered`. Activation returns `NoGraph` when the graph has no start node.
