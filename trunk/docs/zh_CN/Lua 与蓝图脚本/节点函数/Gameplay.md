# GlobalFunctions.Gameplay

`GlobalFunctions.Gameplay` 提供用于读取通用数据玩法事件上下文，以及修改战斗单位属性、状态（State）、效果与派发玩法事件的蓝图节点。

**元数据来源**：`Scripts/GlobalFunctions/Gameplay_meta.lua`

## Gameplay

### 函数

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `GetContext` | — | `context: GlobalCore.GameplayEventData` | 纯函数（Pure）。返回当前玩法事件上下文。 |
| `GetSource` | — | `source: any` | 纯函数（Pure）。返回事件上下文的发起者（`instigator`/来源）。 |
| `GetTarget` | — | `target: any` | 纯函数（Pure）。返回事件上下文的目标对象。 |
| `GetEventTag` | — | `eventTag: string` | 纯函数（Pure）。返回当前活动的玩法事件标签。 |
| `GetPayload` | — | `payload: any` | 纯函数（Pure）。返回事件上下文的 `payload` 表。 |
| `HasTag` | `target: Source.Battler`, `tag: string` | `value: bool` | 纯函数（Pure）。检查 `target` 是否拥有指定的玩法标签。 |
| `GetNumericAttribute` | `target: Source.Battler`, `attribute: string` | `value: any` | 纯函数（Pure）。动态数值引脚。读取 `target` 的数值属性。 |
| `SetNumericAttributeBase` | `target: Source.Battler`, `attribute: string`, `value: any` | — | 动态数值引脚。写入 `target` 指定属性的基础值（Base）。 |
| `ApplyAttributeDelta` | `target: Source.Battler`, `attribute: string`, `magnitude: any` | — | 动态数值引脚。向 `target` 应用即时加法（Instant Add）修饰效果。 |
| `ApplyState` | `target: Source.Battler`, `stateID: string`, `stacks: int = 1` | `handle: int` | `Enums.GeneralData.State` 字符串枚举选择器。对 `target` 应用或叠加指定 State。 |
| `RemoveState` | `target: Source.Battler`, `stateID: string` | — | 执行分支：`Success`、`Failed`。`Enums.GeneralData.State` 字符串枚举选择器。 |
| `ReduceState` | `target: Source.Battler`, `stateID: string`, `stacks: int = 1` | — | 执行分支：`Success`、`Failed`。`Enums.GeneralData.State` 字符串枚举选择器。 |
| `RemovePlayerState` | `stateID: string` | — | `Enums.GeneralData.State` 字符串枚举选择器。移除当前事件上下文目标上的指定 State。 |
| `ReducePlayerState` | `stateID: string`, `stacks: int = 1` | — | `Enums.GeneralData.State` 字符串枚举选择器。减少当前事件上下文目标上指定 State 的层数。 |
| `SendEvent` | `target: Source.Battler`, `eventTag: string`, `payload: any = {}` | `results: any` | 向 `target` 同步发送一条玩法事件。 |
| `ApplyEffect` | `target: Source.Battler`, `effect: any`, `stacks: int = 1`, `sourceKey: any` | `handle: int` | 向 `target` 应用一个 `GlobalCore.GameplayEffect`。 |

上下文读取节点及绑定上下文的修改节点均要求所在通用数据图的所属对象为 `GameplayEventData`；在普通 Actor 蓝图中使用这些节点会报告错误。`RemovePlayerState` 与 `ReducePlayerState` 作用于该上下文的目标，当指定 State 不存在时仍走单一默认执行出口。

动态属性引脚声明类型为 `any`，但 Ability System 仍要求传入符合所选结构定义类型的有限数值。`SetNumericAttributeBase` 直接写入基础值（Base），`ApplyAttributeDelta` 则应用即时加法修饰效果。

`SendEvent` 同步执行，保留原发起者（`instigator`）并将目标设为传入的 `Battler`。`ApplyEffect` 接收一个 `GameplayEffect`；即时（Instant）效果不返回持久句柄，也无法授予标签或能力。
