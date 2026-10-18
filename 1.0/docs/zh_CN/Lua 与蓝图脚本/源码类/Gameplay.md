# Source Gameplay API

`Source.Gameplay` 包含基于 `GlobalCore` 的 Game 专用 ability 与 Effect。

## GameplayScene

模块：`Source.Gameplay.GameplayScene`，派生自 `GlobalCore.SceneBase`。`SceneMap` 实现该契约，Actor 通过所属地图访问它。

| 成员 | 签名 | 行为 |
|---|---|---|
| `getGameMap` | `() -> GameMap` | 返回场景已加载的地图 |
| `getGameInstance` | `() -> GameInstance` | 返回拥有变量和持久化状态的游戏实例 |
| `recordAddedActor`、`recordDestroyedActor` | `(actor)` | 记录对应的持久化变更 |
| `recordActorPosition` | `(actor, position)` | 记录 Actor 的地图位置 |
| `requestFloorStep` | `(teleporter, step) -> boolean` | 接受一次相邻楼层请求；`step` 必须为 `1` 或 `-1`。抵达时选择最近的、可见且未销毁的相反方向楼梯 |
| `requestMapTransfer` | `(teleporter, mapPath, position, record = true) -> boolean` | 接受一次指定地图坐标传送，不搜索楼梯；可选记录两端各自地图的传送点 |
| `requestGameOver` | `(player, delay)` | 等待有限且非负的动画延时后切换场景，不施加伤害或 Effect |

战斗与移动危险必须先取得 `GameplayScene`，才会改变状态。Teleporter 请求无法执行或被拒绝时，移动与记录保持不变。Game Over 采用剩余动画中最长的时长。重复请求会被忽略；替换场景、地图或玩家都会取消待执行的导航。

## GeneralDataGraphAbility

模块：`Source.Gameplay.GeneralDataGraphAbility`

直接基类：`GlobalCore.GameplayAbility`

| 字段 | 类型 |
|---|---|
| `generalType` | `string` |
| `memberID` | `string` |
| `graphEvent` | `string` |

| 成员 | 签名 | 行为 |
|---|---|---|
| `init` | `(generalType, memberID, graphEvent, triggerTags?)` | 创建 ability ID `GeneralData.<type>.<member>.<event>` |
| `calculate` | `(abilitySystem, eventData) -> GameplayAbilityResult` | 返回包含 graph data 的 `ExecutableGraph`，或表示成功的 `NoGraph` |
| `activate` | `(abilitySystem, eventData) -> GameplayAbilityResult` | 构建并同步执行所选 event，返回 `GraphExecuted` 或 `NoGraph` |

graph 的 parent 与 `eventData` 参数是同一个 `GameplayEventData`。它的数据会接收上述三个字段；对 State 图还会接收 `activeEffect`。执行通过 `Engine.BPBase.ExecuteGraph` 完成。通用数据 graph 没有 Actor 派发与继承，不能 latent，错误会直接传播。

## Effects

模块：`Source.Gameplay.Effects`

| 函数 | 签名 | 结果 |
|---|---|---|
| `CreateInstantModifierSpec` | `(effectID, attribute, operation, magnitude, eventData?) -> GameplayEffectSpec` | 创建但不应用单个 Instant modifier spec |
| `ApplyInstantModifier` | `(target, effectID, attribute, operation, magnitude, eventData?)` | 应用单个 Instant modifier |
| `CreateEquipmentEffect` | `(equipID, slot, attrPlus) -> GameplayEffect` | 创建 Infinite 装备 effect 与 `Equipment.Slot.<slot>` tag |
| `CreateStateEffect` | `(stateID) -> GameplayEffect` | 创建 Infinite state effect、tag 与所授予的 state ability |
| `CreateStateSpec` | `(stateID, stacks?, eventData?) -> GameplayEffectSpec` | 创建但不应用 stack 数为正的 State spec |
| `ApplyState` | `(target, stateID, stacks?, eventData?) -> integer` | 应用或叠加 State effect，并返回其 handle |
| `RemoveState` | `(target, stateID) -> bool` | 删除整个 State effect |
| `ReduceState` | `(target, stateID, stacks?) -> bool` | 在支持时只删除请求的 stack 数 |
| `ClearStates` | `(target)` | 删除 ID 以 `State.` 开头的全部 effect |
| `GetStateStacks` | `(target) -> table<string, integer>` | 返回 State ID 到 stack 数的映射 |
| `GetStateIDs` | `(target) -> string[]` | 按稳定顺序返回生效中的 State ID |
| `FindActiveEffectHandle` | `(abilitySystem, effectID) -> integer?` | 返回首个匹配的生效 handle |

