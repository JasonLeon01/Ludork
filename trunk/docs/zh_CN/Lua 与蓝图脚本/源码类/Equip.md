# Source.MapActors.Equip

`Equip` 是绑定到装备通用数据条目的条件可见拾取 Actor，声明了装备标识与拾取音效，在发生碰撞时将对应装备加入玩家背包。

**元数据来源**：`Scripts/Source/MapActors/Equip_meta.lua`

**继承**：`Source.MapActors.ConditionalActor`（继承的可见性条件见 [Source.MapActors.ConditionalActor](<ConditionalActor.md>)）。

编辑器会根据 `ID` 显示生成的 `EquipAttributeSet` 预览。`attributes` 字段为只读，不会序列化到蓝图 JSON 中。

## 属性

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `ID` | `string` | `"FILL_IT_BY_YOURSELF"` | `Enums.GeneralData.Equip` 枚举选择器。 |
| `getSE` | `string` | `""` | 音效路径（Sounds）；配置回退项：`Audio.getSE`。 |

## 运行时 API

实例方法省略隐式接收者 `self`（`Source.MapActors.Equip`）。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `init` | `texture?`, `rect?`, `tag?` | — | 根据 `ID` 构造生成的 `EquipAttributeSet`。 |
| `onCollision` | `other` | — | 事件（Event）。通过共享拾取流程将装备加入玩家背包。 |

调用 `Player:equip` 时，系统会将 `attributes.attrPlus` 作为无限持续（Infinite）效果应用，并显式执行通用数据的 `onEquip` 或 `onUnequip` 能力。
