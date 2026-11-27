# GlobalCore Gameplay API

`require("GlobalCore")` 暴露原生能力系统，涵盖 `AttributeSet`、`AbilitySystemComponent` 以及能力、结果、事件、效果与运行时 spec。以下实例方法表格省略隐式 Lua `self` 接收者；蓝图节点将其作为带类型的首个 `self` 输入引脚暴露，未连接时默认指向图表所属对象（当目标 `AttributeSet`、`AbilitySystemComponent` 或 `GameplayAbility` 并非图表所属对象时，须显式连接对应实例）。

## AttributeSet

`AttributeSet` 子类在 `ATTRIBUTE_NAMES` 中声明有序属性名列表，并在 `SCHEMA` 中为每项定义 `{ type, default }` 条目，`ID` 用于标识对应的通用数据成员。缺失字段取架构默认值，显式存储的 `nil` 分支则覆盖默认值。已存储的通用数据通过 `Source.Configs.GeneralDataTypes.Create` 加载。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `init` | `values?: table` | — | 初始化所有已声明属性与 `ID`。 |
| `initialize` | `values: table` | — | 校验并复制运行时值。 |
| `initializeStored` | `values: table` | — | 按类型架构还原存储值后再复制到属性中。 |
| `getAttributeNames` | — | `string[]` | 返回有序属性名列表的副本。 |
| `getAttributeSchema` | `name: string` | `AttributeSchema?` | 返回属性的 `type` 架构与 `default` 默认值。 |

只有 `int` 与 `float` 字段维护基础值（Base）和当前值（Current）并接受修饰项。所有数值均须为有限数：`int` 要求 Lua 整数，`float` 接受任意有限数值。其他字段按声明类型直接存储。

## AbilitySystemComponent

`AbilitySystemComponent.new(owner, attributeSet)` 创建拥有属性、已授予能力、生效中效果与计数玩法标签的组件。

### 属性

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `getOwner` | — | `any` | 返回所属玩法对象。 |
| `getAttributeSet` | — | `GlobalCore.AttributeSet` | 返回当前使用的属性集。 |
| `getNumericAttribute` | `name: string` | `number` | 返回应用生效中修饰项后的当前值。 |
| `getNumericAttributeBase` | `name: string` | `number` | 返回未经修饰的基础值。 |
| `setNumericAttributeBase` | `name: string`, `value: number` | — | 更新单个基础值，并重新计算所有数值属性的当前值。 |
| `setNumericAttributeBases` | `values: table<string, number>` | — | 批量更新多个基础值，再统一重新计算所有数值属性。 |
| `getNumericAttributeBases` | — | `table<string, number>` | 返回适用于持久化的基础值副本。 |
| `addAttributeChangeListener` | `name: string`, `callback: function`, `params?: any[]` | — | 相关当前值或基础值变更后调用 `callback(oldValue, newValue, change, ...)`。 |
| `setNumericAttributeConstraint` | `name: string`, `callback?: function` | — | 安装或移除（传入 `nil`）最终当前值约束并立即重算。 |
| `getRevision` | — | `integer` | 返回组件版本号；属性、能力或效果变更时递增。属性监听回调看到的是提交后的版本号。 |

基础值写入方法会在提交前校验完整的候选 Base 与 Current 状态。属性名未知、数值非有限或不符合架构定义时直接失败，不会留下部分修改。直接对数值字段赋值同样视作基础值写入。

监听回调的 `oldValue` 与 `newValue` 始终为当前值，随后传入 `AttributeChange` 上下文与绑定的附加 `params`：

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `source` | `"Base" \| "Effect" \| "Constraint"` | — | 触发本次重算的操作：`GameplayAttributeChangeSource.BASE`（`"Base"`）、`EFFECT`（`"Effect"`）或 `CONSTRAINT`（`"Constraint"`）。 |
| `force` | `boolean` | — | 为 `true` 时，即使可见当前值未变化，基础值写入仍会发送通知。 |
| `oldBase` | `number \| Class.MissingValue \| nil` | — | 来源为 `"Base"` 时的旧基础值；其他来源为 `nil`。 |
| `newBase` | `number \| nil` | — | 来源为 `"Base"` 时的新基础值；其他来源为 `nil`。 |

来源常量是 `GlobalCore.GameplayAttributeChangeSource` 上的静态只读字段，`change.source` 始终为字符串。

- **通知规则**：批量写入仅通知最终状态。基础值写入即便未改变当前值也会触发通知，效果与约束则仅在当前值变化时触发通知。
- **约束函数**：修饰项聚合完成后，约束函数接收 `(value, abilitySystem, resolvedValues)`，必须恰好返回一个符合架构的有限数值，且绝不能写入基础值。传入 `nil` 可移除约束。解析按 `ATTRIBUTE_NAMES` 顺序执行且 HP 排在最后；`resolvedValues` 包含本轮已完成解析的字段。

