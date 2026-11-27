# Engine：交互与声明式 UI 控件

本页记录交互式 UI 控件（`TextBox`、`CheckBox`、`DropBox`、`TabView`、`GamepadHintBar`、`ProgressBar`、`Slider`）、扇形基础图元（`SectorShape`）以及声明式 UI 实例化句柄（`AssetInstance`）。实例方法表省略隐式 `self` 接收者（蓝图节点将其公开为首个目标引脚，未连接时默认指向图所属对象）；静态构造函数（`new`）无接收者。

## TextBox

**继承**：`Engine.ControlBase`、`Engine.FunctionalBase`

单行 UTF-8 文本输入控件，初始处于非编辑状态。点击、轻触或按下确认键进入编辑模式；仅获得焦点不会启动编辑。

- **桌面端编辑**：支持闪烁光标、文本选区、水平滚动、按字素（grapheme）移动与删除、Home/End 键及剪贴板快捷键。Windows 上内嵌模式与独立窗口均支持字符输入与输入法（IME）组合；桥接连接断开时按失焦规则处理已提交文本。
- **确认与取消**：按下 Enter 或调用 `finishEdit` 提交编辑后的文本；按下 Escape、已连接手柄的 B/取消键或调用 `cancelEdit` 恢复进入编辑前的取值（两者均不会触发表单提交）。失去焦点时保留已提交文本并丢弃未完成的输入法组合。隐藏、禁用、解绑或销毁控件均会结束编辑。同一时间仅允许一个 `TextBox` 处于编辑状态。
- **移动端与 HarmonyOS**：iOS、Android 及 HarmonyOS 手机与平板弹出系统输入对话框（HarmonyOS 二合一设备沿用桌面端编辑）。原生“完成”提交文本，“取消”恢复打开对话框前的取值；应用切入后台时关闭输入会话，返回前台后不自动重新打开。
- **回调与文本清理**：`setOnTextChanged` 在已提交值变化时触发（含 `setString` 与取消恢复），输入法组合阶段不会改变 `getString()`。换行符与制表符转换为空格，其他控制字符会被移除。表单校验由 Controller 负责。

### 函数与事件

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `new` | `size: sf.Vector2f`, `windowSkin: sf.Image`, `textConfig: Engine.PlainTextConfig`, `text: string = ""` | `Engine.TextBox` | 静态方法。 |
| `getString` | — | `string` | — |
| `setString` | `text: string` | — | — |
| `isEditing` | — | `bool` | — |
| `beginEdit` | — | `bool` | — |
| `finishEdit` | — | — | — |
| `cancelEdit` | — | — | — |
| `setInputDialogLabels` | `title: string`, `done: string`, `cancel: string` | — | — |
| `setOnTextChanged` | `callback: function(text: string) \| nil` | — | — |
| `setOnEditingChanged` | `callback: function(editing: bool) \| nil` | — | — |
| `getSize` | — | `sf.Vector2f` | — |
| `resize` | `size: sf.Vector2f` | — | — |
| `setWindowSkin` | `windowSkin: sf.Image` | — | — |
| `setTextConfig` | `textConfig: Engine.PlainTextConfig` | — | — |

## CheckBox

**继承**：`Engine.ControlBase`、`Engine.FunctionalBase`

使用窗口皮肤与纯文本配置绘制的二态复选框控件。

### 函数与事件

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `getSize` | — | `sf.Vector2f` | 纯函数（Pure）。 |
| `resize` | `size: sf.Vector2f` | — | — |
| `isChecked` | — | `bool` | 纯函数（Pure）。 |
| `setChecked` | `checked: bool` | — | — |
| `toggle` | — | — | — |
| `setWindowSkin` | `windowSkin: sf.Image` | — | — |
| `setTextConfig` | `textConfig: Engine.PlainTextConfig` | — | — |
| `setOnCheckedChanged` | `callback: function` | — | — |
| `getLocalBounds` | — | `sf.FloatRect` | 纯函数（Pure）。 |
| `update` | `deltaTime: float` | — | — |
| `onConfirm` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onClick` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onMouseButtonDown` | `arguments: Engine.UiInputEventArguments` | `bool` | — |
| `onKeyDown` | `arguments: Engine.UiInputEventArguments` | — | — |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## DropBox

**继承**：`Engine.ControlBase`、`Engine.FunctionalBase`

下拉选择控件，收起时显示按钮，展开时绘制可滚动的浮层选项列表。

