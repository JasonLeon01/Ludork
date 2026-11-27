# Source.MapActors.Item

`Item` 是绑定到物品通用数据条目的条件可见拾取 Actor，声明了物品标识、拾取数量与拾取音效，在发生碰撞时向玩家背包发放 `count` 个物品。

**元数据来源**：`Scripts/Source/MapActors/Item_meta.lua`

**继承**：`Source.MapActors.ConditionalActor`（继承的可见性条件见 [Source.MapActors.ConditionalActor](<ConditionalActor.md>)）。

编辑器会根据 `ID` 显示生成的 `ItemAttributeSet` 预览。`attributes` 字段为只读，不会序列化到蓝图 JSON 中。

## 属性

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `ID` | `string` | `""` | `Enums.GeneralData.Item` 枚举选择器。 |
| `count` | `int` | `1` | 拾取时发放的物品数量。 |
| `getSE` | `string` | `""` | 音效路径（Sounds）；配置回退项：`Audio.getSE`。 |

## 运行时 API

实例方法省略隐式接收者 `self`（`Source.MapActors.Item`）。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `init` | `texture?`, `rect?`, `tag?` | — | 根据 `ID` 构造生成的 `ItemAttributeSet`。 |
| `onCollision` | `other` | — | 事件（Event）。通过共享拾取流程将 `count` 个物品加入玩家背包。 |

当玩家持有的某物品数量大于 `0` 时，系统会授予 `GeneralData.Item.<ID>.onUse` 能力。敌人掉落该物品时则会显式激活 `onDrop`。
