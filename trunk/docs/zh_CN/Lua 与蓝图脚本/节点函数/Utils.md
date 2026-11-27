# GlobalFunctions.Utils

`GlobalFunctions.Utils` 提供通用蓝图节点，涵盖流程控制、局部变量与游戏变量、队伍成员管理、属性读写、动画辅助、事件总线与公共函数调用。

**元数据来源**：`Scripts/GlobalFunctions/Utils_meta.lua`

`RunCommonFunction` 与调用方共享局部上下文及蓝图所属对象，嵌套调用亦遵循同一规则。`SELF` 返回该所属对象，同时也是公共函数内部未连接目标引脚的实例方法的默认接收者。

`SUPER(obj, params)` 用于调用当前蓝图事件的父级实现。继承查找、参数数组与 `nil` 回填规则见[父事件调用](<../蓝图脚本/执行流、事件与变量.md#父事件调用>)。

## Utils

### 函数与事件

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `IF` | `condition: bool = false` | `int` | 执行分支：`TRUE`（`0`）、`FALSE`（`1`）。 |
| `SetLocalValue` | `valueName: string`, `value: any` | — | 在当前图上下文中设置节点局部变量。 |
| `GetLocalValue` | `valueName: string`, `default: any` | `value: any` | 纯函数（Pure）。读取节点局部变量；未设置时返回 `default`。 |
| `GetLocalValueRef` | `valueName: string`, `default: any` | `value: any` | 纯函数（Pure）。返回节点局部变量的可变引用。 |
| `SetGameVariable` | `valueName: string`, `value: any` | — | `valueName` 使用 `InstVar` 选择器；`InstVarValue` 按 `valueName` 的类型适配 `value`。 |
| `GetGameVariable` | `valueName: string`, `default: any` | `value: any` | 纯函数（Pure）。`valueName` 使用 `InstVar` 选择器；`InstVarValue` 按 `valueName` 的类型适配 `default`。 |
| `GetGameVariableRef` | `valueName: string`, `default: any` | `value: any` | 纯函数（Pure）。返回游戏变量的可变引用。`valueName` 使用 `InstVar` 选择器；`InstVarValue` 将 `default` 绑定至 `valueName`。 |
| `AddPlayerByClass` | `playerClass: string = ""`, `mapPath: string = ""`, `position: sf.Vector2i` | — | `playerClass` 启用蓝图类选择器（`BlueprintClassVars`）；`Transfer` 元数据将 `position` 与 `mapPath` 关联。 |
| `SetPlayerByClass` | `playerClass: string = ""` | — | `playerClass` 启用蓝图类选择器（`BlueprintClassVars`）。切换当前主玩家。 |
| `RemovePlayerByClass` | `playerClass: string = ""` | — | `playerClass` 启用蓝图类选择器（`BlueprintClassVars`）。移除一名队伍成员。 |
| `AddAnim` | `animName: string`, `position: sf.Vector2f = sf.Vector2f(0, 0)`, `rotation: float = 0`, `scale: sf.Vector2f = sf.Vector2f(1, 1)` | — | `Enums.Animation` 选择器。在 `position` 处播放动画。 |
| `AddAnimOn` | `animName: string`, `actorTag: string`, `rotation: float = 0`, `scale: sf.Vector2f = sf.Vector2f(1, 1)` | — | `Enums.Animation` 选择器。以与 `actorTag` 匹配的 Actor 为中心播放动画。 |
| `GetAnimLength` | `animName: string` | `value: float` | 纯函数（Pure）。`Enums.Animation` 选择器。返回动画总时长。 |
| `GetAnimVisualLength` | `animName: string` | `value: float` | 纯函数（Pure）。`Enums.Animation` 选择器。返回动画视觉时长。 |
| `SUPER` | `obj: any`, `params: any[] = {}` | `value: bool` | 在 `obj` 上调用当前蓝图事件的父级实现。 |
| `SELF` | — | `value: any` | 纯函数（Pure）。返回当前蓝图的所属对象。 |
| `GetAttrRef` | `obj: any`, `attrName: string` | `value: any` | 纯函数（Pure）。返回组件字段或对象属性的可变引用。 |
| `GetAttr` | `obj: any`, `attrName: string` | `value: any` | 纯函数（Pure）。读取组件字段或对象属性。 |
| `SetAttr` | `obj: any`, `attrName: string`, `value: any` | — | 写入组件字段或对象属性。 |
| `GetScene` | — | `value: GlobalCore.SceneBase` | 纯函数（Pure）。从 `SceneManager` 返回当前活动场景。 |
| `IsValidValue` | `value: any` | `value: bool` | 纯函数（Pure）。当 `value` 非 `nil` 时返回 `true`。 |
| `ToShortNumber` | `value: any = 0` | `value: any` | 纯函数（Pure）。将数值格式化为紧凑短记法。 |
| `RunCommonFunction` | `commonFunctionName: string = ""` | `any` | `commonFunctionName` 启用公共函数选择器（`CommonFunctionVars`）。 |
| `RegisterEventBus` | `key: string`, `obj: any`, `functionName: string` | — | 在事件总线的 `key` 下注册对象方法。 |
| `RegisterEventBusEvent` | `key: string`, `obj: any`, `eventName: string` | — | 在事件总线的 `key` 下注册蓝图事件。 |
| `UnregisterEventBus` | `key: string` | `value: bool` | 注销事件总线中 `key` 下的所有处理程序。 |
| `UnregisterEventBusEvent` | `key: string`, `obj: any` | `value: bool` | 注销事件总线中 `obj` 在 `key` 下注册的事件处理程序。 |
| `TriggerEventBus` | `key: string`, `kwargs: any = {}` | — | 触发事件总线中注册在 `key` 下的所有处理程序。 |
| `TriggerBlueprintEvent` | `obj: any`, `eventName: string` | — | 触发 `obj` 上指定名称的蓝图事件。 |
| `BackToTitle` | — | — | 返回标题场景。 |
| `Print` | `message: any = ""` | — | 将 `message` 输出至控制台。 |
| `EXEC` | `script: string = ""` | — | 执行一段 Lua 脚本字符串。 |
| `GetSelfAttr` | `attrName: string` | `value: any` | 纯函数（Pure）。读取当前蓝图所属对象的组件字段或属性。 |
| `SetSelfAttr` | `attrName: string`, `value: any` | — | 写入当前蓝图所属对象的组件字段或属性。 |
| `IfPlayerOverlaps` | — | `value: bool` | 纯函数（Pure）。返回玩家是否与当前 Actor 重叠。 |
| `IfGameVar` | `varName: string = ""`, `op: string = "=="`, `value: any` | `bool` | 执行分支：`TRUE`（`true`）、`FALSE`（`false`）。`op` 下拉选项：`==`、`!=`、`<`、`<=`、`>`、`>=`。`varName` 使用 `InstVar` 选择器；`InstVarValue` 将 `value` 绑定至 `varName`。 |

`AddPlayerByClass` 会记录新成员的所在地图与格子坐标，但不切换当前主玩家。`SetPlayerByClass` 会将该玩家类移至 `playerKeys[1]`，重新绑定场景 UI，并加载该成员记录的地图。

`GetAttr` 与经由 `GetAttrRef` 的读取在组件字段非 `nil` 时返回该字段值，否则读取对象自身属性。两者均会保留 `false`、`0` 与空字符串；若不存在任何非 `nil` 值则会抛出错误。读取 Actor 的 `tag` 会得到字符串值，可直接连接至对话节点的 `refActorTag` 输入引脚。
