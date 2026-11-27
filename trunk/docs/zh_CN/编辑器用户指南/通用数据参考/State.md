# State

定义持久状态效果、是否支持层数叠加，以及按步触发或由事件钩子触发的能力图。

**来源**：`Data/General/State.json` · **成员事件**：`onWalk`、`onHookTriggered`

## Schema

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `name` | `string` | `""` | 该状态的显示名称。 |
| `desc` | `string` | `""` | 该状态的显示说明。 |
| `icon` | `file` | `""` | 规范逻辑图标路径（选择器根目录：`/Game/Assets/Icons`）。 |
| `stackable` | `bool` | `false` | 再次施加同一状态时，是否递增已有状态的叠加层数。 |

## 编辑说明

施加 State 会创建一个无限持续时间的玩法效果，并授予其事件能力：

- **`onWalk`**：监听 `Event.Movement.Step`。
- **`onHookTriggered`**：由 `Event.State.Trigger.<stateID>` 或玩法图中的 `GlobalFunctions.Gameplay.SendEvent` 显式触发；施加或叠加状态本身不会发出 `onHookTriggered`。
