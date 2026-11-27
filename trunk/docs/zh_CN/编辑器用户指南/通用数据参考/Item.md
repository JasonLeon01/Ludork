# Item

定义背包物品及其可用性、商店价格、图标表现与掉落/使用能力图。

**来源**：`Data/General/Item.json` · **成员事件**：`onDrop`、`onUse`

## Schema

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `name` | `string` | `""` | 该物品的显示名称。 |
| `desc` | `string` | `""` | 该物品的显示说明。 |
| `usable` | `bool` | `false` | 是否允许从背包中激活并触发 `onUse`。 |
| `price` | `int` | `0` | 商店购入价格；出售获得 `floor(price / 2)` 金币，售价为 `0` 的物品不列入可出售清单。 |
| `icon` | `file` | `""` | 规范逻辑图标路径（选择器根目录：`/Game/Assets/Icons`）。 |
| `cost` | `bool` | `true` | 是否在背包中显示该物品的堆叠数量。 |

## 编辑说明

- **`onUse`**：由背包激活，传入玩家作为 `GameplayEventData.target`。由于状态移除节点作用于玩家，应放置在此图中。
- **`onDrop`**：在敌人掉落时激活一次，传入生成的 Item Actor 作为 `GameplayEventData.target`。`Item` Schema 不声明 `onGet` 事件。
