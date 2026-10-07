# Source.MapActors.DoorBase

`DoorBase` 声明门的碰撞、开启间隔、开门音效与门状态属性，以及开门和关门节点。

Metadata 来源：`Scripts/Source/MapActors/DoorBase_meta.lua`

`openDoor()` 与 `closeDoor()` 返回 `Engine.AsyncOperation`。动画开始时发布 `Enums.MovementLatentOutput.STARTED`（`0`），结束时以 `FINISHED`（`1`）完成，对应蓝图的 `Started` / `Finished` 分支。

同方向的重复请求共享未完成操作，反向播放会取消原操作。对已打开的门开门、对已关闭的门关门都会立即完成。正常开门在门销毁自身之前完成，其他销毁情形取消未完成操作。

## DoorBase

直接 metadata 基类：`{ "Source.MapActors.ConditionalActor", "ConditionalActor" }`

继承的可见性条件见 [Source.MapActors.ConditionalActor](<ConditionalActor.md>)。

Meta：`{ PathVars = { [1] = { [1] = "gateSE", [2] = "/Game/Assets/Sounds" } }, ConfigVars = { [1] = { [1] = "gateSE", [2] = "Audio", [3] = "gateSE" } } }`

### 属性

| 名称 | 类型 | 默认值 | Metadata |
|---|---|---|---|
| `collisionEnabled` | `bool` | `true` | — |
| `openInterval` | `float` | `0.05` | — |
| `gateSE` | `string` | `""` | Meta { PathVars = "/Game/Assets/Sounds", ConfigVars = { [1] = "Audio", [2] = "gateSE" } } |
| `opening` | `bool` | `false` | — |
| `closing` | `bool` | `false` | — |

### 函数与事件

| 名称 | 种类 | 参数 | 返回 | 执行语义与 metadata |
|---|---|---|---|---|
| `openDoor` | `function` | self: { "Source.MapActors.DoorBase", "DoorBase" } = "self" | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Started", [2] = "Finished", Started = { [1] = 0 }, Finished = { [1] = 1 } } |
| `closeDoor` | `function` | self: { "Source.MapActors.DoorBase", "DoorBase" } = "self" | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Started", [2] = "Finished", Started = { [1] = 0 }, Finished = { [1] = 1 } } |
| `onTick` | `event` | deltaTime: float | — | ExecSplit { [1] = "default", default = nil } |
