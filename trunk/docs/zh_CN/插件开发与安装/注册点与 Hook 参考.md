# 注册点与 Hook 参考

本页汇总 `IPluginRegistrar` 提供的所有注册方法、命令上下文、宿主桥接接口与生命周期 Hook。所有注册操作只能在 `IEditorPlugin.Register` 中完成；留空的 `Register` 方法（如新建模板默认生成）同样合法。

## 作用域与执行顺序

- **生效作用域**：启动页仅加载全局插件；项目窗口会将全局插件与当前项目插件合并生效，覆盖普通菜单、地图右键菜单、文本提示、Export／Run／Pack Hook 与导出参与者指纹。每个窗口都使用自身项目对应的注册，即使另一个项目窗口已获得焦点。
- **来源优先级**：在各自作用域内，工作目录与用户目录的注册记录按插件 ID 合并，工作目录副本优先；仅有被选中的副本会提供命令、提供器与 Hook。
- **排序与唯一性**：提供器与 Hook 按“先全局插件、后项目插件”的顺序执行；菜单项按菜单位置与 `Order` 排序，排序值相同时全局命令在前。插件 ID 与命令 ID 必须在有效的“全局＋当前项目”集合内唯一——普通菜单命令与地图项右键菜单命令共用同一个命令 ID 命名空间。发生冲突时注册失败且不会覆盖已有注册。不同项目作用域之间允许复用 ID，且单个项目的加载失败不会影响全局插件或其他项目插件。

## 菜单命令

`RegisterMenuCommand(PluginMenuCommand)` 将命令添加到 `File`、`Edit`、`Game`、`Database`、`Help` 或 `Plugins` 菜单。命令 ID 必须非空且在有效集合内唯一，`Order` 决定命令在菜单内的排序位置。

| `PluginMenuCommand` 字段 | 含义 |
|---|---|
| `Id` | 非空命令 ID，在有效集合内唯一 |
| `Location` | 上述六个菜单位置之一 |
| `Order` | 菜单内的数值排序键 |
| `Label` | 已本地化的显示文本 |
| `Handler` | 异步 `PluginMenuHandler` 委托 |

传入 `Handler` 的 `PluginMenuContext` 提供可选的项目路径、编辑器语言、只读插件目录、可写插件数据目录、消息 UI、文本提示刷新触发器、按插件隔离的密钥存储、可选的蓝图助手与资源清理宿主桥，以及 `CancellationToken`。引用 `Ludork.Plugin.Avalonia` 的源码插件可将 `UserInterface` 转换为 `IAvaloniaPluginUserInterface` 以获取真实的父窗口。成功时返回 `PluginResult.Completed()`，已处理的失败返回 `PluginResult.Failed(error)`。

## 地图项右键菜单命令

`RegisterMapContextMenuCommand(PluginMapContextMenuCommand)` 在用户右键点击地图列表中的地图项时，将插件命令追加到内置命令之后（右键点击地图列表空白处时不显示）。命令 ID 与普通菜单命令共用唯一命名空间，`Order` 决定插件命令排序，`Label` 为已本地化的显示文本。

| `PluginMapContextMenuCommand` 字段 | 含义 |
|---|---|
| `Id` | 非空命令 ID，在有效集合内唯一 |
| `Order` | 数值排序键 |
| `Label` | 已本地化的显示文本 |
| `Handler` | 异步 `PluginMapContextMenuHandler` 委托 |

`PluginMapContextMenuContext` 提供被右键点击的 `MapKey`、项目路径、编辑器语言、插件目录、消息 UI、文本提示刷新触发器、密钥存储、`CancellationToken` 与 `MapEditorHost`，返回值同为 `PluginResult`。

### `IMapEditorHost` 契约

`IMapEditorHost` 是严格受限的读写边界——插件无法获取 `ProjectDataStore`、编辑器 ViewModel、`MapPanel`、宿主控件或可变编辑器 JSON。