### 浮层与尺寸计算

- **浮层绘制**：存在 `Canvas` 祖先时，展开列表绘制在最外层祖先 `Canvas` 的浮层（overlay）上、位于常规内容与动画之后，因此不受嵌套 `Canvas` 裁剪影响，也不会改变父布局中其他控件的尺寸或位置。
- **展开方向与高度**：列表可见高度限制在根 `Canvas` 内容矩形与游戏屏幕的交集内。完整列表能向下放下时向下展开，否则若能向上放下则向上展开；两侧均放不下时选择空间更大的一侧并启用内部滚动。
- **边界**：`getSize()` 始终返回收起状态的布局尺寸。`getLocalBounds()` 在收起时返回该矩形，展开时返回其与可见弹窗的并集（向上展开可能使局部 Y 坐标为负）。继承的绝对边界与默认触摸命中边界随之同步。

### 弹窗滚动与选择

普通鼠标滚轮按行高步进滚动，精确滚轮与移动端/Web 触摸拖动则直接应用偏移量。滚动仅移动弹窗视口，不会改变光标或已提交选项，松开拖动也不会确认条目。

方向键移动光标并将其滚入可见范围。鼠标悬停预览可见条目，单击即确认；移动端与 Web 轻触采用两段式确认。所有索引均指向完整的 `items` 数组。

### 触摸确认

在移动端与 Web 端，展开的 `DropBox` 采用两段式确认：轻触未高亮条目仅移动高亮并播放光标音效，不改变 `selectedIndex` 也不触发选择回调；轻触当前高亮条目（含初始高亮项）才确认选择。取消操作会丢弃预览并保持原 `selectedIndex`。

### 选择回调

- `setOnSelectedIndexChanged(callback)` 仅在 `setSelectedIndex` 产生不同选项时携带新索引触发；确认过程中先于确认回调运行。
- `setOnSelectionConfirmed(callback)` 在展开且非空的 `DropBox` 确认时携带当前索引触发（即使索引未变化）。代码设值、替换条目、取消或空列表确认不会触发此回调。
- 向任一 setter 传入 `nil` 可清除对应回调。

