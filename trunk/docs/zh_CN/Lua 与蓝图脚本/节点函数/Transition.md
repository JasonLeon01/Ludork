# GlobalFunctions.Transition

`GlobalFunctions.Transition` 提供用于捕获离开画面并播放屏幕转场的潜伏蓝图节点。两个函数均返回 `Engine.AsyncOperation` 并以 `true` 完成：冻结操作在捕获离开画面后触发 `Frozen`，转场操作在提交最后一帧后触发 `Finished`。替换与取消规则见 [GlobalCore.Transition](<../全局与 Core 模块/Core Modules/GlobalCore/场景与系统.md#transition>)。

**元数据来源**：`Scripts/GlobalFunctions/Transition_meta.lua`

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `FreezeTransitionBackground` | — | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Frozen`（`true`）。捕获当前离开画面作为转场背景。 |
| `RequestTransition` | `transitionName: string = ""`, `transitionTime: float = 1` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Finished`（`true`）。路径约束（`transitionName`）：`/Game/Assets/Transitions`。 |