| `IMapEditorHost` 成员 | 含义 |
|---|---|
| `ProjectPath` | 已打开项目的根目录 |
| `CellSize` | 来自已编译引擎声明的只读源格子尺寸，与预览缩放无关 |
| `SuggestedMapKey` | 打开插件时对应的地图键（可用时） |
| `ListMaps()` | 返回含键名与显示名的有序地图摘要列表 |
| `ReadMap(mapKey)` | 当前内存地图的不可变快照 |
| `ReadTileset(tilesetKey)` | 不可变的图块集快照 |
| `ListActorBlueprints()` | Actor 蓝图摘要（`Key` 为规范类引用，`DisplayName` 为项目蓝图键） |
| `ReadActorBlueprintPreview(blueprintReference)` | 已有 Actor 蓝图在 `(0, 0)` 格的解析后默认外观 |
| `AcquireActorPreview(snapshot)` | 获取解析后 Actor 快照的原生材质预览句柄（`IPluginActorPreview`） |
| `ResolveAssetFile(assetPath)` | 在最终编辑器文件 I/O 阶段解析逻辑资产路径；不可用时返回空字符串 |
| `GetSaveDirectoryAsync(cancellationToken)` | 返回项目绝对用户存档目录路径，且不创建目录 |
| `ReplaceLayerAndSaveAsync(request, cancellationToken)` | 经 revision 校验的图层替换与单地图直接保存 |
| `ReplaceActorsInRegionAndSaveAsync(request, cancellationToken)` | 经 revision 校验的指定图层矩形区域内 Actor 摆放替换 |
| `RunProjectScriptAsync(scriptPath, request, cancellationToken)` | 使用当前项目的游戏运行时执行 Lua 入口脚本并返回 JSON 对象结果 |

