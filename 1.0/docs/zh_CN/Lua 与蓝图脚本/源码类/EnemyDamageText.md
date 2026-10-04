# Source.MapActors.EnemyDamageText

`EnemyDamageText` 声明敌人伤害数字所需的 Item、文本配置与偏移。

Metadata 来源：`Scripts/Source/MapActors/EnemyDamageText_meta.lua`

## EnemyDamageText

直接 metadata 基类：`{ "Engine", "Actor" }`

`EnemyDamageText.EnemyDamageHintLevel` 是共享的运行时设置。加载 `Enums.DamageHintLevel` 后，可选择 `NONE = 0`、`BATTLE = 1`（默认）或 `MAP = 2`。`NONE` 隐藏预计伤害提示，`MAP` 还显示移动危险格的合计伤害。提示仍要求持有指定 Item，实际受伤时的伤害粒子不受影响。

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