### 能力与事件

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `giveAbility` | `ability: GlobalCore.GameplayAbility`, `sourceKey?: any` | `GlobalCore.GameplayAbilitySpec` | 授予能力并记录其来源。 |
| `removeAbilitiesBySource` | `sourceKey: any` | — | 移除由该精确来源授予的所有能力。 |
| `tryActivateAbility` | `abilityID: string`, `eventData?: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | 激活该 ID 下优先级最高且满足激活条件的能力。 |
| `getGameplayEventAbilities` | `eventTag: string` | `GlobalCore.GameplayAbilitySpec[]` | 按事件执行顺序返回已订阅的已授予 spec，不调用激活检查或能力本身。 |
| `handleGameplayEvent` | `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult[]` | 执行触发标签与事件匹配的所有能力。 |

能力按 `priority` 降序执行，同优先级按授予顺序执行，并采用层级标签匹配。直接激活时，若不存在对应 ID 则返回 `AbilityNotFound`，若所有匹配能力均未通过激活条件则返回 `AbilityNotActivated`。

`getGameplayEventAbilities` 与 `handleGameplayEvent` 遵循相同的标签匹配与排序规则，涵盖直接授予及由效果授予的能力。事件标签不能为空，无匹配项时返回空数组。修改返回数组不会授予或移除能力；调用该方法不会触发 `canActivate` 或 `activate`，也不会递增组件版本号。

### 效果与标签

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `applyGameplayEffectSpec` | `spec: GlobalCore.GameplayEffectSpec` | `integer?` | 应用 `Instant` 效果（返回 `nil`），或返回 `Infinite` 效果的句柄。 |
| `validateGameplayEffectSpec` | `spec: GlobalCore.GameplayEffectSpec` | `bool` | 校验并预演 spec，不改变能力系统状态。 |
| `removeActiveGameplayEffect` | `handle: integer`, `stacks?: integer` | `bool` | 移除指定层数或整个生效中的效果。 |
| `getActiveEffectStacks` | `effectID: string` | `integer` | 汇总该 ID 下所有生效中效果的层数。 |
| `getActiveGameplayEffects` | — | `GlobalCore.ActiveGameplayEffect[]` | 按应用顺序返回生效中的效果。 |
| `hasMatchingGameplayTag` | `tag: string` | `bool` | 检测是否拥有完全匹配或其子级的标签。 |

效果应用与修饰项解析遵循以下规则：

- **Instant 与 Infinite**：`Instant` 修饰项按顺序更新基础值，逐项立即应用各自的 `minimum`，且不返回句柄。`Infinite` 修饰项持续重算当前值直至移除，移除 `Infinite` 效果时会同时移除其授予的标签与能力。
- **聚合与叠层**：`Add` 修饰量相加，`Multiply` 修饰量相乘；最后生效的 `Override` 胜出，再由匹配修饰项中最高的 `minimum` 钳制当前值。`Aggregate` 叠层对 `Add` 按线性缩放、对 `Multiply` 按指数缩放。`None` 按非空 ID 与来源组合去重。
- **校验约束**：修饰量、下限、基础值与当前值都必须为有限数且符合架构类型。`Instant` 效果不能授予标签或能力。`None` 只允许单层，采用 `Aggregate` 的 `Infinite` 效果要求非空 ID。

> **重要**：应用、叠加与移除效果都会先预演完整结果再提交，`validateGameplayEffectSpec` 仅执行预演而不修改状态。修饰量函数与约束函数在预演阶段运行，不得产生外部副作用。提交后由监听回调抛出的错误不会回滚已完成的变更。

## GameplayAbility

`GameplayAbility` 定义能力的激活策略、标签与执行钩子。

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `id` | `string` | `""` | 授予的能力要求非空 ID。 |
| `priority` | `integer` | `0` | 优先级数值越高越先执行。 |
| `abilityTags` | `string[]` | `{}` | — |
| `requiredTags` | `string[]` | `{}` | — |
| `blockedTags` | `string[]` | `{}` | — |
| `triggerTags` | `string[]` | `{}` | — |

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `canActivate` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | 检查必需标签与阻止标签。 |
| `calculate` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | 默认仅计算结果，不提交修改。 |
| `activate` | `abilitySystem: GlobalCore.AbilitySystemComponent`, `eventData: GlobalCore.GameplayEventData` | `GlobalCore.GameplayAbilityResult` | 默认委托给 `calculate`；子类可重写以提交实际改动。 |

所有重写的方法都必须返回 `GameplayAbilityResult`。

## GameplayAbilityResult

`GameplayAbilityResult` 携带布尔值 `ok`、字符串或整数 `code`（默认 `""`）以及任意类型的 `data`。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `Success` | `code?: string \| integer`, `data?: any` | `GlobalCore.GameplayAbilityResult` | 静态方法。创建成功结果，`code` 默认为 `"Success"`。 |
| `Failure` | `code: string \| integer`, `data?: any` | `GlobalCore.GameplayAbilityResult` | 静态方法。创建失败结果，`code` 必须为非空字符串或非零整数。 |

内建结果码是 `GlobalCore.GameplayAbilityResult` 的静态只读字段。自定义能力也可返回自有字符串或整数结果码。

| 常量 | 字符串值 | 含义 |
|---|---|---|
| `CODE_SUCCESS` | `"Success"` | 默认成功结果。 |
| `CODE_ABILITY_NOT_FOUND` | `"AbilityNotFound"` | 已授予能力中不存在请求的 ID。 |
| `CODE_ABILITY_NOT_ACTIVATED` | `"AbilityNotActivated"` | 没有匹配能力通过激活条件检查。 |
| `CODE_MISSING_REQUIRED_TAG` | `"MissingRequiredTag"` | 缺少必需标签。 |
| `CODE_BLOCKED_BY_TAG` | `"BlockedByTag"` | 存在阻止激活的标签。 |

## GameplayEventData

`GameplayEventData.new(instigator?, target?, eventTag?, payload?)` 携带单次能力激活或事件分发的上下文。

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `instigator` | `any` | `nil` | — |
| `target` | `any` | `nil` | — |
| `eventTag` | `string` | `""` | `handleGameplayEvent` 要求非空；直接激活能力时可选。 |
| `payload` | `table<string, any>` | `{}` | 处理函数可修改 `payload`。 |

## GameplayEffect 与 GameplayEffectSpec

`GameplayEffect` 定义属性修饰项及授予内容，`GameplayEffectSpec` 将效果与事件数据、层数和来源身份绑定。

**`GameplayEffect` 字段**：

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `id` | `string` | `""` | 采用 `Aggregate` 的 `Infinite` 效果要求非空。 |
| `durationPolicy` | `string` | `GameplayEffect.DURATION_INSTANT` | `GameplayEffect.DURATION_INSTANT`（`"Instant"`）或 `DURATION_INFINITE`（`"Infinite"`）。 |
| `stackingPolicy` | `string` | `GameplayEffect.STACKING_NONE` | `GameplayEffect.STACKING_NONE`（`"None"`）或 `STACKING_AGGREGATE`（`"Aggregate"`）。 |
| `modifiers` | `GlobalCore.GameplayModifier[]` | `{}` | — |
| `grantedTags` | `string[]` | `{}` | 仅限 `Infinite` 效果。 |
| `grantedAbilities` | `GlobalCore.GameplayAbility[]` | `{}` | 仅限 `Infinite` 效果。 |
| `data` | `table<string, any>` | `{}` | 对能力系统保持不透明。 |

**`GameplayModifier` 字段**：

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `attribute` | `string` | — | 目标数值属性名。 |
| `operation` | `string` | — | `GameplayModifier.OPERATION_ADD`（`"Add"`）、`OPERATION_MULTIPLY`（`"Multiply"`）或 `OPERATION_OVERRIDE`（`"Override"`）。 |
| `magnitude` | `number \| function` | — | 校验时必填：有限数值，或 `fun(spec, stacks)` 且恰好返回一个有限数值的函数。 |
| `minimum` | `number?` | `nil` | 可选有限下限。 |

策略与运算常量均是所属 `GlobalCore` 类的静态只读属性，保存上表对应的字符串值。

`GameplayEffectSpec.new(effect, eventData?, stacks?, sourceKey?)` 要求 `stacks` 为正整数。效果 ID 与 Lua 相等的 `sourceKey` 共同标识一个 `Infinite` 效果以执行去重或叠加；调用方须确保匹配组合使用一致的效果定义，`Aggregate` 会保留首个生效的 spec。

## 运行时记录

- `GameplayAbilitySpec` 保存 `ability`、`sourceKey` 与 `grantOrder`。
- `ActiveGameplayEffect` 保存 `handle`、`spec`、`stacks`、`applicationOrder` 以及已授予能力的 spec。

## 支持范围

- **支持**：`Instant` 与 `Infinite` 效果，`Add`、`Multiply` 与 `Override` 修饰项，聚合叠层，层级标签与同步能力。
- **不支持**：计时器、周期效果、预测、复制、异步任务与内建持久化。
