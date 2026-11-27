# UI 资产 Schema

声明式 UI 资产是存放在 `Data/UI` 下的 JSON 文档，用于定义控件层级、布局 Slot、动画时间轴与静态属性。本页规定资产 Schema、资源键规则以及各类控件的 JSON 序列化契约。

## 资产标识与结构

UI 资产声明 `type = "uiAsset"`，不含 Schema 版本字段，且拒绝任何未知字段。资产的唯一标识是其相对于 `Data/UI` 的精确大小写路径，使用正斜杠分隔并省略扩展名（例如 `Title.json` 的键为 `Title`，`Parts/Shared/WindowChrome.json` 的键为 `Parts/Shared/WindowChrome`）。

- **键名限制**：资产键不得为绝对路径，不得包含反斜杠、空路径段、`.`、`..`、文件扩展名或重复前缀（`Assets/`、`UI/`、`Data/UI/`）。每个路径段均不得包含点号，以确保生成的 Lua 模块具有无歧义的 `require` 路径。支持空格、连字符与 Unicode 名称。
- **顶层字段**：根文档包含 `designSize`、`palette` 与单个 `root` 节点，以及可选的 `animations` 数组。资产与节点均不存储 UUID。
- **系统控件 ID**：系统控件必须使用绑定类的规范运行时路径（如 `Engine.Canvas` 或 `Engine.FunctionalPlainText`）；其他任何值都会被判定为未知控件而拒绝。
- **嵌套项目资产**：嵌套资产表现为黑盒——父节点仅声明项目 `controlId`、`name` 与父级 `slot`，不包含内部覆盖或 `children`。被引用的资产必须启用 `palette.exposed = true`，且禁止直接或间接循环嵌套。

每个节点遵循以下 Schema：

| 字段 | 契约 |
|---|---|
| `name` | 资产内节点标识、生成引用键与原生查找名，在资产内唯一；完整标识为 `(assetKey, name)`。 |
| `controlId` | 系统控件的规范模块根原生类路径，或 `Project:<relative-key>`。 |
| `properties` | 由控件 Schema 拥有的静态值。 |
| `slot` | 由父级拥有的布局属性；根节点省略该字段。 |
| `editor` | 仅用于设计的预览值，例如 `previewText`。 |
| `children` | 有序子节点列表；z-order 相同时按列表顺序决出先后。 |

## 动画时间轴

资产可包含 `animations` 数组，用于定义线性关键帧时间轴：

