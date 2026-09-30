# Equip

来源：`Data/General/Equip.json`

| 属性 | 值 |
|---|---|
| 成员事件 | `onEquip`, `onUnequip` |

## Schema

| 字段 | 类型 | 默认值 | 描述 | 引用 |
|---|---|---|---|---|
| `name` | `string` | `""` | 该装备的显示名称。 | `—` |
| `desc` | `string` | `""` | 该装备的显示说明。 | `—` |
| `icon` | `file` | `""` | 完整的图标逻辑路径。选择器根路径为 `/Game/Assets/Icons`。 | `—` |
| `slot` | `string` | `""` | 该装备占用的 `Class` 槽位 key。 | `—` |
| `attrPlus` | `Dict[string, int]` | `{}` | 属性名到装备期间生效的整数修正值的映射。 | `—` |

## 编辑说明

`onEquip` 和 `onUnequip` 由 `Source.Gameplay.GeneralDataGraphAbility` 显式激活。图中没有起始节点时，激活返回 `NoGraph`。
