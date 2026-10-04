# GlobalFunctions.Player

`Player` 声明读取与修改 Player 状态、物品栏和装备的节点。

Metadata 来源：`Scripts/GlobalFunctions/Player_meta.lua`

## Player

直接 metadata 基类：—

### 属性

未声明任何可编辑属性。

### 函数与事件

| 名称 | 种类 | 参数 | 返回 | 执行语义与 metadata |
|---|---|---|---|---|
| `GetPlayer` | `function` | — | player: { "Source.MapActors.Player", "Player" } | Pure |
| `GetPlayerFrontPosition` | `function` | — | position: sf.Vector2i | Pure |
| `AddItem` | `function` | itemID: { enum = "Enums.GeneralData.Item", valueType = "string" }; count: int = 1 | — | ExecSplit { [1] = "default", default = nil } |
| `RemoveItem` | `function` | itemID: { enum = "Enums.GeneralData.Item", valueType = "string" }; count: int = 1 | return: int | ExecSplit { [1] = "Success", [2] = "Failed", Success = { [1] = 0 }, Failed = { [1] = 1 } } |
| `HasItem` | `function` | itemID: { enum = "Enums.GeneralData.Item", valueType = "string" } | value: bool | Pure |
| `GetItemCount` | `function` | itemID: { enum = "Enums.GeneralData.Item", valueType = "string" } | count: int | Pure |
| `AddEquip` | `function` | equipID: { enum = "Enums.GeneralData.Equip", valueType = "string" }; count: int = 1 | — | ExecSplit { [1] = "default", default = nil } |
| `RemoveEquip` | `function` | equipID: { enum = "Enums.GeneralData.Equip", valueType = "string" }; count: int = 1 | return: int | ExecSplit { [1] = "Success", [2] = "Failed", Success = { [1] = 0 }, Failed = { [1] = 1 } } |
| `HasEquip` | `function` | equipID: { enum = "Enums.GeneralData.Equip", valueType = "string" } | value: bool | Pure |
| `EquipItem` | `function` | equipID: { enum = "Enums.GeneralData.Equip", valueType = "string" } | — | ExecSplit { [1] = "default", default = nil } |
| `UnequipSlot` | `function` | slotID: string | — | ExecSplit { [1] = "default", default = nil } |
| `GetEquipInSlot` | `function` | slotID: string | equipID: string | Pure |
| `GetPlayerAttr` | `function` | attrName: string | value: any | Pure |
| `SetPlayerAttr` | `function` | attrName: string; value: any | — | ExecSplit { [1] = "default", default = nil } |
| `GetPlayerAttrRef` | `function` | attrName: string | value: any | Pure |
| `HealPlayer` | `function` | amount: int = 1 | — | ExecSplit { [1] = "default", default = nil } |
| `DamagePlayer` | `function` | amount: int = 1 | — | ExecSplit { [1] = "default", default = nil } |
| `AddHP` | `function` | amount: int = 1 | — | ExecSplit { [1] = "default", default = nil } |
| `AddGold` | `function` | amount: int = 1 | — | ExecSplit { [1] = "default", default = nil } |
| `AddATK` | `function` | amount: int = 1 | — | ExecSplit { [1] = "default", default = nil } |
| `AddDEF` | `function` | amount: int = 1 | — | ExecSplit { [1] = "default", default = nil } |
| `AddEXP` | `function` | amount: int = 1 | — | ExecSplit { [1] = "default", default = nil } |
| `MeetPlayer` | `function` | actors: { "Engine", "Actor[]" } | playerInfo: Union[Source.MapActors.Player.Player, nil] | Pure |

属性辅助节点优先使用生成的 Player 属性。数值设置节点写入 Base 值。治疗、伤害和加属性节点应用 Instant Effect。State 节点见 [GlobalFunctions.Gameplay](<Gameplay.md>)。
