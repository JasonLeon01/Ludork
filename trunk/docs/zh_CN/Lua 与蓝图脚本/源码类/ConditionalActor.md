# Source.MapActors.ConditionalActor

`ConditionalActor` 是根据游戏变量条件控制可见性的地图 Actor 基类。通过配置变量名、比较运算符与比较值，可在运行时动态显示或隐藏 Actor。

**元数据来源**：`Scripts/Source/MapActors/ConditionalActor_meta.lua`

**继承**：`Engine.Actor`

## 属性

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `conditionVariable` | `string` | `""` | `InstVar` 选择器，仅限 `int`、`float`、`bool` 与 `string` 类型变量。 |
| `conditionOperator` | `string` | `"=="` | 下拉选项：`==`、`~=`、`>`、`>=`、`<`、`<=`。 |
| `conditionValue` | `any` | `0` | 绑定 `InstVarValue = "conditionVariable"`；运行时接受 `number`、`bool` 或 `string`。 |

当变量名为空时，Actor 沿用编辑时配置的 `visible` 值。变量名非空时，目标变量与比较值的标量类型必须严格一致：数值支持全部六种运算符，布尔值与字符串仅支持相等（`==`）和不等（`~=`）。变量缺失、运算符不受支持、类型不匹配或类型不受支持时均会报告错误，不做隐式类型转换。

条件结果会持续驱动 `setVisible(result, false)`：求值为 `false` 时，Actor 会重新隐藏。子 Actor 保持自身的可见性配置，实际可见状态受祖先可见性约束。隐藏后的 Actor 仍保留实例与订阅关系，Tick 行为继续由 `tickable` 与大地图流送机制控制。

`applyConditionVisibility(variables)` 按相同规则对传入的游戏变量表求值并应用一次可见性。该方法不依赖所属 Scene，不注册变量订阅，也不派发生命周期事件；条件为空时保持 Actor 当前可见性不变。

实时 Actor 与楼层预览共用标量求值函数 `Source.Utils.ActorVisibility.Evaluate(variable, operator, comparison, authoredVisible, variables)`。楼层预览在恢复地图记录后直接将结果应用到静态显示记录，不创建 Actor 实例也不注册订阅。隐藏的记录会保留在缓存中以便后续重新求值，且子记录的有效可见性同样要求所有祖先可见。

## 生命周期

`onCreate()` 会订阅 `GameInstance:getVariables()` 并立即执行一次条件求值。订阅以当前地图路径与非空的 `getMapTag()` 作为标识，多个 Actor 可各自监听同一字段。若首次求值失败，系统会先释放订阅再向上抛出错误。

`onDestroy()` 与 `onWorldSleep()` 会取消订阅；`onWorldWake(elapsedSeconds)` 则重新订阅并按当前变量重新求值。在 Lua 或 Script Mixin 中覆写这些方法时必须调用 `super()`；在蓝图生命周期图中也必须延续执行父事件（Game 项目中的空生命周期图会自动继承该行为）。

`releaseConditionMonitor()` 可重复执行以清理条件订阅，且不影响当前可见性或持久化销毁记录。`ConditionalActor.ReleaseMapMonitors(gameMap)` 用于释放离开地图的全部条件订阅：`SceneMap` 在替换地图与销毁场景前会调用该方法，并在移除被抑制的大地图 Actor 身份前释放其整棵子树。场景被临时覆盖时会保留订阅，以便返回场景后恢复监听。

条件订阅及由条件推导出的可见性均不会写入存档，存档快照仅记录变量当前的逻辑值。加载地图时，已销毁的摆放项会在 `onCreate` 之前被移除，因此已拾取的物品不会重新出现。

## 继承

`Enemy`、`Item`、`Equip`、`DoorBase` 与 `Teleporter` 均继承自此类。`Bottle`、`Gem` 以及 Game 项目中其他直接派生的地图 Actor 蓝图以 `Source.MapActors.ConditionalActor.ConditionalActor` 为父类。`EnemyDamageText` 则直接继承 `Engine.Actor`，并使用独立的伤害提示逻辑。
