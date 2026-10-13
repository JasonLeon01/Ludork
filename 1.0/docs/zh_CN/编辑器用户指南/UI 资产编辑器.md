# UI 资产编辑器

UI 资产编辑器用于直观地编排界面、窗口和可复用组件。窗口包含 Palette（控件面板）、Hierarchy（层级）、Designer（设计区）、Inspector（属性）和 Timeline（时间轴）。

## 打开或创建资产

选择 **游戏 → 新建 UI 资产**，或从项目文件浏览器创建、打开 `Data/UI/Assets` 下的 JSON 资产。资产归属（`Parts/<Owner>` 与 `Shared`）见 [声明式 UI 编写工作流](<../Lua 与蓝图脚本/声明式 UI/编写工作流.md>)。

![显示 Palette、Hierarchy、Designer 和 Inspector 的 UI 资产编辑器](../../../images/editor/ui-asset-editor.png)

*UI 资产编辑器将可用控件、树结构、可视化布局与选中节点属性分开显示。*

## 步骤

1. 设置资产的设计宽度与高度。
2. 添加根控件。新屏幕通常以 `Engine.Canvas` 开头。
3. 把系统控件或已暴露的项目资产从 Palette 拖入 Hierarchy。每个控件的用途以及如何在它们之间选择，见 [Palette 控件](<Palette 控件.md>)。
4. 逐个选中节点，在 Inspector 中编辑控件属性。
5. 配置子节点的 Slot。容器允许的子节点数量和布局方式见[子节点与 Slot](<Palette 控件.md#子节点与-slot>)。Canvas 使用锚点、偏移、对齐、自动尺寸和层叠顺序；List 负责排列自己的子节点。
6. 为每个节点设置名称，且名称在当前资产内必须唯一。生成的 View 使用这些名称访问控件或子 View。
7. 只有当资产本身完整、可复用，并且应当出现在 Project Palette 中时，才启用 **Exposed**。
8. 保存后，使用主窗口的**导出**操作生成 Lua View 和声明。

将 Palette 控件拖到 Hierarchy 容器行的中间可添加子节点，拖到行顶部或底部的四分之一区域可插入同级节点；拖到层级空白处则追加到根节点。双击 Palette 控件仍按当前选择添加。

将节点移入 Canvas 时会保留可见位置和固定尺寸，拉伸方向则随新父节点的可用尺寸变化。旋转和缩放保持不变，因此父节点的变换仍可能影响最终效果。同一父节点内重新排序不会改变 Slot。更换父节点、缩进或取消缩进各算一步撤销。

WrapBox 只允许一个子模板。设计区会显示全部重复项，编辑其中任意一项都会修改同一模板。各控件的属性和子节点规则见 [Palette 控件](<Palette 控件.md>)。

## Timeline

时间轴左侧列出动画。右键可新建或删除动画；选中动画后，可编辑名称、目标、时长、轴心、轨道和关键帧。单击列表空白处可取消选择，恢复界面原本的外观。颜色关键帧使用整数 RGBA 分量。

可通过播放、暂停、停止和拖动时间轴预览动画，不会改动已保存的 Slot 布局。

嵌套的项目节点会列出从它所属资产继承来的 Global 动画。用 **覆盖动画** 操作为当前实例创建同名动画，再在本地编辑；继承来的定义本身仍然是只读的。

## 资产路径与节点名称

资产标识是相对于 `Data/UI/Assets` 的路径，区分大小写，使用正斜杠且不带扩展名。移动或重命名资产会更新受管 JSON 中的嵌套引用。手写的 `require` 路径和节点引用需要自行修改；之后导出会重新生成 View 和声明。完整规则见 [UI 资产 Schema 与 Control Registry](<../Lua 与蓝图脚本/声明式 UI/资产 Schema 与 Control Registry.md>)。

## 预览行为

改动 C++ UI 代码后，先点击**编译**，再查看预览。原生运行时构建期间预览不可用。点击**刷新预览**可显示尚未刷新的编辑结果。

设计区显示资产内容，但不运行 Lua Controller。对于由代码填充的内容，请放置有代表性的占位元素，文本使用 `previewText`。`editor` 数据仅供设计时使用。

嵌套的项目资产不能在父资产中展开或编辑；修改内部内容时，需要打开原资产。

重复实例的访问方式见 [WrapBox API](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#wrapbox>)。

## 相关页面

- [Palette 控件](<Palette 控件.md>)
- [声明式 UI 编写工作流](<../Lua 与蓝图脚本/声明式 UI/编写工作流.md>)
- [声明式 UI Controller 示例](<../Lua 与蓝图脚本/声明式 UI/Controller 示例.md>)
- [UI 资产 Schema 与 Control Registry](<../Lua 与蓝图脚本/声明式 UI/资产 Schema 与 Control Registry.md>)
