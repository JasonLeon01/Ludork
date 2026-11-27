# GlobalFunctions.Player

`GlobalFunctions.Player` provides Blueprint nodes for querying the primary Player Actor and modifying its inventory, equipment and attributes.

**Metadata source:** `Scripts/GlobalFunctions/Player_meta.lua`

## Player

### Functions and events

| Function | Parameters | Returns | Notes |
|---|---|---|---|
| `GetPlayer` | — | `player: Source.MapActors.Player` | Pure. Returns the primary Player Actor. |
| `GetPlayerFrontPosition` | — | `position: sf.Vector2i` | Pure. Returns the map tile directly in front of the Player. |
| `AddItem` | `itemID: string`, `count: int = 1` | — | `Enums.GeneralData.Item` selector. Adds Items to the Player's inventory. |
| `RemoveItem` | `itemID: string`, `count: int = 1` | `int` | Execution branches: `Success` (`0`), `Failed` (`1`). `Enums.GeneralData.Item` selector. |
| `HasItem` | `itemID: string` | `value: bool` | Pure. `Enums.GeneralData.Item` selector. |
| `GetItemCount` | `itemID: string` | `count: int` | Pure. `Enums.GeneralData.Item` selector. |
| `AddEquip` | `equipID: string`, `count: int = 1` | — | `Enums.GeneralData.Equip` selector. Adds equipment to the Player's inventory. |
| `RemoveEquip` | `equipID: string`, `count: int = 1` | `int` | Execution branches: `Success` (`0`), `Failed` (`1`). `Enums.GeneralData.Equip` selector. |
| `HasEquip` | `equipID: string` | `value: bool` | Pure. `Enums.GeneralData.Equip` selector. |
| `EquipItem` | `equipID: string` | — | `Enums.GeneralData.Equip` selector. Equips the specified item. |
| `UnequipSlot` | `slotID: string` | — | Unequips the equipment in `slotID`. |
| `GetEquipInSlot` | `slotID: string` | `equipID: string` | Pure. Returns the equipment ID equipped in `slotID`. |
| `GetPlayerAttr` | `attrName: string` | `value: any` | Pure. Reads a Player attribute value. |
| `SetPlayerAttr` | `attrName: string`, `value: any` | — | Sets a Player attribute value (writes Base for numeric attributes). |
| `GetPlayerAttrRef` | `attrName: string` | `value: any` | Pure. Returns a mutable reference to a Player attribute. |
| `HealPlayer` | `amount: int = 1` | — | Applies an Instant healing Effect to the Player. |
| `DamagePlayer` | `amount: int = 1` | — | Applies an Instant damage Effect to the Player. |
| `AddHP` | `amount: int = 1` | — | Applies an Instant HP Effect to the Player. |
| `AddGold` | `amount: int = 1` | — | Applies an Instant Gold Effect to the Player. |
| `AddATK` | `amount: int = 1` | — | Applies an Instant ATK Effect to the Player. |
| `AddDEF` | `amount: int = 1` | — | Applies an Instant DEF Effect to the Player. |
| `AddEXP` | `amount: int = 1` | — | Applies an Instant EXP Effect to the Player. |
| `MeetPlayer` | `actors: Engine.Actor[]` | `playerInfo: Union[Source.MapActors.Player, nil]` | Pure. Returns the primary Player if present in `actors`, otherwise `nil`. |

Attribute helpers prefer the generated Player attributes. Numeric setters write Base values. The Heal, Damage and Add-stat nodes apply Instant Effects. The State nodes are in [GlobalFunctions.Gameplay](<Gameplay.md>).
