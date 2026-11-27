# Source.MapActors.Teleporter

`Teleporter` 是用于实现楼梯上下楼与指定地图传送的条件可见地图 Actor，声明了楼梯方向、落点偏移、音效、转场与计时属性，并提供对应的蓝图传送方法。

**元数据来源**：`Scripts/Source/MapActors/Teleporter_meta.lua`

## Teleporter

**继承**：`Source.MapActors.ConditionalActor`（继承的可见性条件见 [Source.MapActors.ConditionalActor](<ConditionalActor.md>)）。

`goUpstairs` 与 `goDownstairs` 在传送请求受理后，会记录触发楼梯的 `getTeleportPosition()`（含 `Offset`）及其原始 `getMapTag()`；抵达目标地图后再记录目标楼梯。传送点发现与显示规则见[窗口、菜单、输入与控件](<../默认玩法/窗口、菜单、输入与控件.md>)。

`stairDirection` 使用独立枚举模块 `Enums.StairDirection`，提供 `None`（普通传送点，默认值）、`Up`（上楼梯）与 `Down`（下楼梯）三个选项，并保存对应的字符串值。包括自定义变体在内的所有楼梯蓝图都应配置正确的方向。Lua 代码通过 `local StairDirection = require("Enums.StairDirection")` 加载，并使用 `StairDirection.None`、`StairDirection.Up` 或 `StairDirection.Down`。

上楼会在下一张地图中选取最近的、可见且未销毁的 `Down` 楼梯；下楼则选取 `Up` 楼梯。距离以出发楼梯的偏移位置到各候选楼梯地图坐标的欧氏距离平方计算；距离相等时保持 Actor 遍历顺序，抵达后应用目标楼梯的 `Offset`。若缺少对应方向的楼梯，则按标准取消流程恢复移动并解除转场冻结，不会改选其他方向楼梯或普通传送点。

`FindNearestTeleporter(actors, position, stairDirection?)` 接受相同的枚举参数：省略参数时搜索全部传送点，传入 `None` 时仅筛选普通传送点。`IsAsideOrOverlapping` 与通用最近传送点传送均搜索全部方向。

`goToMap(mapPath, position, record = true)` 在蓝图中提供地图与格子坐标选择器，直接传送至目标 `position`，不搜索目标地图上的 Teleporter。启用记录时，出发地图记录本 Actor 的 `getTeleportPosition()` 与原始标签，目标地图记录所选格子且标签为空；传入 `false` 时两端均不新增记录，已有记录保持不变。若目标为子地图，系统会自动解析为所属世界清单与世界坐标。

每个端点记录均保存在各自所属的地图路径下。出发地图与目标地图分别归属各自配置的区域，楼层传送器窗口仅列出当前区域内已访问的地图。楼梯传送按出发地图解析区域，加载地图时按目标地图重新解析区域；若地图不属于任何已配置区域，系统会清空当前区域且楼层列表为空。

传送请求通过 [GameplayScene.requestFloorStep 与 requestMapTransfer](<Gameplay.md#gameplayscene>) 提交；被拒绝的请求不会改变移动状态或传送记录。

### 属性

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `Offset` | `sf.Vector2i` | `sf.Vector2i(0, 0)` | 传送抵达时应用的格子坐标偏移。 |
| `stairDirection` | `Enums.StairDirection` | `"None"` | 字符串枚举选择器（`None`、`Up`、`Down`）。 |
| `stairSE` | `string` | `""` | 路径约束：`/Game/Assets/Sounds`；配置回退项：`Audio.stairSE`。 |
| `transitionName` | `string` | `""` | 路径约束：`/Game/Assets/Transitions`。 |
| `transitionTime` | `float` | `0.5` | 转场持续时间（秒）。 |

### 函数与事件

实例方法省略隐式接收者 `self`（`Source.MapActors.Teleporter`）；在蓝图中，该参数作为首个目标引脚暴露。留空时默认使用当前 Teleporter 蓝图所属对象，也可连接其他 Teleporter 实例。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `goUpstairs` | — | — | 传送至上一楼层，并在请求受理后记录两端端点。默认执行出口。 |
| `goDownstairs` | — | — | 传送至下一楼层，并在请求受理后记录两端端点。默认执行出口。 |
| `goToMap` | `mapPath: string`, `position: sf.Vector2i`, `record: bool = true` | — | 直接传送至 `mapPath` 的 `position` 处。`Transfer` 元数据将 `position` 与 `mapPath` 关联。默认执行出口。 |
