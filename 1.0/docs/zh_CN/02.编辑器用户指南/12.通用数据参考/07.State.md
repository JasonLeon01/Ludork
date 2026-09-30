# State

来源：`Data/General/State.json`

| 属性 | 值 |
|---|---|
| 成员事件 | `onWalk`, `onHookTriggered` |

## Schema

| 字段 | 类型 | 默认值 | 描述 | 引用 |
|---|---|---|---|---|
| `name` | `string` | `""` | 该 State 的显示名称。 | `—` |
| `desc` | `string` | `""` | 该 State 的显示说明。 | `—` |
| `icon` | `file` | `""` | 完整的图标逻辑路径。选择器根路径为 `/Game/Assets/Icons`。 | `—` |
| `stackable` | `bool` | `false` | 再次添加该 State 时，是否增加已有 State 的层数。 | `—` |

## 编辑说明

应用 State 会创建 Infinite Gameplay Effect，并授予该 State 的事件 Ability。`onWalk` 监听 `Event.Movement.Step`。可以用 `Event.State.Trigger.<stateID>` 显式触发 `onHookTriggered`，也可以在玩法图内调用 `GlobalFunctions.Gameplay.SendEvent`。添加或叠加 State 都不会发出 `onHookTriggered`。图中没有起始节点时，激活返回 `NoGraph`。
