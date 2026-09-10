# Equip

Source: `Data/General/Equip.json`

| Property | Value |
|---|---|
| Member events | `onEquip`, `onUnequip` |

## Schema

| Field | Type | Default | Description | Reference |
|---|---|---|---|---|
| `name` | `string` | `""` | The display name of this equipment. | `—` |
| `desc` | `string` | `""` | The display description of this equipment. | `—` |
| `icon` | `file` | `""` | Full logical icon path. The selector root is `/Game/Assets/Icons`. | `—` |
| `slot` | `string` | `""` | The `Class` slot key that this equipment occupies. | `—` |
| `attrPlus` | `Dict[string, int]` | `{}` | Attribute names mapped to integer modifiers applied while equipped. | `—` |

## Editing notes

`onEquip` and `onUnequip` are activated explicitly through `Source.Gameplay.GeneralDataGraphAbility`. Activation returns `NoGraph` when the graph has no start node.