`GetSaveDirectoryAsync` 直接从已保存的 `Scripts/Entry.lua` 读取静态 `APP_NAME`，并遵循[桌面游戏目录规则](<../编辑器用户指南/项目设置与资产布局.md#游戏配置>)。它既不执行 Lua 也不创建目录；若应用名不可读或不合法、用户主目录不可用或操作被取消，调用将失败。

### 地图、图块集与 Actor 快照

- **地图与通行快照（`PluginMapSnapshot`）**：暴露地图标识、尺寸、不透明的 `Revision`（写入请求须原样回传）、带 `Visible` 标志的有序图层、矩形 `Tiles` 与 `AutoTiles`，以及与地图等大的 `Passage` 矩阵。每个 `PluginMapPassage` 提供地形 `Passable` 与综合可见地形层、自动图块、地图边界及两端方向限制解析出的 `Up`、`Right`、`Down`、`Left` 移动权限；Actor 占位独立处理。
- **图块集与资产路径**：图块集快照包含图块尺寸、总数与通行性。图块集与 Actor 的 `AssetPath` 保持大小写精确的 `/Game/Assets/...` 逻辑路径——仅在执行最终原生文件 I/O 时调用 `ResolveAssetFile`，切勿在快照或中间缓存中使用。
- **Actor 快照（`PluginMapActorSnapshot`）**：每个图层暴露不可变的 Actor 快照，携带 `Tag`、`BlueprintReference` 及由地图编辑器同款蓝图／脚本与实例覆盖解析器计算出的外观。`SourceRect` 为解析后的静态纹理矩形（已包含角色朝向的首帧，无需重新推导角色图集布局）。
- **Actor 变换与占位**：位移、缩放、原点、旋转、`Material`、`MaterialParameters` 与 `PreviewOpacity`（不可见 Actor 为 `0.25`，可见为 `1.0`）均已预解析。缺少外观时 `AssetPath` 为空且 `SourceRect` 为 `null`，可用于绘制单元格占位。渲染缩放应以 `CellSize` 为基准，而非图块集图块尺寸。
- **原生 Actor 材质预览**：调用 `IMapEditorHost.AcquireActorPreview(snapshot)` 可使用原生材质渲染解析后的源帧。返回的 `IPluginActorPreview` 提供 `Changed` 事件以及含预乘 BGRA 像素、尺寸、`OffsetX`／`OffsetY` 和材质错误的 `Frame`。请在应用 Actor 变换前将 `OffsetX`／`OffsetY` 相对源帧原点叠加（顶点位移可能扩大输出帧）。隐藏预览时将 `IsActive` 设为 `false`，不再使用时释放句柄；源帧动画仍由插件自行推进。

### 图层与 Actor 区域写入

- **图块图层替换（`PluginMapLayerWriteRequest`）**：指定地图与图层，回传 `BaseRevision` 与 `ExpectedTilesetKey`，并提供完整 `Height × Width` 形状的 `Tiles` 与 `AutoTiles` 矩阵。若需局部编辑图块，请复制快照矩阵后仅修改目标矩形再提交（不提供独立的区域图块写入 API）。`PluginMapWriteResult` 区分成功、revision／图块集冲突与错误，并返回最新 revision。校验或写入失败不会改动编辑器内存、撤销历史或界面。
- **Actor 区域替换（`PluginMapActorRegionWriteRequest`）**：指定 `MapKey`、`LayerName`、`BaseRevision`、从 0 开始计数的矩形 `PluginMapRegion(X, Y, Width, Height)`、必填的 `PreserveBaseClassReference` 以及非空的 `PluginMapActorPlacement(BlueprintReference, X, Y)` 列表。宿主会保留矩形外的所有 Actor，以及矩形内解析类继承自 `PreserveBaseClassReference` 的 Actor，仅移除矩形内其他 Actor 及其实例覆盖，并保持被保留记录原样不变。
- **Actor 摆放校验**：新增摆放项必须引用有效的 Actor 蓝图，落在矩形内互不重复的可通行格上，且不得与被保留 Actor 或其他图层 Actor 重叠；宿主自动分配地图内唯一标签。隐藏图层、无效边界、无法解析的类、既有标签无效或重复、空摆放列表以及 revision 冲突均会在写入前被拒绝。Tile 与自动图块不受影响。
- **直接保存与撤销语义**：任一写入成功后，宿主都会更新内存地图、原子保存单个 `Data/Maps/<mapKey>.json`、向该地图撤销栈压入一步撤销记录、推进 revision 并刷新目标地图与图层。同一张地图上已有的未保存编辑会随本次保存一并落盘；其他地图与项目文档既不会被保存也不会清除脏标记。执行撤销会恢复写入前的内存状态并将地图标记为脏，但不会自动保存。
- **待处理路径变更保护**：若目标地图存在尚未保存的关联路径变更，写入会被拒绝；请先执行一次全局保存。

### 项目脚本执行

`RunProjectScriptAsync` 以项目根目录为工作目录，调用项目自身的游戏可执行文件运行已存在的绝对路径 `.lua` 或 `.luac` 入口脚本，并传入独立的 `JsonObject` 请求（不调用常规 `Entry`，也不保存项目文档）。

- **前置条件**：存在未保存的非地图项目文档时请求会被拒绝；C++ Source 项目须具备最新的原生构建。若无法从 `Scripts/Entry.lua` 解析 `APP_NAME`，执行也会失败。
- **参数与初始化**：入口脚本通过 `arg[1]` 获取临时请求 JSON 路径、通过 `arg[2]` 获取必须写入的结果 JSON 路径、通过 `arg[3]` 获取解析后的 `APP_NAME`。加载项目模块前请先调用 `Engine.setAppName(arg[3])`，以确保用户数据与资源覆盖正常生效。若脚本需要创建 Actor 或依赖材质的图形对象，还须先完成[材质缓存准备](<../Lua 与蓝图脚本/全局与 Core 模块/Core Modules/Engine/地图与材质.md#材质缓存准备>)。
- **结果契约与限制**：退出前须向 `arg[2]` 写入单个 JSON 对象。`PluginProjectScriptResult.Success` 表示进程执行与结果解码成功，`Data` 保存返回的 JSON 对象，`Error` 描述宿主层错误（插件自定义载荷字段由插件自行处理）。结果超过 32 MiB、JSON 缺失或无效、进程以非零退出码结束，或执行期间检测到项目输入文件变更，都会导致请求失败。
- **进程生命周期**：宿主捕获标准输出与标准错误，在收到取消信号或超过 60 秒后终止子进程，并自动清理临时请求／结果目录。由于脚本在完整子进程而非受限沙箱中运行，其加载内容由脚本自行决定；官方插件 [Mota Map Fill](<官方插件/Mota Map Fill.md>) 即借助此入口在不启动场景的情况下解码存档并计算战斗预览。

## 文本提示

`RegisterTextHintProvider(ITextHintProvider)` 用于注册同步文本提示解析器。`TextHintContext` 提供项目路径、编辑器语言与当前字段文本。提供器按插件顺序依次执行，以首个非空结果为准；抛出的异常会记录为插件诊断。

## Before Export、Before Run 与 Before Pack

- **Before Export（`RegisterBeforeExportHook`）**：接收 `IProjectExportHook`（在 `IProjectOperationHook` 基础上新增 `ProjectExportFiles GetFiles(string projectPath)`）。`GetFiles` 仅检视而不修改文件，返回项目相对的具体文件路径列表 `InputPaths` 与 `OutputPaths`，不得重复且输入与输出不得重叠（尚未创建的输入文件也必须声明，以便后续检测到其生成）；实际导出工作在 `ExecuteAsync` 中完成。
- **导出顺序与指纹**：导出操作先保存并校验项目，再按顺序执行 Export Hook、生成 UI 运行时文件并记录成功状态。编辑器会对声明的文件以及导出插件身份（项目插件含项目作用域）计算指纹，以判定是否需要再次导出。点击**导出**、确认**导出后运行**以及每次编辑器**打包**都会执行 Export Hook；若 before-Run Hook 修改了导出输入，启动运行前会强制重新导出。详见[编译、导出与运行](<../编辑器用户指南/运行、调试与打包.md#编译导出与运行>)。
- **Before Run 与 Before Pack（`RegisterBeforeRunHook`、`RegisterBeforePackHook`）**：接收 `IProjectOperationHook`，在对应操作前依次执行。编辑器游戏打包会先执行完整导出，再执行 before-Pack Hook；[将 Source 转换为 Standalone 项目](<../编辑器用户指南/运行、调试与打包.md#将-source-转换为-standalone-项目>)仅执行 Export Hook，跳过 before-Pack Hook 及其打包排除规则。任一 Hook 返回失败或抛出异常都会中止后续 Hook 并阻断操作；实现时应响应 `CancellationToken` 并通过 `IPluginOutput` 汇报进度。

| 操作上下文字段 | 含义 |
|---|---|
| `ProjectPath` | 已打开项目的根目录 |
| `EditorLanguage` | 当前编辑器语言键 |
| `Operation` | `ProjectOperationKind.Export`、`Run` 或 `Pack` |
| `Packaging` | 仅在 before-Pack Hook 中非 `null`；Export 与 before-Run 中为 `null` |
| `Output` | 仅支持追加的操作输出 |
| `TextHints` | 请求刷新文本提示 |
| `CancellationToken` | 当前操作的取消令牌 |

当 `Packaging` 非 `null` 时，`IProjectPackaging` 暴露以下打包控制成员：

| `IProjectPackaging` 成员 | 含义 |
|---|---|
| `UseLuac` | 指示 Pack 是否编译受支持的 Lua 源码 |
| `UseLdPak` | 指示 Pack 是否将完整的 `Assets`、`Data` 与裁剪后的 `Scripts` 目录树打包为根目录下的 `Assets.ldpak`、`Data.ldpak` 与 `Scripts.ldpak` |
| `CompileLuaDirectory(relativePath)` | 将项目相对路径下的 Lua 目录纳入 Pack 编译集合 |
| `ExcludeFile(relativePath)` | 将项目相对文件从最终发行包中排除 |

例如，官方插件 Locale Tools 在 before-Pack Hook 中调用 `ExcludeFile("Data/Locale/Locale.xlsx")`，以便在最终游戏包中排除创作工作簿，同时在 Standalone 转换中予以保留；其 Export Hook 则单独写出 Lua 语言目录文件。

## 蓝图助手

蓝图助手通过常规 `PluginMenuCommand` 注册入口，并自行管理 UI、设置与会话历史（宿主不提供助手窗口或控件，`IAvaloniaPluginUserInterface` 仅提供可选的父窗口）。在项目窗口中，`PluginMenuContext.BlueprintAssistantHost` 提供项目路径、建议的当前蓝图、可用蓝图键列表与 `CreateSession`。

- **会话与工作区边界**：`IBlueprintAssistantSession` 绑定到单个已有蓝图，暴露其基础 revision 与 `IBlueprintAssistantWorkspace`。工作区支持列出与读取蓝图、查询 API 目录、搜索或读取白名单内的项目源码、查询官方文档、校验候选修改并暂存提案，但无法直接修改蓝图或访问任意编辑器服务。
- **审批与应用**：插件 UI 持有会话，在用户确认后显式调用应用或放弃。应用时会重新校验基础 revision 与候选合法性，记录一步撤销快照，将项目数据标记为脏，并刷新该蓝图已打开的编辑器窗口，但不会自动写入磁盘。
- **密钥管理**：请将 API Key 存储在按作用域隔离的 `PluginMenuContext.SecretStore` 中；详见[保存密钥](<Avalonia UI、密钥与本地化.md#保存密钥>)。

### 官方文档查询

| 文档成员 | 返回结果 |
|---|---|
| `SearchDocumentationAsync(query, maxResults, cancellationToken)` | 在标题、路径与完整 Markdown 中按关键词匹配，返回文档键、摘要与官方页面 URL |
| `ReadDocumentationAsync(documentKey, startLine, lineCount, cancellationToken)` | 按文档键与从 1 开始的行号范围返回带行号的 Markdown 文本及官方页面 URL |

这两个方法会按需拉取固定官网上对应编辑器所选文档版本与语言的搜索索引（与[在线文档](<../编辑器用户指南/启动页、工作区与快捷键.md#在线文档>)保持一致），并在单轮请求工作区内复用同一缓存快照。它们仅接受查询词与文档键（不接受项目路径或任意 URL）。索引缺失、无效或网络故障时会返回工具错误，且不会切换文档版本或语言。

文档结果上限为 12,000 个字符：若存在 `nextLine`，可从该行继续读取；若存在 `truncatedLine`，说明某超长单行尾部被截断，续读应从下一行开始。具体蓝图 API 仍以 API 目录元数据为准；详见 [Blueprint AI 官方文档查询](<官方插件/Blueprint AI.md#官方文档>)。

## 资源清理

在项目窗口中，`PluginMenuContext.ResourceCleanupHost` 暴露 `IResourceCleanupHost`。插件负责扫描窗口 UI、内置原生保留清单与最终确认；宿主负责静态引用分析、报告校验、项目保留清单持久化以及移至系统回收站／垃圾篓。

| 成员 | 含义 |
|---|---|
| `ProjectPath` | 已打开项目的根目录 |
| `ReadKeepPathsAsync` / `SaveKeepPathsAsync` | 读取或替换 `EditorCache/ResourceCleanupKeep.list` 中的项目追加项（保存空列表会删除该文件） |
| `ScanAsync(nativeKeepPaths, userKeepPaths, progress, cancellationToken)` | 分析静态引用并返回含候选文件、类别、大小与扫描问题的不可变报告 |
| `TrashAsync(reportId, progress, cancellationToken)` | 重新校验并将报告中的全部候选移至回收站／垃圾篓，返回已移走路径、剩余路径及错误信息 |

`ResourceCleanupProgress` 报告当前阶段、已完成／总数计数与当前处理路径。包含扫描问题的报告不能执行清理，且项目输入一旦变更原报告即失效。若系统回收站或垃圾篓不可用，宿主绝不回退为永久删除。扫描边界与审阅流程见 [Resource Cleanup](<官方插件/Resource Cleanup.md>)。

## 不支持的扩展点

目前没有面向宿主停靠面板、工具栏、任意右键菜单、自定义数据类型或编辑器事件的公开 Hook。`RegisterMapContextMenuCommand` 是唯一的右键菜单扩展点。插件可由命令打开自己的 Avalonia 窗口，但不能向编辑器自有视图中注入控件。
