# Palette 控件

Palette 列出所有可以拖入 UI 资产的内容：先是引擎内建控件，然后是项目自身已 Exposed 的资产。本页讲内建控件——每个控件做什么、能否放子节点、子节点用哪种 Slot、优先关注哪些属性。运行时 API 只在各条目里给出链接，不在本页重复。

## 如何阅读本页

每个条目的标题就是该控件的 `controlId`，也就是写进节点 `controlId` 字段的原值。条目按 Palette 自身的分类分组，各带一张表：

| 列 | 含义 |
|---|---|
| 分类 | 控件在 Palette 中所属的分组 |
| 子节点 | `childPolicy`，即控件能否接受子节点、能接受几个 |
| Slot | `slotType`，即控件为每个子节点提供哪种 Slot |
| 关键属性 | 控件自身的序列化属性，不含所有控件共有的那几项 |

### 子节点与 Slot

`childPolicy` 与 `slotType` 来自控件的原生 Adapter，两者共同决定什么能放进哪里：

| childPolicy | slotType | 控件 | 每个子节点得到什么 |
|---|---|---|---|
| `multiple` | `canvas` | Canvas | 锚点、偏移、对齐、自动尺寸与 z-order |
| `multiple` | `list` | ListView、ScrollBox | 由父级掌握的列表摆放 |
| `single` | `list` | WrapBox | 列表摆放，且只允许一个模板子节点 |
| `none` | — | 其余 19 个控件 | 无，它们不接受子节点 |