- **条目字段**：每条动画需指定非空 `name`（同一目标内唯一）、必填 `target`（全局动画填 `null`，资产内节点动画填节点名）、正数 `duration`、可选归一化 `pivot`（默认 `[0.5, 0.5]`）以及 `tracks` 对象。`target` 可指向直接嵌套的资产节点，但绝不能指向嵌套资产内部的节点。
- **轨道与关键帧**：`tracks` 支持 `translation`、`rotation`、`scale` 与 `colour`。每条存在的轨道均为非空菱形关键帧数组，元素格式为 `{ "time": number, "value": ... }`，时间必须有限、严格递增且位于 `0..duration` 区间内。向量使用两个有限数字（`scale` 不得为负），`colour` 使用四个 `0..255` 整数 RGBA 通道并独立插值。缺失轨道按恒等变换处理；单关键帧轨道保持常量；超出关键帧时间范围时保持首帧或末帧值；插值方式为线性。
- **变换合成**：`translation` 叠加布局后的局部偏移，`rotation` 叠加局部角度增量，`scale` 与原有或布局缩放相乘，`colour` 与子树原有的 RGBA 通道相乘。变换围绕目标的局部边界与 `pivot` 合成，不改动布局位置、旋转、缩放、原点或 Slot 数据，避免重新布局时产生偏移累积。渲染、绝对边界与命中测试共享同一合成变换，`Canvas` 在其合成边界处对子树颜色仅应用一次。
- **嵌套与编辑器维护**：嵌套资产仅导出全局动画；父资产针对该嵌套节点的同名动画会覆盖该实例（参见[动画播放](<运行时契约.md#动画播放>)）。在编辑器中，重命名节点会同步更新动画目标，删除节点会移除指向其子树的动画，复制节点会连带复制动画并将目标重定向到副本。

## Slot 与资源路径

父级的 Adapter 决定子项策略与 Slot 类型：Canvas 父级提供锚点、偏移、对齐、自动尺寸与 z-order，List 父级提供列表排布并拒绝 Canvas Slot 字段。

- **Canvas Slot 边界与文本对齐**：Canvas Slot 按控件的局部边界定位，包含非零的局部边界偏移。文本的 `lineAlignment` 控制文本边界内部的行对齐，Slot 的 `alignment` 控制该边界在父级中的摆放位置——因此右对齐文本搭配右对齐、自动尺寸的 Slot 时，文本宽度变化会向左扩展而保持右边缘不动。
- **`Engine.ScrollBox`**：使用 List Slot 的多子控件布局，序列化 `size` 与 `windowSkin`。原生控件负责滚动、指示器与裁剪。
- **`Engine.WrapBox`**：使用 List Slot 的单模板布局控件，序列化 `size`（可用布局边界）、`count`（非负整数，默认 `1`）与 `spacing`（二维数值向量，默认 `[0, 0]`）。唯一子节点使用 `slot: {}`，可包含子树。重复实例从左向右排布并按宽度换行，仅在排布步长为正时允许负间距。空模板或 `count = 0` 不生成重复控件。节点名保持资产内唯一，生成的 View 引用止于 WrapBox 边界；运行时通过从 1 开始的 `get(i)` API 访问各实例。

资源引用须严格遵守以下规范格式：

- **资产文件**：大小写精确、以 `/Game/Assets/` 开头、使用正斜杠并带扩展名的逻辑路径。
- **`textConfig`**：相对于 `Data/TextConfigs`、不含目录前缀与扩展名的路径。
- **`opacityCurve`**：相对于 `Data/Curves`、不含目录前缀与扩展名的标量曲线路径。
- **禁止格式**：原生绝对路径、`Assets/...`、类别相对路径、反斜杠、路径穿越段与别名前缀均无效。

普通文本控件直接将字体、字号、样式、间距、对齐、描边、辉光与渐变保存在节点上。共享或特殊文本样式可选用 `textConfig`；当 `textConfig` 非空时，编辑器会禁用内联样式字段，运行时改用该 TextConfig。富文本也可引用 TextConfig 以定义具名标签样式。

## 控件序列化契约

本节规定专用控件的 JSON 字段契约与运行时类型要求。通用控件用途、子项策略与完整 Inspector 属性见 [Palette 控件](<../../编辑器用户指南/Palette 控件.md>)。仅设置了 `palette.exposed = true` 的资产会显示为项目控件。

| 控件 | 序列化字段与默认值 | 说明与运行时 API |
|---|---|---|
| `Engine.ListView` | `childScrollingEnabled`（布尔值，默认 `false`）。 | 亦可在运行时赋值。参见 [ListView](<../全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#listview>)。 |
| `Engine.Window` | `size`、`windowSkin`、`repeated`、`colour`（RGBA，默认 `[255, 255, 255, 255]`）。 | 需要半透明窗口边框时在 JSON 中降低 `colour` 的 Alpha 通道。 |
| `Engine.Image`、`Engine.FunctionalImage` | `drawAs`：`"Image"`（默认）或 `"Tile"`。 | 运行时设置方法要求 `Engine.ImageDrawAs`。参见 [Image](<../全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#image>)。 |
| `Engine.EmitterView` | `particle`（`Data/Particles` 下无扩展名键，默认 `""`）、`size`（`[100, 100]`）、`anchor`（`[0.5, 0.5]`）、`autoPlay`（`true`）。 | 不接受子节点；沿用父级 Slot、排序与裁剪。GPU 粒子在所属 Canvas 的逻辑坐标域中运行，通过 `getEmitter()` 控制播放。隐藏或未挂载时冻结，释放时停止播放，切换 Canvas 时重置坐标域。参见[通用粒子](<../../编辑器用户指南/通用粒子.md>)。 |
| `Engine.Button` | `texture`、可选 `textureRect`、`colour`、`hoverColour`、`pressedColour`、`gamepadButton`（如 `"X"` 的逻辑名称，默认 `""`）、`gamepadLongPress`（`false`）。 | 参见 [Button](<../全局与 Core 模块/Core Modules/Engine/运行时值与功能控件.md#button>)。 |
| `Engine.ProgressBar` | `size`、`progress`、`backgroundColor`、`fillColor`、`backgroundTexture`（`""`）、`fillTexture`（`""`）、可选 `backgroundTextureRect`、`fillTextureRect`。 | 纹理为空时绘制纯色，非空时使用 `/Game/Assets/...` 路径。可选矩形采用源图像素 `[x, y, width, height]`（`null` 或省略表示整图）。背景铺满控件，填充层按进度从左向右裁剪；颜色与对应层相乘（`[255, 255, 255, 255]` 保持原色）。参见 [ProgressBar](<../全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#progressbar>)。 |
| `Engine.TextBox` | `size`、`windowSkin`、`text`、标准纯文本属性与仅限编辑器的 `previewText`。 | 不接受子节点。参见 [TextBox](<../全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#textbox>)。 |
| `Engine.TabView` | `size`、`windowSkin`、仅构造期生效的非空 `items: string[]`，以及内联文本样式或可选 `textConfig`。 | 无子节点与自身 Slot。首个可见帧前由 Controller 提供本地化标签并绑定选择行为（参见 [TabView](<../全局与 Core 模块/Core Modules/Engine/交互与声明式 UI 控件.md#tabview>)）。`selectedIndex` 与按键提示表仅属运行时；拒绝 `tabCount`。 |
| `Engine.CharacterView` | `size`、`texture`、可选 `textureRect`、`characterScale`、`animatable`、`switchInterval`、`material`（`""` 或 `Data/Materials/...json`）、`materialParameters`（字符串键纯数据字典）、`colour`。 | 不接受子节点。自身负责裁剪、动画、Actor 缩放适配、材质绘制与固定边界。参见 [CharacterView](<../全局与 Core 模块/Core Modules/Engine/布局与图像控件.md#characterview>)。 |
