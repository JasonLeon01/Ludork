# Source.MapActors.EnemyDamageText

`EnemyDamageText` is the child Actor that displays an Enemy's predicted damage readout on the map when the Player holds the required Item.

**Metadata source:** `Scripts/Source/MapActors/EnemyDamageText_meta.lua`

## EnemyDamageText

**Inherits:** `Engine.Actor`

`EnemyDamageText.EnemyDamageHintLevel` is the shared runtime setting controlled by `enemyDamageHintLevel` in [Gameplay configuration](<../Default Gameplay/Runtime Data Configuration and Saves.md#shared-gameplay-configuration>), which documents its enum values, script default and application timing.

### Properties

| Field | Type | Default | Notes |
|---|---|---|---|
| `tickable` | `bool` | `true` | — |
| `collisionEnabled` | `bool` | `false` | — |
| `requiredItemID` | `string` | `"EnemyBook"` | `Enums.GeneralData.Item` enum selector. |
| `textConfig` | `string` | `"Enemy/DamageReadout"` | Text configuration key for the damage readout. |
| `damageTextOffset` | `sf.Vector2f` | `sf.Vector2f(0, 0)` | Render offset for the damage readout. |

### Functions and events

Events omit the implicit `self` receiver (`Source.MapActors.EnemyDamageText`).

| Method | Parameters | Returns | Notes |
|---|---|---|---|
| `onTick` | `deltaTime: float` | — | Event. Default execution output. |
