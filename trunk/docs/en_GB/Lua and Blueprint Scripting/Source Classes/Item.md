# Source.MapActors.Item

`Item` is a conditionally visible pickup Actor bound to an Item General Data entry. It declares the item identifier, pickup count and sound effect, adding `count` Items to the Player's inventory on collision.

**Metadata source:** `Scripts/Source/MapActors/Item_meta.lua`

**Inherits:** `Source.MapActors.ConditionalActor` (see [Source.MapActors.ConditionalActor](<ConditionalActor.md>) for inherited visibility conditions).

The editor shows a generated `ItemAttributeSet` preview for `ID`. The `attributes` field is read-only and is not serialised into the Blueprint JSON.

## Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `ID` | `string` | `""` | `Enums.GeneralData.Item` enum selector. |
| `count` | `int` | `1` | Number of Items granted on pickup. |
| `getSE` | `string` | `""` | Sounds path; falls back to `Audio.getSE`. |

## Runtime API

Instance methods omit the implicit `self` receiver (`Source.MapActors.Item`).

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `init` | `texture?`, `rect?`, `tag?` | — | Constructs the generated `ItemAttributeSet` from `ID`. |
| `onCollision` | `other` | — | Event. Adds `count` Items to the Player's inventory through the shared pickup flow. |

The Player grants `GeneralData.Item.<ID>.onUse` while the Item count is positive. Enemy drops explicitly activate `onDrop`.
