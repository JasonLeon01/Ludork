# Source.MapActors.Equip

`Equip` declares the General Data identifier and the sound effect of an equipment Actor.

Metadata source: `Scripts/Source/MapActors/Equip_meta.lua`

Direct metadata base: `{ "Source.MapActors.ConditionalActor", "ConditionalActor" }`

Inherited visibility conditions: [Source.MapActors.ConditionalActor](<ConditionalActor.md>).

The editor shows a generated `EquipAttributeSet` preview for `ID`. The `attributes` field is read-only and is not serialised into the Blueprint JSON.

## Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `ID` | `{ enum = "Enums.GeneralData.Equip", valueType = "string" }` | `"FILL_IT_BY_YOURSELF"` | — |
| `getSE` | `string` | `""` | Sounds path; `Audio.getSE` fallback |

## Runtime API

| Member | Signature | Behaviour |
|---|---|---|
| `init` | `(texture?, rect?, tag?)` | Constructs the generated Equip Attribute Set from `ID` |
| `onCollision` | `(other)` | Adds the Equip to Player inventory through the shared pickup flow |

`Player:equip` applies `attributes.attrPlus` as an Infinite Effect and explicitly runs the General Data `onEquip` or `onUnequip` ability.
