# GlobalFunctions.Player

`Player` declares the nodes that read and change Player state, inventory and equipment.

Metadata source: `Scripts/GlobalFunctions/Player_meta.lua`

## Player

Direct metadata bases: —

### Properties

No editable properties are declared.

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
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

Attribute helpers prefer the generated Player attributes. Numeric setters write Base values. The Heal, Damage and Add-stat nodes apply Instant Effects. The State nodes are in [GlobalFunctions.Gameplay](<Gameplay.md>).
