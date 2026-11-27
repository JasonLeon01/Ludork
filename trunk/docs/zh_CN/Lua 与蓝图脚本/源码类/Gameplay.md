# Source Gameplay API

`Source.Gameplay` 提供基于 `GlobalCore` 构建的默认玩法 Scene 契约、能力、效果、战斗结算规则、移动危险辅助函数以及生成的通用数据 Attribute Set。

## GameplayScene

**模块**：`Source.Gameplay.GameplayScene`  
**继承**：`GlobalCore.SceneBase`

`SceneMap` 实现了该契约，Actor 可通过所属地图访问它。实例方法省略隐式接收者 `self`（`Source.Gameplay.GameplayScene`）。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `getGameMap` | — | `Global.GameMap` | 返回场景当前已加载的地图。 |
| `getGameInstance` | — | `Source.GameInstance` | 返回持有变量与持久化状态的游戏实例。 |
| `recordAddedActor` | `actor: Engine.Actor` | — | 在持久化状态中记录新增的 Actor。 |
| `recordDestroyedActor` | `actor: Engine.Actor` | — | 在持久化状态中记录已销毁的 Actor。 |
| `recordActorPosition` | `actor: Engine.Actor`, `position` | — | 记录 Actor 的地图位置。 |
| `requestFloorStep` | `teleporter`, `step: int` | `bool` | 受理一次相邻楼层移动请求；`step` 必须为 `1` 或 `-1`。抵达时选择最近的、可见且未销毁的反方向楼梯。 |
| `requestMapTransfer` | `teleporter`, `mapPath: string`, `position: sf.Vector2i`, `record?: bool = true` | `bool` | 受理一次前往指定地图格子的传送，不搜索楼梯；可选将出发与抵达端点分别记录在各自地图上。 |
| `requestGameOver` | `player`, `delay: number` | — | 等待有限且非负的动画延时后跳转至 Game Over，期间不施加伤害或效果。 |

战斗与移动危险必须先取得 `GameplayScene`，才会变更游戏状态。当 Teleporter 请求不可用或被拒绝时，移动状态与持久化记录均保持不变。Game Over 等待时长取剩余动画中的最大值；重复请求会被忽略，而替换场景、地图或玩家均会取消待执行的跳转。

## GeneralDataGraphAbility

**模块**：`Source.Gameplay.GeneralDataGraphAbility`  
**继承**：`GlobalCore.GameplayAbility`

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `generalType` | `string` | — | 通用数据类型键。 |
| `memberID` | `string` | — | 通用数据成员标识。 |
| `graphEvent` | `string` | — | 成员图事件名。 |

实例方法省略隐式接收者 `self`（`Source.Gameplay.GeneralDataGraphAbility`）。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `init` | `generalType: string`, `memberID: string`, `graphEvent: string`, `triggerTags?` | — | 创建能力 ID `GeneralData.<type>.<member>.<event>`。 |
| `calculate` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | 返回携带图数据的 `ExecutableGraph`，或表示成功的 `NoGraph`。 |
| `activate` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | 构建并同步执行所选事件，返回 `GraphExecuted` 或 `NoGraph`。 |

成员图的父对象与 `eventData` 参数为同一个 `GameplayEventData` 实例。其 `payload` 包含上述三个字段；对于 State 图，还会附带 `activeEffect`。成员图通过 `Engine.BPBase.ExecuteGraph` 执行，不具备 Actor 派发与继承机制，不支持潜伏执行，运行错误会直接向上传播。

## Effects

**模块**：`Source.Gameplay.Effects`

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `CreateInstantModifierSpec` | `effectID: string`, `attribute: string`, `operation`, `magnitude: number`, `eventData?: GlobalCore.GameplayEventData` | `GlobalCore.GameplayEffectSpec` | 创建但不应用单个即时（Instant）属性修饰规格（Spec）。 |
| `ApplyInstantModifier` | `target`, `effectID: string`, `attribute: string`, `operation`, `magnitude: number`, `eventData?: GlobalCore.GameplayEventData` | — | 应用单个即时属性修饰效果。 |
| `CreateEquipmentEffect` | `equipID: string`, `slot: string`, `attrPlus` | `GlobalCore.GameplayEffect` | 创建无限持续（Infinite）装备效果与 `Equipment.Slot.<slot>` 标签。 |
| `CreateStateEffect` | `stateID: string` | `GlobalCore.GameplayEffect` | 创建无限持续状态（State）效果、对应标签及授予的状态能力。 |
| `CreateStateSpec` | `stateID: string`, `stacks?: int`, `eventData?: GlobalCore.GameplayEventData` | `GlobalCore.GameplayEffectSpec` | 创建但不应用层数为正数的 State 规格。 |
| `ApplyState` | `target`, `stateID: string`, `stacks?: int`, `eventData?: GlobalCore.GameplayEventData` | `int` | 应用或叠加 State 效果并返回其句柄。 |
| `RemoveState` | `target`, `stateID: string` | `bool` | 移除完整的 State 效果。 |
| `ReduceState` | `target`, `stateID: string`, `stacks?: int` | `bool` | 在支持层数缩减时仅移除指定层数。 |
| `ClearStates` | `target` | — | 移除 ID 以 `State.` 开头的所有效果。 |
| `GetStateStacks` | `target` | `table<string, int>` | 返回 State ID 到当前层数的映射表。 |
| `GetStateIDs` | `target` | `string[]` | 按稳定顺序返回生效中的 State ID 列表。 |
| `FindActiveEffectHandle` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `effectID: string` | `int?` | 返回首个匹配的生效效果句柄。 |

