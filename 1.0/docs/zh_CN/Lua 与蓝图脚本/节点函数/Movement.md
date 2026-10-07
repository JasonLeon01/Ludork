# GlobalFunctions.Movement

`Movement` 声明切换移动、设置移动路线与启动自动寻路的节点。

Metadata 来源：`Scripts/GlobalFunctions/Movement_meta.lua`

路线与寻路函数返回 `Engine.AsyncOperation`，开始时发布 `Enums.MovementLatentOutput.STARTED`（`0`），结束时以 `FINISHED`（`1`）完成，对应蓝图的 `Started` / `Finished` 分支。路线替换与取消规则见 [Actor.setRoute](<../全局与 Core 模块/Core Modules/Engine/玩法类型.md#路线与操作生命周期>)。

## Movement

直接 metadata 基类：—

### 属性

未声明任何可编辑属性。

### 函数与事件

| 名称 | 种类 | 参数 | 返回 | 执行语义与 metadata |
|---|---|---|---|---|
| `SetMoveEnabledByTag` | `function` | tag: string; enabled: bool = true | — | ExecSplit { [1] = "default", default = nil } |
| `SetMoveRoute` | `function` | actor: { "Engine", "Actor" }; route: sf.Vector2i[] = {  } | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Started", [2] = "Finished", Started = { [1] = 0 }, Finished = { [1] = 1 } }; Meta { MoveRouteVars = { [1] = "route" } } |
| `SetAutoPathToDestination` | `function` | actor: { "Engine", "Actor" }; destination: sf.Vector2i = { [1] = 0, [2] = 0 } | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Started", [2] = "Finished", Started = { [1] = 0 }, Finished = { [1] = 1 } } |
| `SetAutoPathToDestinationByTag` | `function` | tag: string; destination: sf.Vector2i = { [1] = 0, [2] = 0 } | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Started", [2] = "Finished", Started = { [1] = 0 }, Finished = { [1] = 1 } } |
