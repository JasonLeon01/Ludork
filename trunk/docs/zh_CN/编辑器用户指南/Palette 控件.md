# Palette 控件

查阅 UI 资产编辑器 Palette 中全部内置控件的用途、子节点与 Slot 规则、序列化属性及运行时 API 参考。

## 如何阅读本页

条目按 Palette 分类组织，并以写入 UI 资产 JSON 的精确 `controlId` 为标题。表格列出各控件的子节点策略、Slot 类型及控件特有的序列化属性键（中文界面下 Inspector 显示**颜色**、**填充颜色**、**描边颜色**、**光晕颜色**与**背景颜色**等本地化标签，资产 JSON 与代码仍使用下表中的精确键名）。

### 子节点与 Slot

`childPolicy` 与 `slotType` 决定容器可接受的子节点数量及排布方式：

| `childPolicy` | `slotType` | 控件 | 子节点布局 |
|---|---|---|---|
| `multiple` | `canvas` | `Canvas` | 锚点、偏移、对齐、自动尺寸与层叠顺序 |
| `multiple` | `list` | `ListView`、`ScrollBox` | 由父容器管理的列表排布 |
| `single` | `list` | `WrapBox` | 单个模板子节点的列表排布 |
| `none` | — | 其余 19 个控件 | 不接受子节点 |

