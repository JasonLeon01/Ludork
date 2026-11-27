# Source.MapActors.Enemy

`Enemy` 是绑定到 Enemy 通用数据条目的条件可见战斗 Actor，声明了伤害文本子组件、动画开关与战后变量变更，并提供 `onCollision` 与 `onDefeat` 事件。

**元数据来源**：`Scripts/Source/MapActors/Enemy_meta.lua`

**继承**：`Source.MapActors.ConditionalActor`、`Source.Battler`（继承的可见性条件见 [Source.MapActors.ConditionalActor](<ConditionalActor.md>)）。

蓝图编辑器会合成只读的 `Source.Configs.GeneralDataTypes.EnemyAttributeSet` 属性预览（`attributes`），该预览不会序列化到蓝图 JSON 中。

## 属性

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `ID` | `string` | `"FILL_IT_BY_YOURSELF"` | `Enums.GeneralData.Enemy` 枚举选择器。 |
| `childActorComp` | `Source.Components.ChildActorComponent` | 伤害文本子 Actor | 组件。 |
| `collisionEnabled` | `bool` | `true` | — |
| `animatable` | `bool` | `true` | — |
| `animateWithoutMoving` | `bool` | `true` | — |
| `afterBattleVarChanges` | `Dict[string, Tuple[string, any]]` | `{}` | 数值型游戏变量选择器与固定运算符选择器。 |

共享运行时设置 `DefeatShatterEffectEnabled` 由[玩法配置](<../默认玩法/运行时数据、配置与存档.md#共享玩法配置>)中的 `enemyDefeatShatterEffectEnabled` 控制。`attributes` 字段持有生成的 Enemy Attribute Set。

`Enemy` 继承 `Engine.Actor` 的默认配置 `tickable = false`。

## 函数与事件

事件省略隐式接收者 `self`（`Source.MapActors.Enemy`）。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `onCollision` | `other: Engine.Actor[]` | — | 事件（Event）。默认执行出口。 |
| `onDefeat` | — | — | 事件（Event）。默认执行出口。 |

`init(texture?, rect?, tag?)` 会构造 Attribute Set、Ability System、Mota 战斗能力及配置的特殊效果。碰撞触发时，会先预校验战斗结果与必需的结算数据，再提交反击伤害。若战斗失败，则待动画播放完毕后应用预先准备好的 Game Over 效果。

战斗胜利动画结束后，系统会先执行 Actor 蓝图的 `onDefeat` 回调，随后再执行强制销毁、重生（Reborn）/掉落、战利品奖励与状态（State）应用。这些结算数值均取自预先准备的快照，`afterBattleVarChanges` 则在 `onDefeat` 回调之后求值。Enemy 通用数据不包含 `onDefeat` 成员图。
