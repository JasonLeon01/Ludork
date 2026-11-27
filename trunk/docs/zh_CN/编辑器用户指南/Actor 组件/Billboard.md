# Billboard 组件

当玩家进入指定距离范围时，在 Actor 头顶纵向显示一组图片与文字（如名称铭牌或交互提示）。

`Engine.Actor` 及其派生类通过可选字段 `billboardComp`（类型为 `Engine.BillboardComponent`）提供此能力，请按通用的[组件配置流程](<概述.md#配置步骤>)管理。

## 配置步骤

1. 在**组件**中添加或打开 `billboardComp`。
2. 设置 `showRange`，指定玩家与 Actor 之间的最大显示距离（单位为地图逻辑像素，默认 `128`）。
3. 在 `items` 中使用 **+** 与 **-** 按从上到下的顺序增减条目，点击条目摘要旁的 **...** 进入编辑。
4. 选择 `text` 可配置文本内容、字号（默认 `12`）与颜色；选择 `image` 可通过 `path` 选择图片（对应运行时 API 的 `image` 字段）。切换条目类型时，隐藏字段的值会保留。
5. 保存蓝图或地图。

**API 参考**：[BillboardComponent 与 BillboardItem](<../../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/玩法类型.md#billboardcomponent>)
