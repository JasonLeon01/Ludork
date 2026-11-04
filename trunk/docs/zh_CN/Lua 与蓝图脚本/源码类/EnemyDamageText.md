# Source.MapActors.EnemyDamageText

`EnemyDamageText` 声明敌人伤害数字所需的 Item、文本配置与偏移。

Metadata 来源：`Scripts/Source/MapActors/EnemyDamageText_meta.lua`

## EnemyDamageText

直接 metadata 基类：`{ "Engine", "Actor" }`

`EnemyDamageText.EnemyDamageHintLevel` 是由 [Gameplay 配置](<../Game 玩法/运行时数据、配置与存档.md#共享玩法配置>)中的 `enemyDamageHintLevel` 控制的共享运行时设置；枚举值、脚本默认值与生效时机见该节。

### 属性

| 名称 | 类型 | 默认值 | Metadata |
|---|---|---|---|
| `tickable` | `bool` | `true` | — |
| `collisionEnabled` | `bool` | `false` | — |
| `requiredItemID` | `{ enum = "Enums.GeneralData.Item", valueType = "string" }` | `"EnemyBook"` | — |
| `textConfig` | `string` | `"Enemy/DamageReadout"` | — |
| `damageTextOffset` | `sf.Vector2f` | `{ [1] = 0, [2] = 0 }` | — |

### 函数与事件

| 名称 | 种类 | 参数 | 返回 | 执行语义与 metadata |
|---|---|---|---|---|
| `onTick` | `event` | deltaTime: float | — | ExecSplit { [1] = "default", default = nil } |
