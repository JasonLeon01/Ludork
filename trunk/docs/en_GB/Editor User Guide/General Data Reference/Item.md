# Item

Defines inventory items, their usability, shop pricing, icon presentation and drop/use ability graphs.

**Source:** `Data/General/Item.json` · **Member events:** `onDrop`, `onUse`

## Schema

| Field | Type | Default | Description |
|---|---|---|---|
| `name` | `string` | `""` | Display name of this item. |
| `desc` | `string` | `""` | Display description of this item. |
| `usable` | `bool` | `false` | Whether the item can be activated from the inventory to trigger `onUse`. |
| `price` | `int` | `0` | Shop buy price. Selling grants `floor(price / 2)` gold; items whose sell price is `0` are not listed for sale. |
| `icon` | `file` | `""` | Canonical logical icon path (picker root: `/Game/Assets/Icons`). |
| `cost` | `bool` | `true` | Whether the inventory displays this item's stack count. |

## Editing notes

- **`onUse`:** Activated by the inventory with the Player as `GameplayEventData.target`. Place state-removal nodes here because they act on the Player.
- **`onDrop`:** Activated once by enemy drops with the spawned Item Actor as `GameplayEventData.target`. The `Item` schema declares no `onGet` event.
