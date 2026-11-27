# GlobalFunctions.Player

`GlobalFunctions.Player` 提供用于查询主玩家 Actor 以及修改其背包、装备和属性的蓝图节点。

**元数据来源**：`Scripts/GlobalFunctions/Player_meta.lua`

## Player

### 函数与事件

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `GetPlayer` | — | `player: Source.MapActors.Player` | 纯函数（Pure）。返回主玩家 Actor。 |
| `GetPlayerFrontPosition` | — | `position: sf.Vector2i` | 纯函数（Pure）。返回玩家正前方的地图格坐标。 |
| `AddItem` | `itemID: string`, `count: int = 1` | — | `Enums.GeneralData.Item` 选择器。向玩家背包添加物品。 |
| `RemoveItem` | `itemID: string`, `count: int = 1` | `int` | 执行分支：`Success`（`0`）、`Failed`（`1`）。`Enums.GeneralData.Item` 选择器。 |
| `HasItem` | `itemID: string` | `value: bool` | 纯函数（Pure）。`Enums.GeneralData.Item` 选择器。 |
| `GetItemCount` | `itemID: string` | `count: int` | 纯函数（Pure）。`Enums.GeneralData.Item` 选择器。 |
| `AddEquip` | `equipID: string`, `count: int = 1` | — | `Enums.GeneralData.Equip` 选择器。向玩家背包添加装备。 |
| `RemoveEquip` | `equipID: string`, `count: int = 1` | `int` | 执行分支：`Success`（`0`）、`Failed`（`1`）。`Enums.GeneralData.Equip` 选择器。 |
| `HasEquip` | `equipID: string` | `value: bool` | 纯函数（Pure）。`Enums.GeneralData.Equip` 选择器。 |
| `EquipItem` | `equipID: string` | — | `Enums.GeneralData.Equip` 选择器。穿戴指定装备。 |
| `UnequipSlot` | `slotID: string` | — | 卸下 `slotID` 槽位上的装备。 |
| `GetEquipInSlot` | `slotID: string` | `equipID: string` | 纯函数（Pure）。返回 `slotID` 槽位当前穿戴的装备 ID。 |
| `GetPlayerAttr` | `attrName: string` | `value: any` | 纯函数（Pure）。读取指定玩家属性的值。 |
| `SetPlayerAttr` | `attrName: string`, `value: any` | — | 设置指定玩家属性的值（数值属性写入 Base）。 |
| `GetPlayerAttrRef` | `attrName: string` | `value: any` | 纯函数（Pure）。返回指定玩家属性的可变引用。 |
| `HealPlayer` | `amount: int = 1` | — | 对玩家应用即时（Instant）治疗效果。 |
| `DamagePlayer` | `amount: int = 1` | — | 对玩家应用即时（Instant）伤害效果。 |
| `AddHP` | `amount: int = 1` | — | 对玩家应用即时 HP 增减效果。 |
| `AddGold` | `amount: int = 1` | — | 对玩家应用即时金币（Gold）增减效果。 |
| `AddATK` | `amount: int = 1` | — | 对玩家应用即时攻击力（ATK）增减效果。 |
| `AddDEF` | `amount: int = 1` | — | 对玩家应用即时防御力（DEF）增减效果。 |
| `AddEXP` | `amount: int = 1` | — | 对玩家应用即时经验值（EXP）增减效果。 |
| `MeetPlayer` | `actors: Engine.Actor[]` | `playerInfo: Union[Source.MapActors.Player, nil]` | 纯函数（Pure）。若 `actors` 包含主玩家则返回该玩家实例，否则返回 `nil`。 |

属性辅助节点优先访问生成的玩家属性；数值写入节点直接修改 Base 值。治疗、伤害与各项属性增减节点均应用即时（Instant）效果。状态（State）相关节点见 [GlobalFunctions.Gameplay](<Gameplay.md>)。
