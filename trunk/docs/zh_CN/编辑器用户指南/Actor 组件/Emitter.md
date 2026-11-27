# Emitter 组件

为 Actor 直接挂载 GPU 通用粒子特效（如火焰、光环或火花）。粒子的轨道、纹理与生命周期曲线请在[通用粒子](<../通用粒子.md>)中制作。

`Engine.Actor` 及其派生类通过可选字段 `emitterComp`（类型为 `Engine.EmitterComponent`）提供此能力。若需在 UI 布局内播放粒子特效，请改用 [Engine.EmitterView](<../Palette 控件.md#engineemitterview>)。

## 配置步骤

1. 在 `Data/Particles` 下创建并预览通用粒子资产。
2. 按[组件配置流程](<概述.md#配置步骤>)添加或打开 `emitterComp`。
3. 从实时生成的 `Enums.Particle` 字符串枚举中选择 `resource`（不含扩展名的键，例如 `Combat/Hit`；留空时禁用特效）。
4. 设置 `anchor` 为 Actor 本地边界内的归一化锚点（默认 `[0.5, 0.5]`），并调整相对于 Actor 的 `offset`、`rotation` 与 `scale`。
5. 保持 `beforeActor` 为 `false` 时，特效紧随宿主之后绘制；设为 `true` 时，紧随宿主之前绘制。普通地图与组合式大地图均保持原有的 Actor 列表与图层顺序，该选项不引入 Y 轴排序。
6. 保存蓝图或地图。

**API 参考**：[EmitterComponent](<../../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/时间、渲染与粒子.md#emittercomponent>)
