# Palette 控件

Palette（控件面板）列出可拖入 UI 资产的内置控件，以及启用了 **Exposed** 的项目资产。本页介绍各控件的用途、子节点规则和常用属性，运行时 API 见各条目中的链接。

## 如何阅读本页

条目按 Palette 分类排列，以控件的 `controlId` 为标题。每张表列出分类、允许的子节点、Slot 类型和主要属性。

### 子节点与 Slot

`childPolicy` 与 `slotType` 分别规定容器能容纳哪些子节点，以及如何排列这些子节点：

| childPolicy | slotType | 控件 | 子节点布局 |
|---|---|---|---|
| `multiple` | `canvas` | Canvas | 锚点、偏移、对齐、自动尺寸与层叠顺序 |
| `multiple` | `list` | ListView、ScrollBox | 由父容器排列 |
| `single` | `list` | WrapBox | 由父容器排列，只允许一个模板子节点 |
| `none` | — | 其余 19 个控件 | 不接受子节点 |

Canvas Slot 定位子节点的本地边界；List Slot 不接受 Canvas Slot 字段。完整字段契约见 [UI 资产 Schema 与 Control Registry](<../Lua 与蓝图脚本/声明式 UI/资产 Schema 与 Control Registry.md#slot-与资源路径>)。

### 所有控件共有的属性

23 个控件都带 `visible`、`rotation`、`scale`、`origin`，因此下面的条目表不再列出。

PlainText、RichText、FunctionalPlainText、FunctionalRichText、TextBox 和 DropBox 还提供 `previewText`，便于在 Controller 填入实际文本前查看布局。它保存在节点的 `editor` 对象中，仅供设计时使用。

### 内联文本样式组

有 7 个控件带同一组内联文本样式属性：`textConfig`、`font`、`characterSize`、`bold`、`italic`、`underlined`、`strikeThrough`、`slantAngle`、`fillColor`、`letterSpacing`、`lineSpacing`、`lineAlignment`、`outlineColor`、`outlineThickness`，4 个 `glow*` 字段和 3 个 `gradient*` 字段。它们是 CheckBox、DropBox、FunctionalPlainText、GamepadHintBar、PlainText、TabView、TextBox。下面的条目用*文本样式*指代这一组，不再逐项列出。`textConfig` 非空时，编辑器会禁用这些内联字段，运行时改用 TextConfig。

RichText 与 FunctionalRichText 只接受 `textConfig`，没有内联字段；它们通过 TextConfig 为命名标签设定样式。

### 系统控件与项目资产

以下 23 个是内置控件。Palette 还会列出启用了 **Exposed**（`palette.exposed = true`）的项目 UI 资产。添加后可将其作为整体使用；修改内部内容时，需要打开原资产。

## Layout 控件

Layout 控件负责组织子节点，也是唯一接受子节点的一类。

### Engine.Canvas

自由定位容器，新屏幕通常以它为根。子节点由各自的 Canvas Slot 定位，而不是由父级流式排布。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Layout | 任意数量 | canvas | `size` |

Canvas 决定内部控件的坐标空间和层叠顺序。详见 [Engine：运行时值与功能控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/运行时值与功能控件.md#canvas>)。

### Engine.ListView

多子节点列表，把子节点排成行与列，并且只绘制当前可见的那些。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Layout | 任意数量 | list | `size`、`columns`、`defaultItemHeight`、`fixItemHeight` |

`columns` 默认为 1，`defaultItemHeight` 默认为 32；启用 `fixItemHeight` 后，每行使用固定高度。详见 [Engine：布局与图像控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#listview>)。

### Engine.ScrollBox

带裁剪的可滚动视口，用于容纳放不下的内容。滚动、溢出指示器与裁剪都由原生控件负责。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Layout | 任意数量 | list | `size`、`windowSkin` |

`windowSkin` 为空时加载当前系统窗口皮肤。详见 [Engine：布局与图像控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#scrollbox>)，其中也介绍了窗口皮肤溢出指示器。

### Engine.WrapBox

按等间距重复一个子节点模板，适合图标列、格子背景等。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Layout | 单个模板 | list | `size`、`count`、`spacing` |

重复项从左到右排列，空间不足时换行。编辑方式见 [UI 资产编辑器](<UI 资产编辑器.md>)，运行时访问方式见 [Engine：布局与图像控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#wrapbox>)。

## Visual 控件

Visual 控件只负责绘制，不接受输入。`ProgressBar` 也归在这一类，尽管它显示的数值会在运行时变化。

### Engine.CharacterView

在固定边界内渲染逐帧动画的角色与敌人图集。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `size`、`texture`、`textureRect`、`characterScale`、`animatable`、`switchInterval`、`shader`、`hue`、`colour` |

`textureRect` 选择初始帧，`characterScale` 调整画面大小。启用 `animatable` 并设置正数 `switchInterval` 后，会按顺序播放水平排列的等宽帧。`shader` 可以为空，也可使用完整的 `/Game/Assets/Shaders/...` 路径。详见 [Engine：布局与图像控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#characterview>)。

### Engine.EmitterView

在 UI 中显示一个 GPU 粒子发射器。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `particle`、`size`、`anchor`、`autoPlay` |

`particle` 使用 `Data/Particles` 下不带扩展名的资源键。选中效果后，可设置尺寸、锚点和自动播放。制作方法见[通用粒子](<通用粒子.md>)，代码中的播放控制见 [EmitterView](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/时间、渲染与粒子.md#emitterview>)。

### Engine.Image

一张静态图片，可拉伸或平铺。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `drawAs`、`texture`、`textureRect`、`colour` |

**Draw As** 在 Inspector 中是枚举选择器：**Image** 拉伸图片，**Tile** 重复其纹理区域以填满布局区域，并裁掉不完整的最后一块。两种模式下全局 UI Scale 都缩放完整结果。`textureRect` 可选，用于定义平铺单元。详见 [Engine：布局与图像控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#image>)。

### Engine.ProgressBar

一层背景加一层从左向右显现的填充。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `size`、`progress`、`backgroundTexture`、`fillTexture`、`backgroundTextureRect`、`fillTextureRect`、`backgroundColor`、`fillColor` |

`progress` 取值 0 到 1，超出范围会被夹紧，非有限值变为 0。两个纹理默认为空，此时是纯色层；非空值使用规范的 `/Game/Assets/...` 路径。填充按当前进度裁剪，而不是被压缩进当前宽度。每层颜色与对应图片的颜色相乘，设为白色 `[255,255,255,255]` 可保留原色。详见 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#progressbar>)。

### Engine.Rect

用窗口皮肤画出的矩形——面板、边框、选中高亮。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `size`、`windowSkin`、`opacityCurve` |

`opacityCurve` 相对于 `Data/Curves`，不带目录前缀与扩展名，解析为一条标量曲线。`texture` 为空的 Button 使用当前窗口皮肤的选中 Rect 图像。详见 [Engine：布局与图像控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#rect>)。

### Engine.SolidRect

单色填充的矩形，可带描边。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `size`、`fillColor`、`outlineColor`、`outlineThickness` |

`outlineThickness` 默认为 0，`outlineColor` 默认全透明，因此未改动的 SolidRect 就是纯色填充。详见 [Engine：布局与图像控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#solidrect>)。

### Engine.Window

带窗口皮肤的窗框——对话框、菜单、消息框的标准背景。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `size`、`windowSkin`、`repeated`、`colour` |

`repeated` 为真时背景采用纹理重复，为假时拉伸；四边始终在两个角之间平铺。`colour` 是 RGBA 染色，默认 `[255,255,255,255]`，因此半透明窗框要在 JSON 里改它的 alpha。详见 [Engine：文本与窗口](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/文本与窗口.md#window>)。

## Input 控件

Input 控件接受焦点与玩家输入。它们都是 `FunctionalBase`：只有当自身与所有 `FunctionalBase` 祖先都处于 active、且自身与所有 `ControlBase` 祖先都可见时，才接受焦点与输入。

`FunctionalImage` 是可接受输入的 Image，因此归入 Input。

### Engine.Button

可点击按钮，带悬停与按下染色，并可选绑定手柄。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `texture`、`textureRect`、`colour`、`hoverColour`、`pressedColour`、`gamepadButton`、`gamepadLongPress` |

`texture` 为空时使用当前窗口皮肤的选中 Rect 图像。默认背景和自定义纹理在所有输入设备下均保持可见；`Engine.Input.getInputHintDevice()` 选择 Gamepad 时显示绑定的手柄图标，尺寸、全局边界、命中区域与独立的文本控件都不变。`gamepadButton` 是诸如 `"X"` 的逻辑名，默认为空；`gamepadLongPress` 默认为 false。Button 不接受子节点，因此它的文字标签是另一个文本控件。详见 [Engine：运行时值与功能控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/运行时值与功能控件.md#button>)。

### Engine.CheckBox

带文字标签的复选框，可响应选中状态的变化。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`checked`、`windowSkin`、*文本样式* |

标签使用内联文本样式组或某个 `textConfig`。`toggle()` 由代码翻转状态，结果通过 `setOnCheckedChanged` 回调。详见 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#checkbox>)。

### Engine.DropBox

收起状态的单选列表，点开后显示为弹出层。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`windowSkin`、*文本样式* |

列表选项由 Controller 调用 `setItems` 设置，不保存在资产属性中。选择和音效回调见 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#dropbox>)。

### Engine.FunctionalImage

同时接受焦点与输入的 Image，也是 CharacterView 的基类。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `drawAs`、`texture`、`textureRect`、`colour` |

它保存的属性与 Image 相同，并继承其 `drawAs` API。如果不需要点击图片，就选 Image。详见 [Engine：运行时值与功能控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/运行时值与功能控件.md#functionalimage>)。

### Engine.FunctionalPlainText

同时接受焦点与输入的 PlainText。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `text`、`colour`、*文本样式* |

详见 [Engine：运行时值与功能控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/运行时值与功能控件.md#functionalplaintext>)。

### Engine.FunctionalRichText

同时接受焦点与输入的 RichText。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `textConfig`、`text`、`colour` |

详见 [Engine：运行时值与功能控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/运行时值与功能控件.md#functionalrichtext>)。

### Engine.GamepadHintBar

显示最多三条手柄提示，在自身尺寸内居中。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、*文本样式* |

接入手柄后才会显示提示。Controller 可设置最多三条 `Engine.GamepadHint`。详见 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#gamepadhintbar>)。

### Engine.Slider

通过拖动滑块选择范围内的数值。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`minValue`、`maxValue`、`value`、`lineTexture`、`handleTexture` |

`minValue` 与 `maxValue` 是整数，默认 0 与 100；两个纹理默认为 `/Game/Assets/System/SliderLine.png` 与 `/Game/Assets/System/SliderHandle.png`。编辑涵盖指针、键盘与触摸输入，变化通过 `setOnValueChanged` 上报。详见 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#slider>)。

### Engine.TabView

一排可选标签页。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`windowSkin`、`items`、*文本样式* |

`items` 是仅构造期生效的非空 `string[]`。资产通常写 `#TAB 1...` 之类的占位值，便于预览各标签；在第一帧可见之前，由 Controller 提供本地化标签文字并绑定选择行为。`selectedIndex` 与按键提示表只在运行时存在，且不接受 `tabCount`。详见 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#tabview>)。

### Engine.TextBox

供玩家输入和编辑文本。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`windowSkin`、`text`、*文本样式* |

用 `text` 设置游戏中的初始文本；`previewText` 只影响设计区预览。编辑操作和回调见 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#textbox>)。

## Text 控件

Text 控件只绘制字符串，不接受输入。如果文本自身需要获得焦点或可点击，请改用对应的 Functional 变体。

### Engine.PlainText

样式直接写在节点上的静态文本。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Text | 无 | — | `text`、`colour`、*文本样式* |

详见 [Engine：文本与窗口](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/文本与窗口.md#plaintext>)。

### Engine.RichText

静态文本，命名标签样式由 TextConfig 解析。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Text | 无 | — | `textConfig`、`text`、`colour` |

详见 [Engine：文本与窗口](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/文本与窗口.md#richtext>)。

## 如何在控件之间选择

| 要做的事 | 用哪个 |
|---|---|
| 自由定位子节点，或新建一个屏幕 | Canvas |
| 把子节点排成行与列 | ListView |
| 滚动放不下的内容 | ScrollBox |
| 按等间距重复同一个模板 | WrapBox |
| 显示一张图片，拉伸或平铺 | Image；需要接受输入时用 FunctionalImage |
| 显示逐帧动画的角色或敌人图集 | CharacterView |
| 显示粒子效果 | EmitterView |
| 显示 0 到 1 的进度 | ProgressBar |
| 画带皮肤的对话框窗框 | Window |
| 画带皮肤的面板或选中高亮 | Rect |
| 画纯色块 | SolidRect |
| 显示静态文本 | PlainText；需要命名标签样式时用 RichText |
| 显示需要获得焦点的文本 | FunctionalPlainText 或 FunctionalRichText |
| 接受一次点击 | Button |
| 接受是/否选择 | CheckBox |
| 从列表中接受一个选择 | DropBox |
| 接受范围内的一个数值 | Slider |
| 接受键入的文本 | TextBox |
| 在多个页面之间切换 | TabView |
| 显示手柄按键提示 | GamepadHintBar |

## 仅供代码使用的类型

`Engine.SectorShape` 和 `Engine.AssetInstance` 可在代码中使用，但不是内置 Palette 控件。API 见[交互控件参考](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md>)。已启用 Exposed 的项目资产以自己的名称出现在 Palette 中，生成的 View 通过 `.instance` 提供对运行时资产的访问。

## 相关页面

- [UI 资产编辑器](<UI 资产编辑器.md>)
- [UI 资产 Schema 与 Control Registry](<../Lua 与蓝图脚本/声明式 UI/资产 Schema 与 Control Registry.md>)
- [声明式 UI 编写工作流](<../Lua 与蓝图脚本/声明式 UI/编写工作流.md>)
- [Engine：布局与图像控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md>)
- [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md>)
- [原生 UI Adapter](<../C++ 原生开发/原生 UI Adapter.md>)
