# Emitter 组件

## 用途

为 Actor 挂载火焰、光环或火花等通用粒子效果。粒子的轨道、贴图与生命周期曲线在[通用粒子编辑器](<../通用粒子.md>)中制作。

## 适用范围

`Engine.Actor` 及其派生类提供可选的 `emitterComp`，类型为 `Engine.EmitterComponent`。UI 内的粒子效果使用 `Engine.EmitterView`。

## 配置步骤

1. 在 `Data/Particles` 下创建并预览通用粒子资源。
2. 通过[组件配置流程](<概述.md#配置步骤>)添加或打开 `emitterComp`。
3. 选择 `resource`，引用不含扩展名的资源键，例如 `Combat/Hit`。资源为空时禁用效果。
4. 设置 `anchor`，它是 Actor 本地边界内的归一化位置，默认 `[0.5,0.5]`。通过相对于 Actor 的 `offset`、`rotation` 和 `scale` 调整效果位置与变换。
5. `beforeActor` 为 false 时紧接宿主之后绘制，开启后紧接宿主之前绘制。普通地图与大地图均保留 Actor 列表和图层顺序，此设置不增加 Y 排序。
6. 保存蓝图或地图。

## 参考链接

- [Actor 组件](<概述.md>)
- [通用粒子](<../通用粒子.md>)
- [EmitterComponent API](<../../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/时间、渲染与粒子.md#emittercomponent>)
- [UI 中的 EmitterView](<../Palette 控件.md#engineemitterview>)
