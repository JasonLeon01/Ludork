# GlobalFunctions.Transition

两个函数返回 `Engine.AsyncOperation`，成功结果为 `true`：冻结在离开画面捕获后完成（`Frozen`），转场在最后一帧提交后完成（`Finished`）。替换与取消规则见 [GlobalCore.Transition](<../全局与 Core 模块/Core Modules/GlobalCore/场景与系统.md#transition>)。

Metadata 来源：`Scripts/GlobalFunctions/Transition_meta.lua`

| 名称 | 种类 | 参数 | 返回 | 执行语义与 metadata |
|---|---|---|---|---|
| `FreezeTransitionBackground` | `function` | — | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Frozen", Frozen = { [1] = true } } |
| `RequestTransition` | `function` | transitionName: string = ""; transitionTime: float = 1 | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Finished", Finished = { [1] = true } }; Meta { PathVars = { [1] = { [1] = "transitionName", [2] = "/Game/Assets/Transitions" } } } |
