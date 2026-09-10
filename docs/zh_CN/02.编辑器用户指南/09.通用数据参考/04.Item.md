# Item

来源：`Data/General/Item.json`

| 属性 | 值 |
|---|---|
| 成员事件 | `onDrop`, `onUse` |

## Schema

| 字段 | 类型 | 默认值 | 描述 | 引用 |
|---|---|---|---|---|
| `name` | `string` | `""` | 该物品的显示名称。 | `—` |
| `desc` | `string` | `""` | 该物品的显示说明。 | `—` |
| `usable` | `bool` | `false` | 是否可以在背包中激活该物品以触发 `onUse`。 | `—` |
| `price` | `int` | `0` | 商店的买入价格。卖出可获得 `floor(price / 2)` 金币。算出的卖出价格为零的物品不会出现在出售列表中。 | `—` |
| `icon` | `file` | `""` | 完整的图标逻辑路径。选择器根路径为 `/Game/Assets/Icons`。 | `—` |
| `cost` | `bool` | `true` | 背包是否显示该物品的堆叠数量。 | `—` |

## 编辑说明

背包以 Player 作为 `GameplayEventData.target` 激活 `onUse`。敌人掉落会激活一次 `onDrop`，target 是新生成的 Item。状态移除节点作用于 Player，因此要放在 `onUse` 中。Item schema 不声明 `onGet` 事件。图中没有起始节点时，激活返回 `NoGraph`。
