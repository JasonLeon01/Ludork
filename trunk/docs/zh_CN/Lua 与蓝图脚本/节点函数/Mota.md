# GlobalFunctions.Mota

`GlobalFunctions.Mota` 提供用于打开怪物手册与楼层传送器、管理当前区域以及执行同位上下楼或中心对称传送的蓝图节点。

**元数据来源**：`Scripts/GlobalFunctions/Mota_meta.lua`

## Mota

### 函数与事件

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `OpenMonsterBook` | — | — | 在当前活动 `SceneMap` 中打开怪物手册窗口。 |
| `OpenFloorTeleporter` | — | — | 在当前活动 `SceneMap` 中打开楼层传送器窗口。 |
| `GetCurrentRegion` | — | `region: string` | 纯函数（Pure）。返回当前活动区域的标识符。 |
| `SetCurrentRegion` | `region: string` | — | 设置当前 `GameInstance` 的活动区域标识符。 |
| `CenterSymmetricTeleport` | — | `int` | 执行分支：`Success`（`0`）、`Failed`（`1`）。尝试在当前地图上执行中心对称传送。 |
| `GoUpstairsSamePos` | — | `int` | 执行分支：`Success`（`0`）、`Failed`（`1`）。在相同地图格坐标上传送至上一楼层。 |
| `GoDownstairsSamePos` | — | `int` | 执行分支：`Success`（`0`）、`Failed`（`1`）。在相同地图格坐标上传送至下一楼层。 |