装备按稳定的属性顺序使用 `attrPlus`。Weak 每层应用 `-1 ATK` 与 `-1 DEF`，最低为 `0`。Poisoned 授予其伤害 ability。State 可以授予 `onWalk` 与 `onHookTriggered` graph ability。

## MotaBattleAbility

模块：`Source.Gameplay.MotaBattleAbility`

直接基类：`GlobalCore.GameplayAbility`

Ability ID：`Ability.Combat.MotaBattle`

通过 `require` 直接加载这些常量模块；`MotaBattleAbility` 在 `result.code` 中返回对应的标量值。

| 枚举 | 成员 |
|---|---|
| `Enums.BattleResultCode` | `WIN = 1`、`CANNOT_DAMAGE = 2`、`LETHAL_COUNTER_DAMAGE = 3` |
| `Enums.CriticalResultCode` | `VALUE = 1`、`NOT_NEEDED = 2`、`UNKNOWN = 3` |

| 成员 | 签名 | 行为 |
|---|---|---|
| `calculate` | `(abilitySystem, eventData) -> GameplayAbilityResult` | 不产生任何变更，只生成 `Enums.BattleResultCode` code 与预校验过的伤害、Game Over spec |
| `activate` | `(abilitySystem, eventData) -> GameplayAbilityResult` | 先计算，并在 `eventData.payload.commit` 为 true 时提交 |
| `CalculateDamagePerRound` | `(attacker, defender) -> damage, details` | 运行攻击、防御、伤害、命中次数与受到伤害的事件管线 |
| `CommitResult` | `(result)` | 恰好应用一次已预校验的反击伤害 Effect |
| `CalculateCriticalValue` | `(enemy, player) -> GameplayAbilityResult` | 返回一个 `Enums.CriticalResultCode` code |

持有者是敌人，`eventData.target` 是玩家。计算只做校验，不应用反击伤害或 Game Over Spec。战斗结果的 data 包含每回合伤害、反击回合数、Vampire 回复、先攻（First）与固定伤害（FixDmg）、双方 Battler、预先准备的 Spec 以及 `committed`。`CommitResult` 只提交一次合计伤害。Enemy 在动画结束后处理 Game Over。暴击值计算会纳入 Vampire 回复，First 的回合阈值保持原值，但会忽略 FixDmg 表达式。

## GameplayConstants

模块：`Source.Configs.GameplayConstants`

本模块统一声明共享 Gameplay 事件标签与标识前缀。事件派发方和 ability 订阅方使用同一组字段。

| 字段 | 值 |
|---|---|
| `MOVEMENT_HAZARD_TAG` | `"Gameplay.Movement.Hazard"` |
| `BATTLE_RULES_EVENT` | `"Event.Combat.ResolveBattleRules"` |
| `MOVEMENT_STEP_EVENT` | `"Event.Movement.Step"` |
| `MOVEMENT_QUERY_HAZARD_EVENT` | `"Event.Movement.QueryHazard"` |
| `MOVEMENT_QUERY_HAZARD_BOUNDS_EVENT` | `"Event.Movement.QueryHazardBounds"` |
| `COMBAT_RESOLVE_ATTACK_EVENT` | `"Event.Combat.ResolveAttack"` |
| `COMBAT_RESOLVE_DEFENSE_EVENT` | `"Event.Combat.ResolveDefense"` |
| `COMBAT_RESOLVE_HIT_COUNT_EVENT` | `"Event.Combat.ResolveHitCount"` |
| `COMBAT_RESOLVE_DAMAGE_EVENT` | `"Event.Combat.ResolveDamage"` |
| `COMBAT_RESOLVE_INCOMING_DAMAGE_EVENT` | `"Event.Combat.ResolveIncomingDamage"` |
| `EQUIPMENT_EQUIP_EVENT` | `"Event.Equipment.Equip"` |
| `SPECIAL_PREFIX` | `"Special."` |
| `STATE_PREFIX` | `"State."` |
| `EQUIPMENT_PREFIX` | `"Equipment."` |

## SpecialAbilities

模块：`Source.Gameplay.SpecialAbilities`

具体 ability 类也可以从 `Source.Gameplay.SpecialAbilities.<ClassName>` 加载，例如 `require("Source.Gameplay.SpecialAbilities.MultiHitAbility").new(magnitude)`。