### 函数与事件

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `getSize` | — | `sf.Vector2f` | 纯函数（Pure）。 |
| `getLocalBounds` | — | `sf.FloatRect` | 纯函数（Pure）。 |
| `resize` | `size: sf.Vector2f` | — | — |
| `setWindowSkin` | `windowSkin: sf.Image`, `repeated: bool = false` | — | — |
| `setTextConfig` | `textConfig: Engine.PlainTextConfig` | — | — |
| `getItems` | — | `string[]` | 纯函数（Pure）。 |
| `setItems` | `items: string[]` | — | — |
| `getSelectedIndex` | — | `int` | 纯函数（Pure）。 |
| `setSelectedIndex` | `index: int` | — | — |
| `getSelectedItem` | — | `string` | 纯函数（Pure）。 |
| `isExpanded` | — | `bool` | 纯函数（Pure）。 |
| `setExpanded` | `expanded: bool` | — | — |
| `open` | — | — | — |
| `cancel` | — | — | — |
| `setOnSelectedIndexChanged` | `callback: function` | — | — |
| `setOnSelectionConfirmed` | `callback: function` | — | — |
| `setOnExpandedChanged` | `callback: function` | — | — |
| `setOpenSound` | `filename: string` | — | — |
| `getOpenSound` | — | `string` | 纯函数（Pure）。 |
| `setCursorSound` | `filename: string` | — | — |
| `getCursorSound` | — | `string` | 纯函数（Pure）。 |
| `setSelectSound` | `filename: string` | — | — |
| `getSelectSound` | — | `string` | 纯函数（Pure）。 |
| `setCancelSound` | `filename: string` | — | — |
| `getCancelSound` | — | `string` | 纯函数（Pure）。 |
| `update` | `deltaTime: float` | — | — |
| `onConfirm` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onCancel` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onClick` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onMouseButtonDown` | `arguments: Engine.UiInputEventArguments` | `bool` | — |
| `onMouseMoved` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onMouseWheelScrolled` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onKeyDown` | `arguments: Engine.UiInputEventArguments` | — | — |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## TabView

**继承**：`Engine.ControlBase`、`Engine.FunctionalBase`

固定页签数量的水平分页条，支持可选的键盘/手柄边缘提示且不获取键盘焦点。

### 选择与导航

- **布局与构造**：`TabView` 拥有固定的非零条目数、从零开始且自动限幅的索引以及始终可见的选中项。中间区域等分各页签槽位，左右两侧各预留 `16` 逻辑像素用于显示按键提示。该控件永不获取键盘焦点。声明式资产依据必填非空属性 `items: string[]` 确定固定数量（`tabCount` 不是资产属性）。
- **更新条目与切换选中**：数量一致时 `setItems(items)` 替换文本标签而不改变选中项或回调（数量不匹配时保持原状）。`setOnSelectedIndexChanged(callback)` 设置或清除（传 `nil`）回调。`setSelectedIndex` 发生实际变化时静默调用回调；`selectPrevious`、`selectNext` 以及有效的 Q/E、LB/RB、鼠标移动或触摸轻触先播放一次光标音效再调用回调。未发生变化或到达边界时均不播放音效也不触发回调，且页签不循环首尾衔接。
- **输入处理**：持有焦点的业务内容调用 `handleNavigationInput()`，在 `TabView` 可见且活跃时消费 Q、E、LB 与 RB（同向重复输入只移动一次，反向输入互相抵消）。鼠标移入中间槽位即切换选中，左键点击该区域会被消费但不重复触发选择，右键、中键及两侧提示区事件照常透传。触摸轻触可切换页签，触摸拖动不切换。

### 按键提示

`setKeyHint(leftHint, rightHint)` 接收两个 `Engine.KeyHint`，可选字段为 `Keyboard`（非 Unknown 的 `sf.Keyboard.Key`）与 `Joystick`（`Engine.JoystickButton` getter 返回值）。空提示清除对应侧，缺少当前设备字段则隐藏，非法输入保持两侧原状。

```lua
tabView:setKeyHint(
    Engine.KeyHint.new({ Keyboard = sf.Keyboard.Key.Q, Joystick = Engine.JoystickButton.getLB() }),
    Engine.KeyHint.new({ Keyboard = sf.Keyboard.Key.E, Joystick = Engine.JoystickButton.getRB() })
)
```

仅在对应方向可继续切换时显示提示：连接手柄时显示 `Joystick`，否则桌面端、HarmonyOS 二合一设备与 HarmonyOS 平板显示 `Keyboard`，Android、iOS 及 HarmonyOS 手机隐藏提示。按键提示不会重绑定或禁用输入。

### 函数与事件

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `getSize` | — | `sf.Vector2f` | 纯函数（Pure）。 |
| `resize` | `size: sf.Vector2f` | — | — |
| `setWindowSkin` | `windowSkin: sf.Image` | — | — |
| `setTextConfig` | `textConfig: Engine.PlainTextConfig` | — | — |
| `getItems` | — | `string[]` | 纯函数（Pure）。 |
| `setItems` | `items: string[]` | — | — |
| `setOnSelectedIndexChanged` | `callback: function \| nil` | — | — |
| `getSelectedIndex` | — | `int` | 纯函数（Pure）。 |
| `setSelectedIndex` | `index: int` | — | — |
| `getSelectedItem` | — | `string` | 纯函数（Pure）。 |
| `selectPrevious` | — | `bool` | — |
| `selectNext` | — | `bool` | — |
| `handleNavigationInput` | — | `bool` | — |
| `setCursorSound` | `filename: string` | — | — |
| `getCursorSound` | — | `string` | 纯函数（Pure）。 |
| `setKeyHint` | `leftHint: Engine.KeyHint`, `rightHint: Engine.KeyHint` | — | — |
| `getLocalBounds` | — | `sf.FloatRect` | 纯函数（Pure）。 |
| `update` | `deltaTime: float` | — | — |
| `onClick` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onMouseButtonDown` | `arguments: Engine.UiInputEventArguments` | `bool` | — |
| `onMouseMoved` | `arguments: Engine.UiInputEventArguments` | — | — |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## GamepadHintBar

**继承**：`Engine.ControlBase`、`Engine.FunctionalBase`

连接手柄时居中显示最多 3 项手柄按键提示（`setHints` 超过 3 项时拒绝）。每项为 `Engine.GamepadHint`，包含 `Button`（`Engine.JoystickButton` getter 返回值）、`LongPress` 与 `Text`。默认窗口通过共享 `WindowChrome` 资产将其置于底部边框，由 `WindowBase` 绑定回调。