Canvas Slot 定位子节点的本地边界；List Slot 不接受 Canvas Slot 字段。完整字段契约见 [UI 资产 Schema 与 Control Registry](<../Lua 与蓝图脚本/声明式 UI/资产 Schema 与 Control Registry.md#slot-与资源-key>)。

### 所有控件共有的属性

23 个控件都带 `visible`、`rotation`、`scale`、`origin`，因此下面的条目表不再列出。

其中 6 个承载文本的控件——PlainText、RichText、FunctionalPlainText、FunctionalRichText、TextBox、DropBox——还带一个仅编辑器可见的 `previewText`。Designer 显示它，好在 Controller 填入真实文本之前就能看清排版。它存放在节点的 `editor` 对象里，只用于设计期。

### 内联文本样式组

有 7 个控件带同一组内联文本样式属性：`textConfig`、`font`、`characterSize`、`bold`、`italic`、`underlined`、`strikeThrough`、`slantAngle`、`fillColor`、`letterSpacing`、`lineSpacing`、`lineAlignment`、`outlineColor`、`outlineThickness`，4 个 `glow*` 字段和 3 个 `gradient*` 字段。它们是 CheckBox、DropBox、FunctionalPlainText、GamepadHintBar、PlainText、TabView、TextBox。下面的条目用*文本样式*指代这一组，不再逐项列出。`textConfig` 非空时，编辑器会禁用这些内联字段，运行时改用 TextConfig。

RichText 与 FunctionalRichText 只接受 `textConfig`，没有内联字段；它们通过 TextConfig 为具名标签设定样式。

### 系统控件与项目资产

下面 23 个都是系统控件。编辑器从当前项目编译出的 `EditorCache/UiPreview.registry.json` 读取它们的原生 descriptor。Palette 还会列出项目控件，即 `palette.exposed = true` 的 UI 资产。嵌套的项目资产是黑盒：它的节点只声明项目 `controlId`、`name` 和父级 Slot，不带内部覆盖，也不带子节点。

## Layout 控件

Layout 控件负责组织子节点，也是唯一接受子节点的一类。

### Engine.Canvas

自由定位容器，新屏幕通常以它为根。子节点由各自的 Canvas Slot 定位，而不是由父级流式排布。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Layout | 任意数量 | canvas | `size` |

Canvas 还掌握子树的 z-order 与动画列表，在自身合成边界处对子树颜色只应用一次，并为其中的 GPU 粒子定义逻辑坐标域。详见 [Engine：运行时值与功能控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/运行时值与功能控件.md#canvas>)。

### Engine.ListView

多子节点列表，把子节点排成行与列，并且只绘制当前可见的那些。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Layout | 任意数量 | list | `size`、`columns`、`defaultItemHeight`、`fixItemHeight` |

`columns` 默认为 1，`defaultItemHeight` 默认为 32，`fixItemHeight` 让每一行都固定为该高度。不可见的 ListView 会跳过绘制，且在该次绘制中不做布局准备。详见 [Engine：布局与图像控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#listview>)。

### Engine.ScrollBox

带裁剪的可滚动视口，用于容纳放不下的内容。滚动、溢出指示器与裁剪都由原生控件负责。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Layout | 任意数量 | list | `size`、`windowSkin` |

`windowSkin` 为空时加载当前系统窗口皮肤。详见 [Engine：布局与图像控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#scrollbox>)，该页同时讲窗口皮肤溢出指示器。

### Engine.WrapBox

按等间距重复同一份子节点模板——状态火焰、图标列、格子背景。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Layout | 单个模板 | list | `size`、`count`、`spacing` |

Designer 显示全部重复项；选中并编辑任一重复项即编辑同一模板，保存时只保留那份编写的子节点。项目从左到右排列，下一个会超出可用宽度时换行，格子尺寸取自已布局的模板。生成的引用止于模板边界，因此运行时要用 1-based 的 `get(i)` 取重复实例。详见 [Engine：布局与图像控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#wrapbox>)。

## Visual 控件

Visual 控件只负责绘制，不接受输入。`ProgressBar` 也归在这一类，尽管它显示的数值会在运行时变化。

### Engine.CharacterView

在固定边界内渲染逐帧动画的角色与敌人图集。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `size`、`texture`、`textureRect`、`characterScale`、`animatable`、`switchInterval`、`shader`、`hue`、`colour` |

`textureRect` 选定初始帧，`characterScale` 在帧被居中并按不放大方式适配之前生效。`animatable` 为真且 `switchInterval` 为正时，按等宽水平帧推进；该值非正则暂停动画。`shader` 为空，或是完整的 `/Game/Assets/Shaders/...` 逻辑路径。裁剪、动画、Actor 缩放适配、Shader 与 Hue 合成以及边界都由控件自身负责。它不是输入控件。详见 [Engine：布局与图像控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#characterview>)。

### Engine.EmitterView

在 UI 中显示一个 GPU 粒子发射器。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Visual | 无 | — | `particle`、`size`、`anchor`、`autoPlay` |

`particle` 是 `Data/Particles` 下的无扩展名 key，默认为空。该视图沿用父级已有的 Slot、顺序与裁剪，其粒子使用所在 Canvas 的逻辑坐标域；`getEmitter()` 向代码开放播放控制。隐藏或卸载的视图会冻结，销毁会释放播放，切换 Canvas 会重置坐标域。详见 [Engine：时间、渲染与粒子](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/时间、渲染与粒子.md#emitterview>) 与 [通用粒子](<通用粒子.md>)。

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

`progress` 取值 0 到 1，超出范围会被夹紧，非有限值变为 0。两个纹理默认为空，此时是纯色层；非空值使用规范的 `/Game/Assets/...` 路径。填充按当前进度裁剪，而不是被压缩进当前宽度。颜色各自染色所在层，因此 `[255,255,255,255]` 可保留图片原色。详见 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#progressbar>)。

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

`repeated` 为真时背景采用纹理重复，为假时拉伸；四边始终在两个角之间平铺。`colour` 是 RGBA 染色，默认 `[255,255,255,255]`，因此半透明窗框要在 JSON 里改它的 alpha。Window 在创建、改尺寸或换皮肤时把皮肤合成为一张缓存纹理。详见 [Engine：文本与窗口](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/文本与窗口.md#window>)。

## Input 控件

Input 控件接受焦点与玩家输入。它们都是 `FunctionalBase`：只有当自身与所有 `FunctionalBase` 祖先都处于 active、且自身与所有 `ControlBase` 祖先都可见时，才接受焦点与输入。

`FunctionalImage` 归在这一类而不在 Visual 下：它是同时接受输入的 Image。

### Engine.Button

可点击按钮，带悬停与按下染色，并可选绑定手柄。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `texture`、`textureRect`、`colour`、`hoverColour`、`pressedColour`、`gamepadButton`、`gamepadLongPress` |

`texture` 为空时使用当前窗口皮肤的选中 Rect 图像。接入手柄期间该默认背景隐藏，但尺寸、全局边界、命中区域与独立的文本控件都不变；自定义纹理始终可见，断开手柄后默认背景恢复。`gamepadButton` 是诸如 `"X"` 的逻辑名，默认为空；`gamepadLongPress` 默认为 false。Button 不接受子节点，因此它的文字标签是另一个文本控件。详见 [Engine：运行时值与功能控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/运行时值与功能控件.md#button>)。

### Engine.CheckBox

两态开关，自绘标签并上报变化。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`checked`、`windowSkin`、*文本样式* |

标签使用内联文本样式组或某个 `textConfig`。`toggle()` 由代码翻转状态，结果通过 `setOnCheckedChanged` 回调。详见 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#checkbox>)。

### Engine.DropBox

收起状态的单选列表，点开后显示为弹出层。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`windowSkin`、*文本样式* |

它的条目、选中索引与展开状态都是运行时值，不是序列化属性：由 Controller 调用 `setItems` 并读取选择回调。开启、光标、选中、取消四种音效按控件单独设置。详见 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#dropbox>)，该页还讲弹出层、测量、滚动、选择回调与移动端触摸确认。

### Engine.FunctionalImage

同时接受焦点与输入的 Image，也是 CharacterView 的基类。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `drawAs`、`texture`、`textureRect`、`colour` |

它序列化与 Image 相同的属性，并继承其 `drawAs` API。如果不需要点击图片，就选 Image。详见 [Engine：运行时值与功能控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/运行时值与功能控件.md#functionalimage>)。

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

只有接入手柄时才绘制并响应。每条提示是一个 `Engine.GamepadHint`，含 `Button`（`Engine.JoystickButton` 的 getter 值）、`LongPress` 标志与 `Text`；`setHints` 拒绝超过三条。默认窗口通过共享的 `WindowChrome` 资产把它放在下边框，长按进度用 `SectorShape` 绘制。详见 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#gamepadhintbar>)。

### Engine.Slider

在范围内拖动的数值。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`minValue`、`maxValue`、`value`、`lineTexture`、`handleTexture` |

`minValue` 与 `maxValue` 是整数，默认 0 与 100；两个纹理默认为 `/Game/Assets/System/SliderLine.png` 与 `/Game/Assets/System/SliderHandle.png`。编辑涵盖指针、键盘与触摸输入，变化通过 `setOnValueChanged` 上报。详见 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#slider>)。

### Engine.TabView

一排可选标签页。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`windowSkin`、`items`、*文本样式* |

`items` 是仅构造期生效的非空 `string[]`。资产通常写 `#TAB 1...` 之类的占位值，好让原生预览显示出每个标签；在第一帧可见之前，由 Controller 提供本地化标签文字并绑定选择行为。`selectedIndex` 与按键提示表只在运行时存在，且不接受 `tabCount`。详见 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#tabview>)。

### Engine.TextBox

可编辑的单字段文本输入。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Input | 无 | — | `size`、`windowSkin`、`text`、*文本样式* |

编辑通过 `beginEdit`、`finishEdit`、`cancelEdit` 开始与结束，并通过 `setOnTextChanged`、`setOnEditingChanged` 上报；`setInputDialogLabels` 设置屏幕上的确认标签。它的 `previewText` 仅编辑器可见，因此实际交付的是编写的 `text`。详见 [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#textbox>)。

## Text 控件

Text 控件只绘制字符串，不接受输入。如果文本自身需要可获得焦点或可点击，请改用对应的 Functional 变体。

### Engine.PlainText

样式直接写在节点上的静态文本。

| 分类 | 子节点 | Slot | 关键属性 |
|---|---|---|---|
| Text | 无 | — | `text`、`colour`、*文本样式* |

详见 [Engine：文本与窗口](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/文本与窗口.md#plaintext>)。

### Engine.RichText

静态文本，具名标签样式由 TextConfig 解析。

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
| 显示静态文本 | PlainText；需要具名标签样式时用 RichText |
| 显示需要可获得焦点的文本 | FunctionalPlainText 或 FunctionalRichText |
| 接受一次点击 | Button |
| 接受是/否选择 | CheckBox |
| 从列表中接受一个选择 | DropBox |
| 接受范围内的一个数值 | Slider |
| 接受键入的文本 | TextBox |
| 在多个页面之间切换 | TabView |
| 显示手柄按键提示 | GamepadHintBar |

## 不在 Palette 中

有两个 `Engine` 类型出现在交互控件参考里，但不是 Palette 条目：

- `Engine.SectorShape` 是基础图形，不是 UI 控件。它从 12 点方向顺时针填充一个扇形，角度夹紧到 0–360 度；`GamepadHintBar` 与 `Button` 用它绘制长按进度。
- `Engine.AssetInstance` 是嵌套项目资产在运行时的对象，生成的 View 以 `.instance` 暴露它。Palette 提供的是那个已 Exposed 的资产，代码拿到的是它的 instance。

项目资产只有自身 `palette.exposed` 为 `true` 时才出现在 Palette 中。

## 相关页面

- [UI 资产编辑器](<UI 资产编辑器.md>)
- [UI 资产 Schema 与 Control Registry](<../Lua 与蓝图脚本/声明式 UI/资产 Schema 与 Control Registry.md>)
- [声明式 UI 编写工作流](<../Lua 与蓝图脚本/声明式 UI/编写工作流.md>)
- [Engine：布局与图像控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/布局与图像控件.md>)
- [Engine：交互与声明式 UI 控件](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md>)
- [原生 UI Adapter](<../C++ 原生开发/原生 UI Adapter.md>)