装备按稳定的属性顺序应用 `attrPlus`。Weak（虚弱）每层施加 `-1 ATK` 与 `-1 DEF`，最低降至 `0`。Poisoned（中毒）会授予对应的持续伤害能力。状态可授予 `onWalk` 与 `onHookTriggered` 成员图能力。

## MotaBattleAbility

**模块**：`Source.Gameplay.MotaBattleAbility`  
**继承**：`GlobalCore.GameplayAbility`  
**能力 ID**：`Ability.Combat.MotaBattle`

以下常量模块可直接通过 `require` 加载，`MotaBattleAbility` 在 `result.code` 中返回对应的标量值。

| 枚举 | 成员 |
|---|---|
| `Enums.BattleResultCode` | `WIN = 1`、`CANNOT_DAMAGE = 2`、`LETHAL_COUNTER_DAMAGE = 3` |
| `Enums.CriticalResultCode` | `VALUE = 1`、`NOT_NEEDED = 2`、`UNKNOWN = 3` |

实例方法省略隐式接收者 `self`（`Source.Gameplay.MotaBattleAbility`）。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `calculate` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | 不修改任何状态，生成 `Enums.BattleResultCode` 结果码与预校验后的伤害及 Game Over 规格。 |
| `activate` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | 执行战斗计算；当 `eventData.payload.commit` 为 `true` 时提交结算结果。 |
| `CalculateDamagePerRound` | `attacker`, `defender` | `damage: number`, `details: table` | 静态方法。运行攻击、防御、伤害、命中次数与受击伤害事件管线。 |
| `CommitResult` | `result: GlobalCore.GameplayAbilityResult` | — | 静态方法。恰好应用一次预校验后的反击伤害效果。 |
| `CalculateCriticalValue` | `enemy`, `player` | `GlobalCore.GameplayAbilityResult` | 静态方法。返回 `Enums.CriticalResultCode` 结果码。 |

该能力的持有者为敌人，`eventData.target` 为玩家。计算阶段仅执行校验，不应用反击伤害或 Game Over 规格。返回的战斗结果数据包含每回合伤害、反击回合数、Vampire 回复量、先攻（First）与固定伤害（FixDmg）、双方 Battler、预备规格以及 `committed` 标记。`CommitResult` 仅提交一次合计伤害，Enemy 会在战斗动画结束后处理 Game Over。临界值（Critical）计算计入 Vampire 回复，无论是否具备 First 均沿用同一回合数阈值，但忽略 FixDmg 表达式。

战斗结算各中间阶段返回 `Enums.CombatResolutionCode` 中的字符串常量：`ATTACK = "AttackResolved"`、`DEFENSE = "DefenseResolved"`、`HIT_COUNT = "HitCountResolved"`、`DAMAGE = "DamageResolved"`、`INCOMING_DAMAGE = "IncomingDamageResolved"` 和 `BATTLE_RULES = "BattleRulesResolved"`。这些中间结果独立于最终的 `Enums.BattleResultCode` 胜负结果。

## GameplayConstants

**模块**：`Source.Configs.GameplayConstants`

本模块统一声明共享的玩法事件标签与标识前缀，供事件发布方与能力订阅方共同使用。

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

**模块**：`Source.Gameplay.SpecialAbilities`

具体能力类也可通过 `Source.Gameplay.SpecialAbilities.<ClassName>` 单独加载，例如 `require("Source.Gameplay.SpecialAbilities.MultiHitAbility").new(magnitude)`。

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `CreateEffect` | `specialID: string`, `magnitude: any` | `GlobalCore.GameplayEffect` | 创建单个无限持续 `Special.<id>` 效果、对应标签及能力。 |
| `CreatePoisonedAbility` | — | `GlobalCore.GameplayAbility` | 创建 Poisoned 状态的受击伤害能力。 |
| `GetMagnitude` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `specialID: string` | `any` | 读取生效中特殊效果上保存的 `magnitude` 值。 |

