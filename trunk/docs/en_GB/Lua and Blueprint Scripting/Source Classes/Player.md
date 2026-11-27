# Source.MapActors.Player

`Player` is the playable Character and Battler Actor. It declares Character defaults alongside Blueprint and Lua APIs for player naming, inventory, equipment, attributes and persistence.

**Metadata source:** `Scripts/Source/MapActors/Player_meta.lua`

**Inherits:** `Engine.Character`, `Source.Battler`

The editor shows a read-only generated `PlayerAttributeSet` preview. The preview is not serialised into the Blueprint JSON.

## Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `ID` | `string` | `"FILL_IT_BY_YOURSELF"` | `Enums.GeneralData.Player` enum selector. |
| `tickable` | `bool` | `true` | — |
| `collisionEnabled` | `bool` | `true` | — |
| `animatable` | `bool` | `true` | — |
| `speed` | `float` | `96` | — |

`attributes` is the generated `PlayerAttributeSet`. Inventory, equipment, movement and the Ability System are runtime state.

## Blueprint functions and events

Instance methods omit the implicit `self` receiver (`Source.MapActors.Player`), which Blueprint nodes expose as the first target pin.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `getDisplayName` | — | `name: string` | Pure. Returns the custom name literally or the localised General Data default. |
| `setName` | `name: string` | `success: bool` | Execution branches: `Success`, `Invalid`. Validates and updates the custom player name. |
| `onFixedTick` | `fixedDelta: float` | — | Event. Default execution output. |
| `addItem` | `itemID: string`, `count: int = 1` | — | `Enums.GeneralData.Item` selector. Adds Items to inventory. |
| `removeItem` | `itemID: string`, `count: int = 1` | `bool` | Execution branches: `Success`, `Failed`. `Enums.GeneralData.Item` selector. |
| `getItemCount` | `itemID: string` | `count: int` | Pure. `Enums.GeneralData.Item` selector. |
| `hasItem` | `itemID: string` | `value: bool` | Pure. `Enums.GeneralData.Item` selector. |
| `addEquip` | `equipID: string`, `count: int = 1` | — | `Enums.GeneralData.Equip` selector. Adds equipment to inventory. |
| `removeEquip` | `equipID: string`, `count: int = 1` | `bool` | Execution branches: `Success`, `Failed`. `Enums.GeneralData.Equip` selector. |
| `equip` | `equipID: string` | — | `Enums.GeneralData.Equip` selector. Equips the item and updates its Infinite Effect. |
| `unequip` | `slotID: string` | — | Unequips the specified slot. Default execution output. |
| `getEquipCount` | `equipID: string` | `count: int` | Pure. `Enums.GeneralData.Equip` selector. |
| `hasEquip` | `equipID: string` | `value: bool` | Pure. `Enums.GeneralData.Equip` selector. |
| `getEquipInfo` | `slotID: string` | `value: string` | Pure. Returns the equipment ID currently in `slotID`. |
| `getForbiddenMoving` | — | `value: bool` | Pure. Returns whether player movement is forbidden. |
| `setForbiddenMoving` | `value: bool` | — | Enables or disables player movement restriction. Default execution output. |

## Lua runtime API

- **`Player.MAX_NAME_GRAPHEMES`** (`int`, `32`) — Maximum player name length in grapheme clusters.

Instance methods omit the implicit `self` receiver (`Source.MapActors.Player`); static functions are called directly on `Player`.

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `ValidateName` | `name: string` | `string`, `Enums.PlayerNameValidationError?` | Static. Returns the trimmed candidate and the validation error (`nil` when valid). |
| `init` | `texture?`, `tag?` | — | Constructs the Attribute Set, Ability System and empty inventory/equipment state. |
| `InitPlayer` | `playerPath: string`, `applyInitialEquipment?: bool = true` | `Source.MapActors.Player` | Static. Constructs a Player from a Blueprint class path and optionally applies Class initial equipment. |
| `FromDict` | `data: table` | `Source.MapActors.Player` | Static. Restores Bases, inventory abilities, equipment Effects and State Effects without applying Class initial equipment or executing member graphs. |
| `asDict` | — | `SaveData` | Serialises stable state, excluding runtime Effect handles. |
| `activateItem` | `itemID: string` | `GlobalCore.GameplayAbilityResult` | Activates `GeneralData.Item.<id>.onUse`. |
| `consumeMovementSpecialPath` | — | `sf.Vector2i[]` | Takes and clears arrived map cells. |
| `getClassPath` | — | `string` | Returns the reconstructable Blueprint class path. |
| `setClassPath` | `path: string` | — | Sets the reconstructable Blueprint class path. |
| `getItems` | — | `table<string, int>` | Returns an independent Item count snapshot; use inventory commands to change the Player. |
| `getEquips` | — | `table<string, int>` | Returns an independent equipment count snapshot; use equipment commands to change the Player. |
| `restoreFromData` | `data: table` | — | Restores saved state into a Player of the same Blueprint class without replaying member graphs. |
| `MeetPlayer` | `actors: Engine.Actor[]`, `player: Source.MapActors.Player` | `Source.MapActors.Player?` | Static. Returns the specified player only when present in the Actor array. |

A runtime equip or unequip changes the Infinite Effect before the corresponding graph runs. Movement completion sends `Event.Movement.Step`.

HP Current is constrained to `0..MAXHP`. Permanent MAXHP and LEVEL growth uses Base deltas. Effect and Constraint changes never alter progression Bases.

`setName` trims Unicode whitespace and accepts between 1 and `Player.MAX_NAME_GRAPHEMES` grapheme clusters. `Enums.PlayerNameValidationError.EMPTY` is `"empty"`; `TOO_LONG` is `"tooLong"`. Invalid input leaves the name unchanged. `getDisplayName` returns the custom name literally or the localised General Data default.