短按提示在 `16` 逻辑像素的白色圆盘上绘制当前手柄字形并附带标签文本；长按提示额外绘制在 1 秒内顺时针填充的绿色扇形，满 1 秒时以从零开始的索引触发一次 `setOnHintTriggered`（中途松开则重置进度）。`setHintEnabled` 可置灰指定提示、停止其进度并阻止触发。

### 属性

除 `size` 与纯文本配置外不声明额外可编辑属性。

### 函数与事件

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `getSize` | — | `sf.Vector2f` | 纯函数（Pure）。 |
| `resize` | `size: sf.Vector2f` | — | — |
| `setTextConfig` | `textConfig: Engine.PlainTextConfig` | — | — |
| `setHints` | `hints: Engine.GamepadHint[]` | — | — |
| `getHintCount` | — | `int` | 纯函数（Pure）。 |
| `setHintEnabled` | `index: int`, `enabled: bool` | — | — |
| `isHintEnabled` | `index: int` | `bool` | 纯函数（Pure）。 |
| `isGamepadConnected` | — | `bool` | 纯函数（Pure）。 |
| `setOnHintTriggered` | `callback: function \| nil` | — | — |
| `getLocalBounds` | — | `sf.FloatRect` | 纯函数（Pure）。 |
| `update` | `deltaTime: float` | — | — |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## SectorShape

**继承**：`sf.Drawable`、`sf.Transformable`

基础图形图元（不属于 UI 控件，不出现在 Palette 中），从 12 点钟方向顺时针填充扇形，角度限幅在 `0–360` 度之间。`GamepadHintBar` 与 `Button` 使用该图元绘制长按进度指示器。

### 函数与事件

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `new` | `radius: float = 0.0`, `fillColour: sf.Color = white` | `Engine.SectorShape` | 静态方法。 |
| `getRadius` | — | `float` | 纯函数（Pure）。 |
| `setRadius` | `radius: float` | — | — |
| `getAngle` | — | `sf.Angle` | 纯函数（Pure）。 |
| `setAngle` | `angle: sf.Angle` | — | — |
| `getFillColour` | — | `sf.Color` | 纯函数（Pure）。 |
| `setFillColour` | `colour: sf.Color` | — | — |
| `getLocalBounds` | — | `sf.FloatRect` | 纯函数（Pure）。 |
| `getGlobalBounds` | — | `sf.FloatRect` | 纯函数（Pure）。 |

## ProgressBar

**继承**：`Engine.ControlBase`

水平进度条控件，包含独立的背景层与填充层，各层均可使用纯色或裁剪纹理。

- **进度范围**：取值范围为 `0` 到 `1`，越界值自动限幅，非有限值归零。背景铺满整个控件，填充层按比例裁剪显示左侧区域（不压缩变形）。显示缩放与表现透明度同时作用于两层。
- **纹理与着色**：两层各自持有 `sf.Texture`，传入 `nil` 清除纹理并恢复纯色绘制。纹理矩形采用源图像像素坐标（`nil` 表示完整纹理）。各层颜色与纹理相乘（白色保持原色）。更换或清除纹理不会改变控件尺寸与当前进度。

### 函数与事件

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `getSize` | — | `sf.Vector2f` | 纯函数（Pure）。 |
| `resize` | `size: sf.Vector2f` | — | — |
| `getProgress` | — | `float` | 纯函数（Pure）。 |
| `setProgress` | `progress: float` | — | — |
| `getBackgroundColor` | — | `sf.Color` | 纯函数（Pure）。 |
| `setBackgroundColor` | `color: sf.Color` | — | — |
| `getFillColor` | — | `sf.Color` | 纯函数（Pure）。 |
| `setFillColor` | `color: sf.Color` | — | — |
| `getBackgroundTexture` | — | `sf.Texture \| nil` | 纯函数（Pure）。 |
| `setBackgroundTexture` | `texture: sf.Texture \| nil` | — | — |
| `getFillTexture` | — | `sf.Texture \| nil` | 纯函数（Pure）。 |
| `setFillTexture` | `texture: sf.Texture \| nil` | — | — |
| `getBackgroundTextureRect` | — | `sf.IntRect \| nil` | 纯函数（Pure）。 |
| `setBackgroundTextureRect` | `rect: sf.IntRect \| nil` | — | — |
| `getFillTextureRect` | — | `sf.IntRect \| nil` | 纯函数（Pure）。 |
| `setFillTextureRect` | `rect: sf.IntRect \| nil` | — | — |
| `getLocalBounds` | — | `sf.FloatRect` | 纯函数（Pure）。 |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## Slider