Compete、Hard、Magic、MultiHit 与 Poisoned 分别通过各自的战斗结算事件生效。`GameplayConstants.BATTLE_RULES_EVENT` 负责汇总 Vampire 回复、First 伤害与 FixDmg：数值形式的 FixDmg 直接生效，字符串形式则在求值前代入玩家与敌人当前的数值属性。Domain、Flank 与 Blockade 处理 `Event.Movement.QueryHazard` 并授予 `Gameplay.Movement.Hazard` 标签；其余 ID 则授予可供查询的被动标签。

## MovementSpecials 与移动危险

**模块**：`Source.Utils.MovementSpecials`

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `CreatePreviewContext` | `enemies`, `player` | `PreviewContext` | 校验危险覆盖范围，并为未变化的玩家/敌人快照缓存战斗伤害与覆盖半径。 |
| `Preview` | `enemies`, `player`, `playerPosition: sf.Vector2i`, `ignoredEnemies?`, `previewContext?` | `GlobalCore.GameplayAbilityResult` | 解析移动危险伤害及其贡献来源。 |
| `Commit` | `player`, `pathPositions: sf.Vector2i[]` | `GlobalCore.GameplayAbilityResult` | 对路径上经过的每个格子复用 `Preview`，应用总伤害并执行 Blockade 阻拦后退。 |
| `NotifyPlayerMovementFinished` | `player`, `pathPositions?: sf.Vector2i[]` | `GlobalCore.GameplayAbilityResult` | 提交已完成的移动路径；省略路径参数时提交当前所在格。 |

可通过 `local MovementResultCode = require("Enums.MovementResultCode")` 加载移动结果常量：`HAZARD_BOUNDS = "MovementHazardBounds"`、`HAZARD = "MovementHazard"`、`NO_HAZARD = "NoMovementHazard"` 和 `HAZARD_COMMITTED = "MovementHazardCommitted"`。

`Preview` 负责移动危险的几何判定。它会派发 `Event.Movement.QueryHazard`，处理 Flank（夹击）与被忽略的敌人，并返回有序的伤害来源列表与总伤害。Domain（领域）在曼哈顿距离 `<= max(1, magnitude)` 时生效（包含边界），并调用 `MotaBattleAbility.CalculateDamagePerRound` 计算伤害（计入 Magic 等战斗特性）。被忽略的 Flank 敌人仍参与几何站位判定，但不贡献伤害。

传入 `CreatePreviewContext` 的应为具备移动危险能力的敌人列表（例如 `CollectEnemies` 的返回结果）。所有订阅 `Event.Movement.QueryHazard` 的已授予能力都必须同时处理 `Event.Movement.QueryHazardBounds`。范围查询事件以敌人为 `instigator`、玩家为 `target`，`payload` 为空。

每个范围处理函数必须成功返回 `GameplayAbilityResult.Success(MovementResultCode.HAZARD_BOUNDS, { radius = n })`，其中 `n` 为非负整数。从 `(enemy.x - n, enemy.y - n)` 到 `(enemy.x + n, enemy.y + n)` 的闭区间正方形必须完整包含该能力可能造成伤害的所有格子：Domain 声明 `max(1, magnitude)`，Blockade 与 Flank 声明 `1`。若缺少订阅、查询被拒绝或返回格式不合法，即使同一敌人的其他能力提供了有效范围，系统仍会报告错误。范围查询与伤害预览均不得修改玩法状态。

`PreviewContext` 包含 `player`、`damageByEnemy` 与 `radiusByEnemy`；其中半径取同一敌人所有能力声明中的最大值。当敌人集合、可见性、位置或玩家/敌人的能力状态发生变化时，必须重新创建上下文。危险网格按 `Gameplay.Movement.Hazard` 筛选敌人并按需生成：它会将覆盖范围裁剪至请求区域，仅对候选格及相关敌人调用 `Preview`，并保持来源顺序与行优先的条目顺序。覆盖范围仅作为保守边界，精确伤害仍由逐格事件与 Flank 判定决定。当查询区域移动或缩放时，未变化的重叠格（包括零伤害格）会直接复用缓存结果。

## 生成的 GeneralDataTypes

**模块**：`Source.Configs.GeneralDataTypes`

通用数据生成器负责维护本模块及其声明存根（Stub）。`Types` 与各命名字段对外暴露所有生成的 `<TypeName>AttributeSet`：类型键来自 `Enums.GeneralDataKey`，成员键来自对应的 `Enums.GeneralData.<TypeName>` 模块。

`Create(typeName, memberID, memberData)` 返回 `GlobalCore.AttributeSet`，会对结构定义字段执行深拷贝并写入 `ID`。若仅持有 ID，请改用 `Source.Data.CreateGeneralAttributeSet(typeName, memberID)`。
