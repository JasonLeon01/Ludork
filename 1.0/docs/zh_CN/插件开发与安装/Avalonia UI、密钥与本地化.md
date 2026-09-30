# Avalonia UI、密钥与本地化

## 目标

打开一个插件自有的 Avalonia 窗口，通过宿主凭据服务保存密钥，并按当前编辑器语言显示标签，同时不依赖编辑器实现类型。

## 前置条件

- 一个有效源码插件，引用 `Ludork.Plugin.Avalonia` 与宿主共享的 Avalonia 程序集。
- 已注册的菜单命令或地图项右键菜单命令。
- 随插件目录一同分发的本地化字符串资源。

## 步骤

### 取得真正的所有者窗口

```csharp
if (context.UserInterface is not IAvaloniaPluginUserInterface avaloniaUi)
{
    return PluginResult.Failed("Avalonia UI is unavailable.");
}

MyPluginWindow window = new();
await window.ShowDialog(avaloniaUi.Owner);
return PluginResult.Completed();
```

适用时把 `WindowStartupLocation` 设为 `CenterOwner`。窗口、控件、视图状态、取消处理，以及图像与其他资源的释放都由插件负责。不要在 `Application.Current` 中查找编辑器窗口，也不要把控件注入编辑器拥有的视图。

通过 `Ludork.Plugin.Avalonia.PluginTheme` 使用编辑器的公共外观：`Brush("Surface")` 或 `Color("Accent")` 读取语义颜色，`FontFamily` 提供包含中文回退的界面字体。可用颜色键为 `Background`、`Surface`、`Input`、`Hover`、`Border`、`Text`、`TextMuted`、`TextDisabled`、`Accent`、`AccentMuted`、`AccentHover`、`AccentPressed`、`TextOnAccent`、`Error`、`Warning` 和 `Success`。在编辑器应用初始化后读取这些资源，并让标准控件继承宿主主题。

`Ludork.Plugin.Avalonia.EditorBitmapEffects` 提供与编辑器预览相同的色相处理。`NormalizeHue(double)` 把角度归一化到 `[0, 360)`，非有限数以及距离整圈不超过 `0.0001` 度的偏移均视为零；`IsNeutralHue(double)` 使用相同规则。`CreateHueShiftedBitmap(Bitmap, double, CancellationToken)` 返回与源图尺寸、DPI 相同的新位图，零偏移时也返回新对象，由调用方负责释放。`ApplyHueShiftBgra(Span<byte>, int width, int height, int stride, double hue, CancellationToken)` 原地修改未预乘的 BGRA 缓冲区，保留 alpha、完全透明像素和行尾填充字节。两种处理方法都接受可选的取消令牌，统一使用 Avalonia 的双精度 HSV 转换与 RGB 最近整数舍入。颜色选择器使用相同转换，并在灰阶颜色下保留上一次色相。

`EditorZoomInput.PrimaryModifier` 与 `HasPrimaryModifier` 在 macOS 选择 Command，其他平台选择 Control。`EditorZoomAnchor.Capture(contentPoint, viewportPoint, origin, scale)` 记录内容坐标中的光标锚点；布局更新后，`Apply(scrollViewer, origin, scale)` 消费锚点，并将新偏移限制在视口范围内。scale 必须为正有限数。坐标换算、布局订阅和视口复位由调用方负责，`IsPending` 与 `Clear()` 用于检查和清除待应用状态。

`MarkdownSyntax.GetLines`、`GetInlineMatches` 与 `ReadTable` 提供公共解析，不加载资源或打开链接。`Profile.Documentation` 使用文档的图片、删除线语法和表格转义规则，`Profile.Assistant` 使用消息语法并保留非竖线转义。指定行不是表格起点时，`ReadTable` 返回 null；否则返回 `MarkdownTable`，包含 `Header`、`Alignments`、补齐或截断后的 `Rows` 及 `ConsumedLines`。控件、样式、资源校验与链接操作仍由调用方负责。

### 分离程序路径与数据路径

`PluginDirectory` 存放引入的程序内容，必须按只读目录处理。设置、缓存与历史写入 `PluginDataDirectory`。两条路径都不得从当前工作目录推导。

`Ludork.Plugin.Abstractions.FilePersistence` 提供无 BOM UTF-8 文本方法 `WriteAllTextAtomic` / `WriteAllTextAtomicAsync`，以及用于其他格式的流方法 `WriteAtomic` / `WriteAtomicAsync`。它们写入同目录临时文件、刷新到存储后替换目标，并在失败时清理临时文件。异步写入在替换前检查取消；同步 `beforeCommit` 回调在临时文件关闭后执行。业务校验和多文件事务、回滚仍由调用方负责。

`CreateTemporaryPath`、`WriteDurable`、`MoveFile`、`DeleteFile` 与 `MoveDirectory` 可用于自定义事务。`WriteDurable` 创建新文件并刷新到存储。移动和删除方法对 Windows 瞬态共享或访问失败最多重试十秒，覆盖和删除会清除目标只读属性。错误向调用方传递。

### 保存密钥

通过命令 context 中按插件隔离的 `SecretStore` 保存：

```csharp
await context.SecretStore.WriteAsync(
    "api-key",
    apiKey,
    context.CancellationToken);
string? stored = await context.SecretStore.ReadAsync(
    "api-key",
    context.CancellationToken);
```

`ContainsAsync`、`ReadAsync`、`WriteAsync` 与 `DeleteAsync` 使用宿主提供的操作系统凭据服务。返回的密钥不得复制到普通设置、诊断信息、URL 或会话日志中。逻辑 key 必须保持稳定，并且只服务于一个用途。

### 本地化插件 UI

注册期间读取 `IPluginRegistrar.EditorLanguage`，加载匹配的插件资源，并设置明确的回退语言，例如 `en_GB`。注册时，`PluginMenuCommand.Label` 与 `PluginMapContextMenuCommand.Label` 必须已经本地化。窗口标题、按钮、校验信息与诊断文本都由插件自行本地化。宿主不会翻译插件字符串。

`Ludork.Plugin.Abstractions.PluginLocalizer.LoadDirectory(pluginDirectory, language)` 读取 `locales/<language>.json`，仅在目标文件不存在时回退到整个 `en_GB.json`。`LoadCatalog(localePath, language)` 读取按语言分组的单个 JSON 对象，并逐键回退到 `en_GB`。`Text(key)` 无法解析时返回原 key，`Format(key, arguments)` 使用当前文化格式化。资源格式和语言选择均由调用方明确指定。

游戏本地化是另一套数据工作流。不要把插件自己的 locale 资源当作项目的运行时文本表。

生成 Lua 字符串时，使用 `Ludork.Plugin.Abstractions.LuaStringLiteral.Quote(string)` 或 `Append(StringBuilder, string)`。两者都生成完整的双引号字面量，转义引号、反斜杠及 ASCII 控制字符；Unicode 文本（包括 C1 控制字符）保持原文，在写入源文件时编码为 UTF-8。Lua 十进制转义表示字节，因此不得把 ASCII 范围之外的 Unicode 码点直接写成十进制转义。此共享表示可以由原生 Lua 运行时与编辑器读取器一致读取。

## 限制

`IAvaloniaPluginUserInterface` 只暴露所有者窗口，不暴露编辑器控件、服务或样式。源码插件引入器不接受零散的 XAML 源码。宿主不为已经加载的插件提供实时语言切换。

## 相关页面

- [Manifest 与最小插件](<Manifest 与最小插件.md>)
- [注册点与 Hook 参考](<注册点与 Hook 参考.md>)
- [游戏本地化工作流](<Official 插件/游戏本地化工作流.md>)
