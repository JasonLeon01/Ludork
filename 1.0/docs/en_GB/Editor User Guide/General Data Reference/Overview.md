# General Data Reference Overview

This group documents the General Data types that ship with the Game project. Every value in these tables is project data rather than an engine enumeration, so a project is free to redefine the same type. Each page is generated from the current schema in `Data/General`, states its source file, and lists that schema's fields, defaults and member events.

## Where the data lives

General Data is stored in the project's `Data/General` directory, one JSON file per type. The Game project contains exactly the seven files below.

| File | Page |
|---|---|
| `Data/General/Class.json` | [Class](<Class.md>) |
| `Data/General/Enemy.json` | [Enemy](<Enemy.md>) |
| `Data/General/Equip.json` | [Equip](<Equip.md>) |
| `Data/General/Item.json` | [Item](<Item.md>) |
| `Data/General/Player.json` | [Player](<Player.md>) |
| `Data/General/Special.json` | [Special](<Special.md>) |
| `Data/General/State.json` | [State](<State.md>) |

Each file holds the ordered field definitions under `params`, the named records under `members`, and optional `events`. The type structure, supported field types and project enum schemas are described in [General Data and Text Config](<../General Data and Text Config.md#type-structure>). To add an enum field, choose `enum` and an existing module through **kind**; its saved `type` is an enum schema and each member stores the selected scalar value. Member ability graphs remain graph-only and do not use Blueprint `attrDefs`.

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

## Brace-wrapped display strings

General Data preserves every string exactly as stored, and display fields in the Game project hold locale keys wrapped in braces, such as `{WARRIOR}`. Nothing resolves those tokens on load: the catalogues are authored in `Data/Locale/Locale.xlsx` and exported to `Scripts/Source/Locale/<language>.lua`, and gameplay resolves a bare key or each `{ID}` inside a string with `Locale.ApplyStringLocaleFormat`, normally bound to a local `LOC`. Missing keys remain unchanged, doubled `{{` and `}}` produce literal braces, and an unavailable language falls back to `en_GB` where possible. The editor can also show a hint for a field whose complete trimmed text is one `{ID}` token; spaces and nested braces invalidate it. `Locale.SetLanguage` publishes `LocaleChanged`, so long-lived UI resolves its stored keys again. See [Game Localisation Workflow](<../../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md#runtime>) for the catalogue format and the formatter contract.

## Relation to the General Data editor

These pages are reference output, not an editing surface. Schemas, members and member ability graphs are edited in **Database → General Data** (`F10`), whose Form and Table views, search behaviour, enum selectors and shared Undo history are documented in [General Data and Text Config](<../General Data and Text Config.md>). Saving there regenerates `Enums.GeneralDataKey`, each `Enums.GeneralData.<TypeName>` module, and `Source.Configs.GeneralDataTypes`, and the runtime activates member graphs explicitly through `Source.Gameplay.GeneralDataGraphAbility`.

## Related pages

- [General Data and Text Config](<../General Data and Text Config.md>) — the editor window, schema authoring and generated Lua
- [Game Localisation Workflow](<../../Plug-in Development and Installation/Official Plug-ins/Game Localisation Workflow.md>) — workbook, Export and runtime resolution of `{ID}` tokens
- [Blueprints, Common Functions and Actors](<../Blueprints Common Functions and Actors.md>) — Blueprint class paths referenced by these schemas