**继承**：`Engine.ControlBase`、`Engine.FunctionalBase`

支持自定义轨道与滑块纹理的整数范围滑条控件。

### 触摸交互

捕获触摸后，`Slider` 依据指针 X 坐标相对于可视边界的位置实时更新并限幅取值，即使拖动超出命中区域仍持续响应直至松手。`setTouchHitBounds` 仅调整触摸命中区，不影响视觉或鼠标边界。

### 函数与事件

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `setVisible` | `visible: bool` | — | — |
| `getSize` | — | `sf.Vector2f` | 纯函数（Pure）。 |
| `resize` | `size: sf.Vector2f` | — | — |
| `getValue` | — | `int` | 纯函数（Pure）。 |
| `setValue` | `value: int` | — | — |
| `setRange` | `minValue: int`, `maxValue: int` | — | — |
| `getRange` | — | `minimum: int`, `maximum: int` | 纯函数（Pure）。 |
| `setValueFromRatio` | `ratio: float` | — | — |
| `setValueFromBoundsPosition` | `bounds: sf.FloatRect`, `position: sf.Vector2f` | — | — |
| `adjust` | `delta: int` | — | — |
| `getHandlePosition` | — | `int` | 纯函数（Pure）。 |
| `setLineTexture` | `texture: sf.Texture` | — | — |
| `setHandleTexture` | `texture: sf.Texture` | — | — |
| `setOnValueChanged` | `callback: function` | — | — |
| `getLocalBounds` | — | `sf.FloatRect` | 纯函数（Pure）。 |
| `update` | `deltaTime: float` | — | — |
| `onClick` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onMouseButtonDown` | `arguments: Engine.UiInputEventArguments` | `bool` | — |
| `onMouseMoved` | `arguments: Engine.UiInputEventArguments` | — | — |
| `onKeyDown` | `arguments: Engine.UiInputEventArguments` | — | — |
| `draw` | `target: sf.RenderTarget`, `states: sf.RenderStates` | — | — |

## AssetInstance

通过生成的声明式 UI View 的 `.instance` 字段访问的原生实例句柄。原生 `requireAsset` 返回 `Engine.AssetInstance`，而 `UiController:requireAsset` 返回生成的子 View；详见[声明式 UI 运行时契约](<../../../声明式 UI/运行时契约.md>)。

- **强类型属性**：`setProperty` 要求传入对应属性声明的运行时类型——例如 `drawAs` 必须传入 `Engine.ImageDrawAs.Image` 或 `Engine.ImageDrawAs.Tile`（拒绝 JSON 字符串）。`WrapBox` 模板构成独立的实例边界，访问重复生成的子节点应通过 `wrapBox:get(i)`，而非在外层资产中查找模板节点名。
- **局部重排**：`reflowControl(localName, logicalSize)` 为指定的本地系统控件应用正值 `sf.Vector2u` 尺寸并在保持其位置的前提下重排其子层级。调用不会跨越嵌套资产边界（请改为对子实例调用 `reflowControl`）；该覆盖尺寸会一直保持，直至父级重排重新应用原始 Slot 布局。

### 函数与事件

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `getRoot` | — | `Engine.ControlBase` | 纯函数（Pure）。 |
| `requireControl` | `localName: string` | `Engine.ControlBase` | 纯函数（Pure）。 |
| `getNodeByName` | `name: string` | `Engine.ControlBase` | 纯函数（Pure）。 |
| `requireAsset` | `localName: string` | `Engine.AssetInstance` | 纯函数（Pure）。 |
| `setProperty` | `localName: string`, `propertyId: string`, `value: Union[nil, bool, int, float, string, sf.Vector2f, sf.Vector2u, sf.IntRect, sf.Color, Engine.ImageDrawAs, string[]]` | — | — |
| `setText` | `localName: string`, `text: string` | — | — |
| `reflow` | `logicalSize: sf.Vector2u = nil` | — | — |
| `reflowControl` | `localName: string`, `logicalSize: sf.Vector2u` | — | — |
| `hasAnimation` | `name: string`, `target: string = nil` | `bool` | 纯函数（Pure）。 |
| `playAnimation` | `name: string`, `target: string = nil`, `onFinished: function = nil` | `bool` | — |
| `stopAnimation` | `name: string`, `target: string = nil` | — | — |
