# Light 组件

为 Actor 挂载随其变换移动的光源，例如手持灯或墙上火把。

`Engine.Actor` 及其派生类提供可选的 `lightComp`，类型为 `Engine.LightComponent`。地图光照通过这些 Actor 组件配置，使用与普通 Actor 相同的选择方式和属性面板。

## 配置步骤

1. 通过[组件配置流程](<概述.md#配置步骤>)添加或打开 `lightComp`。
2. 设置 `lightColour` 和大于零的 `lightRadius`。新组件默认为白光，半径为 `16`。
3. 设置 `lightOffset`，即从 Actor 本地边界中心到预期光源的偏移；默认 `[0,0]`，并随 Actor 的完整变换变化。墙上火把应使用独立 Actor，并将光源放在墙体朝向房间的一侧。
4. 在**角色信息**中编辑实例覆盖值，然后保存蓝图或地图。查看光照效果与范围辅助的方式见 [PreviewHost：Actor 与图层辅助显示](<../../PreviewHost/地图与世界预览.md#actor-与图层辅助显示>)。

## 参考链接

- [LightComponent API](<../../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/玩法类型.md#lightcomponent>)
- [地图灯光编辑](<../图块集、自动图块与地图.md#灯光与-actor>)
