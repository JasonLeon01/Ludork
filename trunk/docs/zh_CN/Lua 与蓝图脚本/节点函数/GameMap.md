# GlobalFunctions.GameMap

`GlobalFunctions.GameMap` 提供用于在当前活动地图上查找 Actor 的纯函数蓝图节点。

**元数据来源**：`Scripts/GlobalFunctions/GameMap_meta.lua`

## GameMap

### 函数与事件

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `GetActorByTag` | `tag: string` | `actor: Engine.Actor` | 纯函数（Pure）。返回当前活动地图中与 `tag` 匹配的 Actor。 |
| `GetAllActors` | — | `actors: Engine.Actor[]` | 纯函数（Pure）。返回当前活动地图上的全部 Actor。 |
