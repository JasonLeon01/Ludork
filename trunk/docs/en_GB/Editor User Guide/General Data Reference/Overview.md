# General Data Reference Overview

This group documents the General Data types that ship with the Game project. Every value in these tables is project data rather than an engine enumeration, so a project is free to redefine the same type. Each page is generated from the current schema in `Data/General`, states its source file, and lists that schema's fields, defaults and member events.

## Where the data lives

Each type is stored in `Data/General/<Type>.json`. Edit schemas, members and ability graphs through **Database → General Data** (`F10`); see [General Data and Text Config](<../General Data and Text Config.md#type-structure>) for field options and generated files.

## What each page documents

| Page | Contents | Member events |
|---|---|---|
| [Class](<Class.md>) | The player class preset: display name and description, plus `slot` mapping equipment slot names to the equipment initially equipped in each slot. | — |
| [Enemy](<Enemy.md>) | Enemy statistics (`MAXHP`, `ATK`, `DEF`, `EXP`, `GOLD`), `drops` as Blueprint class paths with map-cell offsets, `special` parameters and the attack animation. | — |
| [Equip](<Equip.md>) | Equipment: icon, the `Class` slot key it occupies and `attrPlus` attribute modifiers applied while equipped. | `onEquip`, `onUnequip` |
| [Item](<Item.md>) | Inventory items: `usable`, shop `price` and the derived sell price, icon and `cost`. | `onDrop`, `onUse` |
| [Player](<Player.md>) | Player presets: the referenced `CLASS` member and the starting HP, attribute, experience, gold and level values. | — |
| [Special](<Special.md>) | Presentation-only records — name, description and icon — whose parameters remain in `Enemy.special`. | — |
| [State](<State.md>) | States, including whether `stackable` increases an existing state's stack count. | `onWalk`, `onHookTriggered` |

Project references are expressed directly in enum schemas: `Player.CLASS` uses `Enums.GeneralData.Class`, `Enemy.special` uses `Enums.GeneralData.Special` as its dictionary key, and attack animations use `Enums.Animation`. Their stored values remain strings.

Member events are activated explicitly through `Source.Gameplay.GeneralDataGraphAbility`; a missing start node returns `NoGraph`. The individual pages describe type-specific callers and contexts.

## Brace-wrapped display strings

Display fields can contain locale keys such as `{WARRIOR}`. Resolve them when displaying text; see [Game Localisation Workflow](<../../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md#runtime>) for formatting and language changes.

## Related pages

- [Blueprints, Common Functions and Actors](<../Blueprints Common Functions and Actors.md>) — Blueprint class paths referenced by these schemas