| 成员 | 签名/值 | 行为 |
|---|---|---|
| `CreateEffect` | `(specialID, magnitude) -> GameplayEffect` | 创建一个 Infinite `Special.<id>` effect、tag 与 ability |
| `CreatePoisonedAbility` | `() -> GameplayAbility` | 创建 State Poisoned 的受到伤害 ability |
| `GetMagnitude` | `(abilitySystem, specialID) -> any` | 读取 active special effect 上保存的 magnitude |

Compete、Hard、Magic、MultiHit 与 Poisoned 使用各自的战斗结算事件。`GameplayConstants.BATTLE_RULES_EVENT` 汇集 Vampire 回复、First 伤害与 FixDmg。数值形式的 FixDmg 直接生效；字符串形式会在求值之前代入玩家与敌人的当前数值属性。Domain、Flank 与 Blockade 处理 `Event.Movement.QueryHazard`，并授予 `Gameplay.Movement.Hazard`。其他 ID 授予可查询的 passive 标签。

## MovementSpecials 与移动危险

模块：`Source.Utils.MovementSpecials`

| 函数 | 签名 | 行为 |
|---|---|---|
| `CreatePreviewContext` | `(enemies, player) -> PreviewContext` | 校验危险覆盖声明，为未变化的玩家/敌人快照缓存战斗伤害和覆盖半径 |
| `Preview` | `(enemies, player, playerPosition, ignoredEnemies?, previewContext?) -> GameplayAbilityResult` | 解析移动危险伤害与贡献来源 |
| `Commit` | `(player, pathPositions) -> GameplayAbilityResult` | 对每个经过的格子复用 `Preview`，应用总伤害并执行 Blockade 后退 |
| `NotifyPlayerMovementFinished` | `(player, pathPositions?) -> GameplayAbilityResult` | 提交已完成的移动路径；缺省时提交当前格 |

`Preview` 负责移动危险的几何计算。它派发 `Event.Movement.QueryHazard`，解析 Flank 与被忽略的敌人，并返回有序的来源与总伤害。Domain 在曼哈顿距离 `<= max(1, magnitude)` 时触发，包含范围边界，并通过 `MotaBattleAbility.CalculateDamagePerRound` 计算伤害，保留 Magic 等战斗特性。被忽略的 Flank 敌人只参与几何计算，不贡献伤害。

传给 `CreatePreviewContext` 的应是具有移动危险能力的敌人，例如 `CollectEnemies` 的返回结果。每个订阅 `Event.Movement.QueryHazard` 的已授予能力，都必须同时处理 `Event.Movement.QueryHazardBounds`。范围事件以敌人为 `instigator`、玩家为 `target`，payload 为空。每个处理函数必须成功返回 `GameplayAbilityResult.Success("MovementHazardBounds", { radius = n })`，其中 `n` 为非负整数。从 `(enemy.x - n, enemy.y - n)` 到 `(enemy.x + n, enemy.y + n)` 的闭合正方形必须覆盖该能力可能贡献伤害的全部格子。Domain 声明 `max(1, magnitude)`，Blockade 和 Flank 声明 `1`。缺少订阅、查询被拒绝或结果格式错误都会报错，即使同一敌人的其他能力已提供有效范围也不能省略。范围查询和伤害预览均不得修改玩法状态。

`PreviewContext` 包含 `player`、`damageByEnemy` 和 `radiusByEnemy`；覆盖半径取同一敌人各能力声明中的最大值。敌人集合、可见性、位置或玩家/敌人的能力状态变化后，必须重新创建上下文。危险格网按 `Gameplay.Movement.Hazard` 筛选并按需生成，将覆盖范围裁剪到请求区域，仅对候选格及相关敌人调用 `Preview`，同时保留伤害来源顺序和格子按行排列的顺序。覆盖范围只是保守边界，准确伤害仍由逐格事件与 Flank 配对决定。区域变化时，未变化的重叠格会复用已有结果，包括零伤害格。

## 生成的 GeneralDataTypes

模块：`Source.Configs.GeneralDataTypes`

通用数据生成器拥有该模块及其 stub。`Types` 与其中的命名字段导出每一个生成的 `<TypeName>AttributeSet`。类型键来自 `Enums.GeneralDataKey`，成员键来自对应的 `Enums.GeneralData.<TypeName>` 模块。

`Create(typeName, memberID, memberData) -> AttributeSet` 深拷贝结构定义字段并设置 `ID`。以 ID 为起点时，改用 `Source.Data.CreateGeneralAttributeSet(typeName, memberID)`。

## 相关页面

- [GlobalCore Gameplay API](<../全局与 Core 模块/Global APIs/Gameplay.md>)
- [Source.Battler](<Battler.md>)
- [Source.MapActors.Enemy](<Enemy.md>)
