# Billboard 组件

## 用途

玩家靠近时，在 Actor 头顶显示一组图片与文字，例如名称或交互提示。

## 适用范围

`Engine.Actor` 及其派生类提供可选的 `billboardComp`，类型为 `Engine.BillboardComponent`。通过[组件配置流程](<概述.md#配置步骤>)进行编辑。

## 配置步骤

1. 在**组件**中添加或打开 `billboardComp`。
2. 设置 `showRange`，即玩家与 Actor 的最大显示距离，单位为地图逻辑像素，默认 `128`。
3. 使用 `items` 中的 **+**、**-** 增减条目，条目按从上到下的顺序显示。点击条目摘要右侧的 **...** 进入编辑。
4. 选择 `text` 可设置内容、字号（默认 `12`）和颜色。选择 `image` 可通过 `path` 选择图片；此编辑器标签对应 API 的 `image` 字段。切换类型会保留隐藏字段的值。
5. 保存蓝图或地图。

## 参考链接

- [Actor 组件](<概述.md>)
- [BillboardComponent 与 BillboardItem API](<../../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/玩法类型.md#billboardcomponent>)
