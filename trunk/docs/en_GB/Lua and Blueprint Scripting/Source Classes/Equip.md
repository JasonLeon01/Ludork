# Source.MapActors.Equip

`Equip` is a conditionally visible pickup Actor bound to an Equipment General Data entry. It declares the equipment identifier and pickup sound effect, adding itself to the Player's inventory on collision.

**Metadata source:** `Scripts/Source/MapActors/Equip_meta.lua`

**Inherits:** `Source.MapActors.ConditionalActor` (see [Source.MapActors.ConditionalActor](<ConditionalActor.md>) for inherited visibility conditions).

The editor shows a generated `EquipAttributeSet` preview for `ID`. The `attributes` field is read-only and is not serialised into the Blueprint JSON.

## Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `ID` | `string` | `"FILL_IT_BY_YOURSELF"` | `Enums.GeneralData.Equip` enum selector. |
| `getSE` | `string` | `""` | Sounds path; falls back to `Audio.getSE`. |

## Runtime API

Instance methods omit the implicit `self` receiver (`Source.MapActors.Equip`).

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `init` | `texture?`, `rect?`, `tag?` | — | Constructs the generated `EquipAttributeSet` from `ID`. |
| `onCollision` | `other` | — | Event. Adds the Equip to the Player's inventory through the shared pickup flow. |

`Player:equip` applies `attributes.attrPlus` as an Infinite Effect and explicitly runs the General Data `onEquip` or `onUnequip` ability.
