# Avalonia UI、密钥与本地化

在不依赖编辑器内部实现类型的前提下，打开插件自有的 Avalonia 窗口、通过宿主密钥存储安全保存凭据，并按当前编辑器语言呈现本地化界面文本。建议从引用了 `Ludork.Plugin.Avalonia` 与宿主共享 Avalonia 程序集的[带界面模板](<Manifest 与最小插件.md#自动生成模板>)出发，注册一条菜单命令或地图项右键菜单命令来打开窗口。

## 设置父窗口

将 `context.UserInterface` 转换为 `IAvaloniaPluginUserInterface`，即可把编辑器父窗口传入 `ShowDialog`：

```csharp
if (context.UserInterface is not IAvaloniaPluginUserInterface avaloniaUi)
{
    return PluginResult.Failed("Avalonia UI is unavailable.");
}

MyPluginWindow window = new();
await window.ShowDialog(avaloniaUi.Owner);
return PluginResult.Completed();
```

需要居中显示时可将 `WindowStartupLocation` 设为 `CenterOwner`。窗口、控件、视图状态、取消逻辑以及位图等原生资源的释放均由插件自行负责。请勿通过 `Application.Current` 查找编辑器窗口，也不要向编辑器自有视图注入控件。

### 宿主主题与共享 UI 工具

- **主题资源（`Ludork.Plugin.Avalonia.PluginTheme`）**：通过 `Brush("Surface")` 或 `Color("Accent")` 读取语义颜色，通过 `FontFamily` 获取带中文回退的界面字体。可用键包括 `Background`、`Surface`、`Input`、`Hover`、`Border`、`Text`、`TextMuted`、`TextDisabled`、`Accent`、`AccentMuted`、`AccentHover`、`AccentPressed`、`TextOnAccent`、`Error`、`Warning` 与 `Success`。仅在编辑器应用初始化完成后读取这些资源，并让标准控件继承宿主主题。
- **色相归一化（`Ludork.Plugin.Avalonia.EditorBitmapEffects`）**：`NormalizeHue(double)` 将角度归一化到 `[0, 360)`，并将非有限值或距整圈不超过 `0.0001` 度的偏移视为零（`IsNeutralHue(double)` 遵循同一规则）。
- **位图与 BGRA 色相偏移**：`CreateHueShiftedBitmap(Bitmap, double, CancellationToken)` 返回与源图尺寸和 DPI 相同的新位图，即便偏移为零也会创建新对象（调用方负责释放）。`ApplyHueShiftBgra(Span<byte>, int width, int height, int stride, double hue, CancellationToken)` 原地修改未预乘的 BGRA 缓冲区，保留 Alpha、完全透明像素与行尾填充字节。两者均接受可选取消令牌，使用 Avalonia 双精度 HSV 转换与 RGB 最近整数舍入（与颜色选择器保持一致，灰阶颜色保留上次色相）。
- **缩放锚定（`EditorZoomInput` / `EditorZoomAnchor`）**：`EditorZoomInput.PrimaryModifier` 与 `HasPrimaryModifier` 在 macOS 上映射为 Command，在其他平台上映射为 Control。`EditorZoomAnchor.Capture(contentPoint, viewportPoint, origin, scale)` 在内容坐标系记录指针锚点，布局完成后由 `Apply(scrollViewer, origin, scale)` 消费并将偏移钳制到视口范围内（`scale` 必须为正有限数）。坐标换算、布局事件订阅与视口复位由调用方负责；`IsPending` 与 `Clear()` 用于查询与清除待应用状态。
- **Markdown 语法解析（`MarkdownSyntax`）**：`GetLines`、`GetInlineMatches` 与 `ReadTable` 提供共享解析逻辑，不加载资源也不打开链接。传入 `Profile.Documentation` 可启用文档图片、删除线与表格转义规则；传入 `Profile.Assistant` 适用于助手消息并保留非竖线转义。若指定行并非表格起始行，`ReadTable` 返回 `null`；否则返回的 `MarkdownTable` 包含 `Header`、`Alignments`、已补齐或截断的 `Rows` 以及 `ConsumedLines`。渲染控件、样式、资源校验与链接打开仍由插件自行负责。

## 分离程序路径与数据路径

请将 `PluginDirectory`（引入的程序内容）严格视为只读目录，将所有设置、缓存与历史写入 `PluginDataDirectory`。两条路径均自动对应插件当前生效的安装作用域（全局共享数据或项目隔离数据），切勿从进程当前工作目录推导路径。

- **原子文件辅助方法（`Ludork.Plugin.Abstractions.FilePersistence`）**：`WriteAllTextAtomic` 与 `WriteAllTextAtomicAsync` 写入无 BOM 的 UTF-8 文本；`WriteAtomic` 与 `WriteAtomicAsync` 用于基于流的其他格式。各方法均先写入同目录临时文件、刷盘后再替换目标，失败时自动清理临时文件。异步重载会在替换前检查取消信号；同步 `beforeCommit` 回调在临时文件关闭后运行。业务校验、多文件事务与回滚仍由调用方负责。
- **自定义事务原语**：`CreateTemporaryPath`、`WriteDurable`（创建新文件并刷盘）、`MoveFile`、`DeleteFile` 与 `MoveDirectory` 用于构建自定义原子流程。移动与删除方法遇到 Windows 瞬态共享或访问失败时最多重试 10 秒，覆盖或删除前会自动清除目标只读属性，并将最终错误传播给调用方。
- **并行文件批处理（`Ludork.Plugin.Abstractions.FileBatch`）**：`Run` 默认最多使用 `min(inputs.Count, Environment.ProcessorCount)` 个工作线程并发处理独立任务，按输入顺序返回结果并支持取消；带状态的重载还支持限制最大并发数（`maxWorkers`），并为每个工作线程独立创建与释放状态。
- **Lua 源码辅助工具（`Ludork.Plugin.Abstractions.LuaSourceTokens`、`LuaIdentifier`）**：`LuaSourceTokens.Read(source, token, lineComment)` 对 Lua 源码分词，并通过可选回调以零起始半开区间 `[start, end)` 报告每条单行短注释（自 `--` 起至换行符或文本末尾，忽略长注释与字符串字面量）。`LuaIdentifier.IsValid(name)` 校验单个名称是否为合法的非保留字 Lua 标识符。

## 保存密钥

请通过命令上下文中按插件隔离的 `SecretStore` 存取密钥：

```csharp
await context.SecretStore.WriteAsync(
    "api-key",
    apiKey,
    context.CancellationToken);
string? stored = await context.SecretStore.ReadAsync(
    "api-key",
    context.CancellationToken);
```

- **平台后端**：`ContainsAsync`、`ReadAsync`、`WriteAsync` 与 `DeleteAsync` 在 Windows 上使用凭据管理器、在 macOS 上使用钥匙串，绝不退化为明文文件。在 Linux 上，读取操作从环境变量获取，写入与删除操作会抛出包含所需环境变量名的异常（全局插件为 `LUDORK_PLUGIN_SECRET_<hash>`，项目插件为 `LUDORK_PROJECT_PLUGIN_SECRET_<hash>`）。
- **作用域隔离**：全局凭据按插件 ID 隔离；项目凭据额外纳入项目作用域，因此不同项目间相同的插件 ID 与密钥名称不会冲突。
- **安全使用规则**：保持逻辑密钥名稳定且一钥一用；切勿将读取出的密钥复制到设置文件、诊断信息、URL 或会话日志中。

## 本地化插件 UI

在 `Register` 阶段读取 `IPluginRegistrar.EditorLanguage`，加载随插件目录分发的对应语言资源，并配置明确的回退语言（如 `en_GB`）。`PluginMenuCommand.Label` 与 `PluginMapContextMenuCommand.Label` 必须在注册前完成本地化；窗口标题、按钮、校验提示与诊断文本同样由插件自行本地化——宿主不会翻译插件字符串。

- **`Ludork.Plugin.Abstractions.PluginLocalizer`**：`LoadDirectory(pluginDirectory, language)` 读取 `locales/<language>.json`，仅当目标语言文件缺失时整体回退至 `en_GB.json`。`LoadCatalog(localePath, language)` 读取单个按语言键分组的 JSON 对象，并逐键回退至 `en_GB`。`Text(key)` 在键缺失时返回原键，`Format(key, arguments)` 按当前区域性规则格式化。
- **与游戏本地化分离**：游戏本地化属于独立的项目数据工作流，切勿将插件的 UI 语言文件复用为游戏的运行时文本目录。
- **生成 Lua 字符串（`Ludork.Plugin.Abstractions.LuaStringLiteral`）**：使用 `Quote(string)` 或 `Append(StringBuilder, string)` 输出完整的双引号 Lua 字面量，自动转义引号、反斜杠与 ASCII 控制字符，同时将 Unicode 文本（含 C1 控制字符）原样保留为 UTF-8。由于 Lua 十进制转义按字节而非 Unicode 码点编码，切勿将非 ASCII 码点直接转换为十进制转义序列。

> **注意**：`IAvaloniaPluginUserInterface` 仅暴露父窗口，不暴露任何编辑器控件、服务或样式。插件引入器会拒绝零散 XAML 文件，且宿主不支持对已加载插件实时切换界面语言。
