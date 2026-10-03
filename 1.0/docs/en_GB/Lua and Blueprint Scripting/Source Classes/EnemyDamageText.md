# Source.MapActors.EnemyDamageText

`EnemyDamageText` declares the required Item, text configuration and offset of the Enemy damage readout.

Metadata source: `Scripts/Source/MapActors/EnemyDamageText_meta.lua`

## EnemyDamageText

Direct metadata bases: `{ "Engine", "Actor" }`

Meta: `{ GeneralDataVars = { [1] = { [1] = "requiredItemID", [2] = "Item" } } }`

`EnemyDamageText.EnemyDamageHintLevel` is the shared runtime setting. Load `Enums.DamageHintLevel` to choose `NONE = 0`, `BATTLE = 1` (default), or `MAP = 2`. `NONE` hides expected-damage hints; `MAP` also enables movement-danger cell totals. The required Item still gates hints, and actual damage particles are unaffected.

### Properties

| Name | Type | Default | Metadata |
|---|---|---|---|
| `tickable` | `bool` | `true` | — |
| `collisionEnabled` | `bool` | `false` | — |
| `requiredItemID` | `string` | `"EnemyBook"` | Meta { GeneralDataVars = "Item" } |
| `textConfig` | `string` | `"Enemy/DamageReadout"` | — |
| `damageTextOffset` | `sf.Vector2f` | `{ [1] = 0, [2] = 0 }` | — |

### Functions and events

| Name | Kind | Parameters | Returns | Execution and metadata |
|---|---|---|---|---|
| `onTick` | `event` | deltaTime: float | — | ExecSplit { [1] = "default", default = nil } |
