# Source.MapActors.EnemyDamageText

`EnemyDamageText` declares the required Item, text configuration and offset of the Enemy damage readout.

Metadata source: `Scripts/Source/MapActors/EnemyDamageText_meta.lua`

## EnemyDamageText

Direct metadata bases: `{ "Engine", "Actor" }`

`EnemyDamageText.EnemyDamageHintLevel` is the shared runtime setting controlled by `enemyDamageHintLevel` in [Gameplay configuration](<../Default Gameplay/Runtime Data Configuration and Saves.md#shared-gameplay-configuration>), which documents its enum values, script default and application timing.

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `tickable` | `bool` | `true` | — |
| `collisionEnabled` | `bool` | `false` | — |
| `requiredItemID` | `{ enum = "Enums.GeneralData.Item", valueType = "string" }` | `"EnemyBook"` | — |
| `textConfig` | `string` | `"Enemy/DamageReadout"` | — |
| `damageTextOffset` | `sf.Vector2f` | `{ [1] = 0, [2] = 0 }` | — |

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `onTick` | `event` | deltaTime: float | — | ExecSplit { [1] = "default", default = nil } |
