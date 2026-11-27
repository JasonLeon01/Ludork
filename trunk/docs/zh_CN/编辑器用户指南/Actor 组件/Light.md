# Light 组件

为 Actor 挂载动态点光源（如手持提灯或墙上火把），使其随 Actor 的变换同步移动。

`Engine.Actor` 及其派生类通过可选字段 `lightComp`（类型为 `Engine.LightComponent`）提供此能力。地图光照统一通过 Actor 组件配置，并在**角色信息**中调整实例覆盖值。

## 配置步骤

1. 按[组件配置流程](<概述.md#配置步骤>)添加或打开 `lightComp`。
2. 配置 `lightColour` 与大于零的 `lightRadius`（默认为白光、半径 `16`）。
3. 配置 `lightOffset`，指定从 Actor 本地边界中心到光源位置的偏移（默认 `[0, 0]`，随 Actor 的旋转和缩放同步变换）。墙上火把应使用独立 Actor，并将 `lightOffset` 置于墙体朝向房间的一侧。
4. 在**角色信息**中编辑单实例覆盖值并保存蓝图或地图。可通过 [Actor 与图层辅助显示](<../../PreviewHost/地图与世界预览.md#actor-与图层辅助显示>)检视光照与范围叠加效果。

**相关指南与 API**：[LightComponent](<../../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/玩法类型.md#lightcomponent>) · [地图光源编辑](<../图块集、自动图块与地图.md#光源与-actor>)
