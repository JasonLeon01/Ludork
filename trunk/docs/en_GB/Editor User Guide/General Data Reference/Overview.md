# General Data Reference Overview

Reference for the seven General Data schemas shipped with the Game project (`Class`, `Enemy`, `Equip`, `Item`, `Player`, `Special` and `State`). Because General Data is project data rather than fixed engine enumerations, each project can freely modify or extend these schemas.

## Where the data lives

Each type lives in `Data/General/<Type>.json`. Edit schemas, members and member ability graphs in **Database → General Data** (`F10`); see [General Data and Text Config](<../General Data and Text Config.md#type-structure>) for field types and generated modules.

## Built-in schemas

| Schema | Purpose | Member events |
|---|---|---|
| [Class](<Class.md>) | Player class presets: display name, description and `slot` mapping equipment slots to initial equipment. | — |
| [Enemy](<Enemy.md>) | Enemy stats (`MAXHP`, `ATK`, `DEF`, `EXP`, `GOLD`), `drops` (Blueprint paths with map-cell offsets), `special` parameters and attack animation. | — |
| [Equip](<Equip.md>) | Equipment records: icon, target `Class` slot key and `attrPlus` stat modifiers applied while equipped. | `onEquip`, `onUnequip` |
| [Item](<Item.md>) | Inventory items: `usable` flag, shop `price` (and derived sell price), icon and `cost` stack-count display. | `onDrop`, `onUse` |
| [Player](<Player.md>) | Player presets: referenced `CLASS` member plus starting HP, stats, experience, gold, level and attack animation. | — |
| [Special](<Special.md>) | Presentation-only records (name, description and icon) for enemy special traits whose parameters live in `Enemy.special`. | — |
| [State](<State.md>) | Status effects, including whether `stackable` increments an existing state's stack count. | `onWalk`, `onHookTriggered` |

- **Enum references:** Cross-type references use enum schemas backed by string IDs — `Player.CLASS` uses `Enums.GeneralData.Class`, `Enemy.special` keys use `Enums.GeneralData.Special`, and attack animations use `Enums.Animation`.
- **Member events:** Activated explicitly via `Source.Gameplay.GeneralDataGraphAbility` (a missing start node returns `NoGraph`). See each schema page for callers and event contexts.

## Brace-wrapped display strings

Display fields can store localisation keys such as `{WARRIOR}`, resolved at display time rather than load time. See [Game Localisation Workflow](<../../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md#runtime>) for formatting and language switching.