Canvas Slot 定位子节点的本地边界；List Slot 拒绝 Canvas Slot 字段。详见 [Slot 与资源路径](<../Lua 与蓝图脚本/声明式 UI/资产 Schema 与 Control Registry.md#slot-与资源路径>)。

### 所有控件共有的属性

全部 23 个内置控件均包含 `visible`、`rotation`、`scale` 与 `origin`，下文条目表中不再重复列出。6 个文本控件（`PlainText`、`RichText`、`FunctionalPlainText`、`FunctionalRichText`、`TextBox` 与 `DropBox`）还在 `editor` 中保存仅编辑器使用的 `previewText`，便于在 Controller 赋值前于 Designer 中预览排版。

### 内联文本样式组

7 个控件（`CheckBox`、`DropBox`、`FunctionalPlainText`、`GamepadHintBar`、`PlainText`、`TabView` 与 `TextBox`）共用同一组内联文本样式属性：`textConfig`、`font`、`characterSize`、`bold`、`italic`、`underlined`、`strikeThrough`、`slantAngle`、`fillColor`、`letterSpacing`、`lineSpacing`、`lineAlignment`、`outlineColor`、`outlineThickness`，4 个 `glow*` 字段与 3 个 `gradient*` 字段。

下表以*文本样式*代称该属性组。当 `textConfig` 非空时，编辑器会禁用内联字段，运行时改用对应文字配置。

`RichText` 与 `FunctionalRichText` 仅接受 `textConfig`，并通过其中的命名标签控制样式。

### 系统控件与项目资产

以下 23 个控件为引擎内置控件。启用了 **Exposed**（`palette.exposed = true`）的项目 UI 资产也会作为可复用组件出现在 Palette 中，修改其内部结构须打开源资产。

## Layout 控件

Layout 控件用于组织子节点，也是唯一接受子节点的控件分类。

### Engine.Canvas

自由定位容器，新界面的标准根节点。子节点通过各自的 Canvas Slot 定位，而非自动流式排布。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Layout | 任意数量 | `canvas` | `size` |

定义其子内容的坐标空间与绘制顺序。API：[Canvas](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/运行时值与功能控件.md#canvas>)。

### Engine.ListView

多子节点网格／列表容器，按行列排布子节点并仅绘制当前可见项。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Layout | 任意数量 | `list` | `size`、`columns`、`defaultItemHeight`、`fixItemHeight`、`childScrollingEnabled` |

默认值：`columns = 1`、`defaultItemHeight = 32`、`childScrollingEnabled = false`（在 Inspector 中开启可令超宽子项在列内滚动）。启用 `fixItemHeight` 可强制统一行高。API：[ListView](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#listview>)。

### Engine.ScrollBox

带裁剪的可滚动视口，由原生控件负责滚动、裁剪与窗口皮肤溢出指示器。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Layout | 任意数量 | `list` | `size`、`windowSkin` |

`windowSkin` 留空时加载当前系统窗口皮肤。API：[ScrollBox](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#scrollbox>)。

### Engine.WrapBox

按等间距重复渲染单个子模板，适合状态图标、网格与单元格背景。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Layout | 单个模板 | `list` | `size`、`count`、`spacing` |

子项从左向右排列并在超出可用宽度时自动换行。模板编辑见 [UI 资产编辑器](<UI 资产编辑器.md>)，运行时访问见 [WrapBox](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#wrapbox>)。

## Visual 控件

Visual 控件仅负责绘制内容，不接受焦点或输入。

### Engine.CharacterView

在固定边界内渲染逐帧动画的角色或敌人精灵表。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `size`、`texture`、`textureRect`、`characterScale`、`animatable`、`switchInterval`、`material`、`materialParameters`、`colour` |

`textureRect` 选定初始帧，`characterScale` 控制缩放；启用 `animatable` 并设置正数 `switchInterval` 后，按水平顺序循环播放等宽帧。

`material` 填写完整的 `Data/Materials/...json` 路径（留空使用默认材质），`materialParameters` 保存实例覆盖值并在更换材质时清空，可通过 Inspector 中的材质参数控件编辑。API：[CharacterView](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#characterview>)。

### Engine.EmitterView

在 UI 布局中渲染 GPU 粒子发射器。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `particle`、`size`、`anchor`、`autoPlay` |

`particle` 为 `Data/Particles` 下不含扩展名的相对键。资产制作见[通用粒子](<通用粒子.md>)，运行时播放控制见 [EmitterView](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/时间、渲染与粒子.md#emitterview>)。

### Engine.Image

支持拉伸或平铺绘制的静态图片。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `drawAs`、`texture`、`textureRect`、`colour` |

在 Inspector 中将 **Draw As** 设为 **Image**（拉伸）或 **Tile**（在布局区域内重复平铺 `textureRect`，末尾不完整块自动裁剪）；全局 UI 缩放作用于最终合成结果。API：[Image](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#image>)。

### Engine.ProgressBar

由背景层与自左向右显现的填充层组成的进度条。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `size`、`progress`、`backgroundTexture`、`fillTexture`、`backgroundTextureRect`、`fillTextureRect`、`backgroundColor`、`fillColor` |

`progress` 夹紧至 `[0, 1]`（非有限值视为 `0`），按进度裁剪填充而非压扁图像。纹理留空时绘制纯色层，非空值使用规范的 `/Game/Assets/...` 路径。各层颜色对对应纹理染色（`[255, 255, 255, 255]` 保留原色）。API：[ProgressBar](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#progressbar>)。

### Engine.Rect

使用窗口皮肤绘制的矩形，常用于面板、边框与选中高亮。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `size`、`windowSkin`、`opacityCurve` |

`opacityCurve` 是相对于 `Data/Curves` 且不含扩展名的标量曲线键。API：[Rect](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#rect>)。

### Engine.SolidRect

带可选描边的纯色矩形。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `size`、`fillColor`、`outlineColor`、`outlineThickness` |

默认 `outlineThickness = 0` 且 `outlineColor` 全透明，未配置描边时仅绘制纯色填充。API：[SolidRect](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#solidrect>)。

### Engine.Window

采用窗口皮肤的九宫格窗框，适合作为对话框、菜单与消息框的标准背景。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `size`、`windowSkin`、`repeated`、`colour` |

`repeated` 控制背景平铺（`true`）或拉伸（`false`），四条边框始终在四角之间平铺。`colour` 默认为 `[255, 255, 255, 255]`，调低其 Alpha 通道可实现半透明窗框。API：[Window](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/文本与窗口.md#window>)。

## Input 控件

Input 控件继承自 `FunctionalBase`，仅当自身与全部 `FunctionalBase` 祖先均处于激活状态，且自身与全部 `ControlBase` 祖先均可见时，才响应焦点与玩家输入。

### Engine.Button

支持悬停／按下染色与可选手柄提示绑定的可点击按钮。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `texture`、`textureRect`、`colour`、`hoverColour`、`pressedColour`、`gamepadButton`、`gamepadLongPress` |

`texture` 留空时回退至当前窗口皮肤的选中矩形图像；背景在所有输入设备下均保持可见。按钮不接受子节点，文字标签请使用独立的文本控件。

当 `Engine.Input.getInputHintDevice()` 返回手柄时，绑定的 `gamepadButton` 图标（逻辑按键名如 `"X"`，默认 `""`；`gamepadLongPress` 默认 `false`）会自动叠加显示，不改变尺寸、边界、命中区域或同级文本控件。API：[Button](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/运行时值与功能控件.md#button>)。

### Engine.CheckBox

自带文本标签并在状态变化时触发回调的双态复选框。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`checked`、`windowSkin`、*文本样式* |

代码中可调用 `toggle()` 切换状态，并绑定 `setOnCheckedChanged` 监听变化。API：[CheckBox](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#checkbox>)。

### Engine.DropBox

可展开弹出列表的收起式单选下拉框。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`windowSkin`、*文本样式* |

选项需在运行时由 Controller 调用 `setItems` 填充，不保存在 UI 资产中。API：[DropBox](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#dropbox>)。

### Engine.FunctionalImage

可聚焦、可点击的 `Image`，也是 `CharacterView` 的基类。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `drawAs`、`texture`、`textureRect`、`colour` |

与 `Image` 共用序列化属性与 `drawAs` 行为；无需点击或焦点交互时请选用 `Engine.Image`。API：[FunctionalImage](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/运行时值与功能控件.md#functionalimage>)。

### Engine.FunctionalPlainText

可聚焦、可点击的 `PlainText`。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `text`、`colour`、*文本样式* |

API：[FunctionalPlainText](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/运行时值与功能控件.md#functionalplaintext>)。

### Engine.FunctionalRichText

可聚焦、可点击的 `RichText`。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `textConfig`、`text`、`colour` |

API：[FunctionalRichText](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/运行时值与功能控件.md#functionalrichtext>)。

### Engine.GamepadHintBar

手柄接入时居中显示最多三条由 Controller 提供的 `Engine.GamepadHint` 提示。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、*文本样式* |

API：[GamepadHintBar](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#gamepadhintbar>)。

### Engine.Slider

支持指针、键盘与触摸操作的整数范围滑块。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`minValue`、`maxValue`、`value`、`lineTexture`、`handleTexture` |

默认值：`minValue = 0`、`maxValue = 100`、`lineTexture = "/Game/Assets/System/SliderLine.png"`、`handleTexture = "/Game/Assets/System/SliderHandle.png"`。通过 `setOnValueChanged` 监听数值变更。API：[Slider](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#slider>)。

### Engine.TabView

横向排列的可选标签栏。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`windowSkin`、`items`、*文本样式* |

`items` 为仅构造期生效的非空 `string[]`；须在首帧显示前由 Controller 提供本地化标签文本并绑定选中回调。`selectedIndex` 与按键提示表仅存在于运行时，`tabCount` 不属于资产属性。

详见[控件序列化契约](<../Lua 与蓝图脚本/声明式 UI/资产 Schema 与 Control Registry.md#控件序列化契约>)与 [TabView](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#tabview>)。

### Engine.TextBox

可编辑的单行文本输入框。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`windowSkin`、`text`、*文本样式* |

`text` 定义运行时初始内容，`previewText` 仅在 Designer 中预览。API：[TextBox](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#textbox>)。

## Text 控件

Text 控件仅渲染静态文本，不接受焦点或输入；需要交互时请改用对应的 Functional 变体。

### Engine.PlainText

直接在节点上配置样式或通过文字配置染色的静态文本。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Text | 无 | — | `text`、`colour`、*文本样式* |

API：[PlainText](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/文本与窗口.md#plaintext>)。

### Engine.RichText

通过文字配置解析命名富文本标签的静态文本。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Text | 无 | — | `textConfig`、`text`、`colour` |

API：[RichText](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/文本与窗口.md#richtext>)。

## 仅供代码使用的类型

`Engine.SectorShape` 与 `Engine.AssetInstance` 仅可在代码中使用，不作为内置控件出现在 Palette 中（见[交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md>)）。启用了 **Exposed** 的项目 UI 资产以自身名称显示在 Palette 中，其生成的 View 通过 `.instance` 暴露底层运行时资产。
