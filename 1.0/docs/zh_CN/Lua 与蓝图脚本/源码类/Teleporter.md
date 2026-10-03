# Source.MapActors.Teleporter

`Teleporter` 声明楼梯方向、偏移、音效、转场与计时属性，以及上下楼和指定地图传送节点。三个蓝图方法都保留有类型的 `self` 输入：不填写时使用当前 Teleporter 蓝图的 owner，也可以连接另一个 Teleporter 实例。

Metadata 来源：`Scripts/Source/MapActors/Teleporter_meta.lua`

## Teleporter

直接 metadata 基类：`{ "Source.MapActors.ConditionalActor", "ConditionalActor" }`

继承的可见性条件见 [Source.MapActors.ConditionalActor](<ConditionalActor.md>)。

`goUpstairs` 与 `goDownstairs` 在传送请求通过后，记录触发楼梯的 `getTeleportPosition()`（含 `Offset`）与原始 `getMapTag()`；抵达时记录目标楼梯。发现与显示规则见 [窗口、菜单、输入与控件](<../Game 玩法/窗口、菜单、输入与控件.md>)。

`stairDirection` 使用 `Teleporter.StairDirection` 字符串枚举。蓝图属性下拉框可选择 `None`（普通传送点，默认值）、`Up`（上楼梯）或 `Down`（下楼梯）。所有楼梯蓝图，包括自定义变体，都应配置方向。Lua 代码直接使用现有 Teleporter 类上的 `Teleporter.StairDirection.None`、`.Up` 或 `.Down`。

上楼时，在下一张地图中选择最近的、可见且未销毁的 `Down` 楼梯；下楼时选择 `Up` 楼梯。以出发楼梯含偏移的位置为基准，比较候选楼梯地图坐标的欧氏距离平方；同距离保持 Actor 遍历顺序，抵达时应用目标楼梯的 `Offset`。缺少对应楼梯时沿用现有取消处理，恢复移动并解除转场冻结，不改选其他方向或普通传送点。

`FindNearestTeleporter(actors, position, stairDirection?)` 接受同一枚举。省略参数时搜索全部传送点；传入 `None` 时只筛选普通传送点。`IsAsideOrOverlapping` 和通用最近传送点传送仍搜索所有方向。

`goToMap(mapPath, position, record = true)` 在蓝图中提供地图与格子坐标选择器，直接传送到 `position`，不搜索目标地图中最近的 Teleporter。开启记录时，出发地图记录本 Actor 的 `getTeleportPosition()` 与原始标签，目标地图记录指定落点，标签为空。传入 `false` 时两边都不新增记录，已有记录保持不变。目标为子地图时，会转换为所属世界清单与世界坐标。

两端记录各自保存在所属地图路径下，出发地图与目标地图分别归属各自配置的区域；楼层传送器窗口只列出当前区域已记录的地图。上下楼根据出发地图查找区域，加载目标地图时重新查找目标区域。目标地图未配置区域时清空当前区域，楼层列表为空。

传送请求通过 [GameplayScene.requestFloorStep 与 requestMapTransfer](<Gameplay.md#gameplayscene>) 提交。请求被拒绝时不改变移动状态或记录。

Meta：`{ PathVars = { [1] = { [1] = "stairSE", [2] = "/Game/Assets/Sounds" }, [2] = { [1] = "transitionName", [2] = "/Game/Assets/Transitions" } }, ConfigVars = { [1] = { [1] = "stairSE", [2] = "Audio", [3] = "stairSE" } } }`

### 属性

| 名称 | 类型 | 默认值 | Metadata |
|---|---|---|---|
| `Offset` | `sf.Vector2i` | `{ [1] = 0, [2] = 0 }` | — |
| `stairDirection` | `string` 枚举 | `"None"` | Meta { DropBox = { "None", "Up", "Down" } } |
| `stairSE` | `string` | `""` | Meta { PathVars = "/Game/Assets/Sounds", ConfigVars = { [1] = "Audio", [2] = "stairSE" } } |
| `transitionName` | `string` | `""` | Meta { PathVars = "/Game/Assets/Transitions" } |
| `transitionTime` | `float` | `0.5` | — |

### 函数与事件

| 名称 | 种类 | 参数 | 返回 | 执行语义与 metadata |
|---|---|---|---|---|
| `goUpstairs` | `function` | self: { "Source.MapActors.Teleporter", "Teleporter" } = "self" | — | ExecSplit { [1] = "default", default = nil } |
| `goDownstairs` | `function` | self: { "Source.MapActors.Teleporter", "Teleporter" } = "self" | — | ExecSplit { [1] = "default", default = nil } |
| `goToMap` | `function` | self: { "Source.MapActors.Teleporter", "Teleporter" } = "self"; `mapPath: string`、`position: sf.Vector2i`、`record: bool = true` | — | 默认执行出口；`Transfer` 将 `position` 与 `mapPath` 关联 |
