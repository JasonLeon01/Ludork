# GlobalFunctions.Movement

`GlobalFunctions.Movement` 提供用于开关 Actor 移动、指派显式移动路线及启动自动寻路的蓝图节点。

**元数据来源**：`Scripts/GlobalFunctions/Movement_meta.lua`

路线与寻路函数均返回 `Engine.AsyncOperation`：开始移动时发出 `Enums.MovementLatentOutput.STARTED`（`0`），移动完成时返回 `FINISHED`（`1`），对应蓝图中的 `Started` / `Finished` 分支。路线替换与取消规则见 [Actor.setRoute](<../全局与 Core 模块/Core Modules/Engine/玩法类型.md#路线与操作生命周期>)。

## Movement

### 函数与事件

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `SetMoveEnabledByTag` | `tag: string`, `enabled: bool = true` | — | 启用或禁用与 `tag` 匹配的 Actor 的移动能力。 |
| `SetMoveRoute` | `actor: Engine.Actor`, `route: sf.Vector2i[] = {}` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Started`（`0`）、`Finished`（`1`）。`route` 参数启用移动路线编辑器（`MoveRouteVars`）。 |
| `SetAutoPathToDestination` | `actor: Engine.Actor`, `destination: sf.Vector2i = sf.Vector2i(0, 0)` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Started`（`0`）、`Finished`（`1`）。令 `actor` 自动寻路至 `destination`。 |
| `SetAutoPathToDestinationByTag` | `tag: string`, `destination: sf.Vector2i = sf.Vector2i(0, 0)` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Started`（`0`）、`Finished`（`1`）。令与 `tag` 匹配的 Actor 自动寻路至 `destination`。 |
