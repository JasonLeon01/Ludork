# Source.MapActors.EnemyDamageText

`EnemyDamageText` 是用于在地图上显示敌人预测伤害读数的子 Actor，当玩家持有指定物品时显示伤害信息。

**元数据来源**：`Scripts/Source/MapActors/EnemyDamageText_meta.lua`

## EnemyDamageText

**继承**：`Engine.Actor`

`EnemyDamageText.EnemyDamageHintLevel` 是由[玩法配置](<../默认玩法/运行时数据、配置与存档.md#共享玩法配置>)中的 `enemyDamageHintLevel` 控制的共享运行时设置，具体枚举值、脚本默认值与生效时机见该节。

### 属性

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `tickable` | `bool` | `true` | — |
| `collisionEnabled` | `bool` | `false` | — |
| `requiredItemID` | `string` | `"EnemyBook"` | `Enums.GeneralData.Item` 枚举选择器。 |
| `textConfig` | `string` | `"Enemy/DamageReadout"` | 伤害读数所使用的文本配置键。 |
| `damageTextOffset` | `sf.Vector2f` | `sf.Vector2f(0, 0)` | 伤害读数的渲染位置偏移。 |

### 函数与事件

事件省略隐式接收者 `self`（`Source.MapActors.EnemyDamageText`）。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `onTick` | `deltaTime: float` | — | 事件（Event）。默认执行出口。 |
