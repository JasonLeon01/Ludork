# Source.MapActors.DoorBase

`DoorBase` 是用于实现门体动画的条件可见地图 Actor，声明了碰撞、开启帧间隔、门音效与开关状态属性，并提供潜伏式的开门与关门方法。

**元数据来源**：`Scripts/Source/MapActors/DoorBase_meta.lua`

## DoorBase

**继承**：`Source.MapActors.ConditionalActor`（继承的可见性条件见 [Source.MapActors.ConditionalActor](<ConditionalActor.md>)）。

`openDoor()` 与 `closeDoor()` 均返回 `Engine.AsyncOperation`。动画开始时发出 `Enums.MovementLatentOutput.STARTED`（`0`），动画完成时返回 `FINISHED`（`1`），对应蓝图中的 `Started` / `Finished` 分支。

同方向的重复请求会共享当前未完成的操作；反方向请求则会取消原有操作。对已打开的门调用开门、或对已关闭的门调用关门时会立即完成。正常开门在门销毁自身之前完成；因其他原因销毁门体时会取消尚未完成的操作。

### 属性

| 字段 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `collisionEnabled` | `bool` | `true` | — |
| `openInterval` | `float` | `0.05` | 门体开关动画的帧间隔。 |
| `gateSE` | `string` | `""` | 路径约束：`/Game/Assets/Sounds`；配置回退项：`Audio.gateSE`。 |
| `opening` | `bool` | `false` | — |
| `closing` | `bool` | `false` | — |

### 函数与事件

实例方法省略隐式接收者 `self`（`Source.MapActors.DoorBase`）；在蓝图中，该参数作为首个目标引脚暴露。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `openDoor` | — | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Started`（`0`）、`Finished`（`1`）。打开门体并在正常完成后销毁自身。 |
| `closeDoor` | — | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Started`（`0`）、`Finished`（`1`）。关闭门体。 |
| `onTick` | `deltaTime: float` | — | 事件（Event）。默认执行出口。 |
