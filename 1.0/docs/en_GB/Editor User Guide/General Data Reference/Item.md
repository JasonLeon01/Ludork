# Item

Source: `Data/General/Item.json`

| Property | Value |
|---|---|
| Member events | `onDrop`, `onUse` |

## Schema

| Field | Type | Default | Description | Reference |
|---|---|---|---|---|
| `name` | `string` | `""` | The display name of this item. | `—` |
| `desc` | `string` | `""` | The display description of this item. | `—` |
| `usable` | `bool` | `false` | Whether the item can be activated from the inventory to trigger `onUse`. | `—` |
| `price` | `int` | `0` | The shop buy price. Selling grants `floor(price / 2)` gold. Items whose resulting sell price is zero are not listed for sale. | `—` |
| `icon` | `file` | `""` | Full logical icon path. The selector root is `/Game/Assets/Icons`. | `—` |
| `cost` | `bool` | `true` | Whether the inventory displays this item's stack count. | `—` |

## Editing notes

The inventory activates `onUse` with the Player as `GameplayEventData.target`. Enemy drops activate `onDrop` once, with the spawned Item as the target. State-removal nodes act on the Player, so place them in `onUse`. The Item schema declares no `onGet` event. Activation returns `NoGraph` when the graph has no start node.
