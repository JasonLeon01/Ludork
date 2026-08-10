# Ludork → Avalonia / Lua 迁移状态与 TODO

> 唯一功能与交互基准：原版 Ludork `D:\Dev\Ludork`。
> 当前项目：`D:\Dev\Ludork-ava-lua-migration`。
> 最后重审：2026-08-03。
> `docs/` 已按 Avalonia 编辑器、Lua runtime、C++ 绑定、插件与 Script Mixin 工作流重构，并提供 `en_GB` / `zh_CN` 镜像。

本清单不再使用整体完成百分比。当前主功能迁移已经基本完成，后续主要工作是目标平台、发布产物和完整交互流程验收；源码实现状态与验收状态继续分开记录。

## 状态定义

| 状态 | 含义 |
|---|---|
| ✅ 已完成 | 当前源码中已有完整入口、数据流和主要交互；仍可能在“待专项验收”表中列出目标平台或综合流程验收 |
| 🧪 待验收 | 实现已存在，但尚未在对应目标平台或完整真实流程中验收 |
| ➖ 明确不迁移 | 已作出的迁移边界，不应重新列为缺陷 |
| ⬜ 待办 | 当前源码仍缺失，后续需要实现 |

## 当前结论

- 编辑器主工作流已从“界面骨架”进入可编辑状态：新建/打开项目、File 菜单、地图 CRUD、地图属性、Console、运行模式、Actor/Light 属性、General Data、Blueprint 节点编辑、Animation/Curve、打包均已有实现。
- 2026-07-22 这一批补齐了新建 Blueprint、Common Functions、全局引用索引与引用树、Blueprint 校验、Game Config、性能监控以及文件浏览器/地图列表等非 AI 编辑交互。
- 2026-07-29 已统一可信 C# 源码插件协议，并加入 `Plugins/OfficialBlueprintAI` 官方插件源码；导入 staging 副本时先动态编译校验，启动时从已登记源码再次编译加载。Blueprint AI 的菜单、窗口、设置、provider、历史与 Agent 编排归插件，宿主只保留可选 Avalonia owner 以及受限 Blueprint workspace/session/Apply 边界。
- Lua/C++ runtime 主干已经落地：Engine、GlobalCore、GlobalFunctions、Standard、LuaSF、自动绑定/metadata/stub、Class/Blueprint 执行、地图/Actor/动画/音频/视频/渲染/输入/存档/本地化均有实际实现。
- 发布源码链已经齐全：Windows 10 或更高版本的 x64 编辑器包与 MSI、macOS 13.3 或更高版本的 Apple Silicon `.app`、对应 Windows/macOS 项目包、iOS 15.0 或更高版本的 arm64 Xcode 构建/签名/IPA/真机安装启动、HarmonyOS 6.0.1 / API 21 或更高版本的 arm64-v8a HAP 构建，以及 ScriptTools、Lua 5.5 bytecode、数据/Shader 可选加密均有正式入口。
- 本轮静态重审未发现仍为空白或占位的原版主功能。当前剩余项均归为专项验收：跨平台插件/AI、完整 Sample/编辑器流程和长期性能回归。

## 已完成

### 应用壳、菜单与项目流程

| 能力 | 原版参考 | 当前实现 | 状态 |
|---|---|---|---|
| 应用入口、主题、启动窗口、CLI 打开项目 | `LoadEditor.py`, `StartWindow.py`, `main.py` | `Program.cs`, `App.axaml`, `Views/StartWindow` | ✅ |
| 新建项目 | 原版 new project 流程 | `Views/NewProjectWindow`，创建项目结构和项目配置 | ✅ |
| File 菜单 | `MenuBuilderMixin` | New/Open/Save/Pack/Exit 已接入 `MainWindow` | ✅ |
| 主窗口布局、分栏与编辑模式 | `MainWindow.py`, `LayoutMixin` | `Views/MainWindow` + `MainViewModel` | ✅ |
| 帮助、About、本地化切换 | `MarkdownPreviewer.py`, `AboutDialog.py`, `Locale.py` | `MarkdownPreviewWindow`, `AboutDialog`, `LocaleService` | ✅ |
| 插件系统与官方 Locale 工具 | `PluginSystem.py`, `Plugins/OfficialLocaleTools` | `Ludork.Plugin.Abstractions`、`Ludork.Plugin.Avalonia`、`Services/Plugins`、`Plugins/OfficialLocaleTools`；所有插件统一使用源码 manifest、安全目录导入、导入编译校验和启动时 Roslyn 编译加载 | ✅ |
| 官方 Blueprint AI 源码实现 | 原版 Blueprint AI / QuickJS 编排 | `Plugins/OfficialBlueprintAI`；`Game → Blueprint AI`、插件自带 Avalonia UI/设置/历史、OpenAI Responses 与兼容 Chat adapter、受限 Blueprint Agent | ✅ |
| 文件与资源预览 | `FilePreview.py`、Animation 音频/波形预览 | `FilePreviewDialog`、`FileSelectorDialog`、`AnimationAudioPlayback` 及 Windows/macOS 后端 | ✅ |
| 窗口状态与独立/嵌入运行设置 | `EditorStatus`, Development Tools | `EditorSettings`, `ProjectConfigService` | ✅ |
| Toast 与通用确认/输入/文件选择 | `Toast.py`, `Widgets/Utils` | `Toast`, `EditorFeedback`, `AlertDialog`, `ConfirmationDialog`, `SingleRowDialog`, `FileSelectorDialog` | ✅ |

### Official Blueprint AI 源码实现状态

下表只记录当前源码中已经存在的实现，不等同于发布包、真实 API 或跨平台流程已经验收；实际执行结果单独记录在“待专项验收”和“验证记录”中。

| 层级 | 当前源码实现 | 状态 |
|---|---|---|
| 目录与分发边界 | 源码固定在 `Plugins/OfficialBlueprintAI`；开发插件根为 `./Plugins`，打包用户插件根为 `~/Ludork/Plugins`，可写状态为 `Plugins/.data/Ludork.OfficialBlueprintAI`；插件 ID 为 `Ludork.OfficialBlueprintAI` | ✅ |
| 插件自带界面 | 插件通过普通 `PluginMenuCommand` 提供 `Game → Blueprint AI`，并自带聊天/设置窗口、本地化、历史侧栏、提案差异与 Apply/Discard 交互；主程序不携带 AI 窗口 | ✅ |
| 宿主契约 | `IAvaloniaPluginUserInterface` 只提供可选窗口 owner；`IBlueprintAssistantHost`、session 与 workspace 只提供项目/Blueprint 查询、受限源码/API 读取、候选验证、提案及用户确认后的 Apply/Discard | ✅ |
| Provider 与 Agent | 基于 .NET HTTP/JSON 的 OpenAI Responses adapter 使用 `store:false`；DeepSeek、Google OpenAI-compatible 与 Custom 使用兼容 Chat adapter；显式工具循环限制为蓝图研究与提案能力 | ✅ |
| 设置、密钥与历史 | provider profile 与凭据引用写入插件数据；密钥使用 Credential Manager / Keychain 或其他平台环境变量，并只以脱敏前后缀显示；不读取旧版 `Ludork.ini`；本地 append-only 历史为权威数据，请求上下文使用滚动摘要与最近消息 | ✅ |
| Blueprint 写入 | 模型不能直接 Apply；宿主在 Apply 前检查基础 revision 与候选有效性，通过 `GameDataService` 记录一个 Undo、保持 Dirty、刷新目标编辑器且不直接写盘 | ✅ |

### 数据、保存与历史

| 能力 | 当前实现 | 状态 |
|---|---|---|
| JSON 数据分区加载/保存 | `GameDataService` 管理 Configs、Tilesets、AutoTiles、Maps、Blueprints、CommonFunctions、Animations、Curves、General | ✅ |
| Undo/Redo 与 dirty 状态 | snapshot、undo/redo 栈、`ModifiedChanged`、`DataChanged`、差异通知 | ✅ |
| Blueprint/Common Function CRUD | 明确的 Create/Update/Rename/Delete/Copy API | ✅ |
| 修改 Blueprint 集合查询 | `GetModifiedBlueprintKeys()` | ✅ |
| 统一保存入口 | `ProjectSaveService` 在保存前 flush 图编辑器，只校验新增/修改 Blueprint，并支持取消或强制保存 | ✅ |
| GameData 与 `Main.ini` 联合 dirty | 标题星号、Ctrl+S、退出/换项目提示统一覆盖 JSON 和 pending INI | ✅ |
| 文件变更定向同步 | 新增/移动/重命名/删除同步 current/origin/undo/redo，不再用全量 Reload 清空未保存状态 | ✅ |

### 地图、图块、Actor 与 Light

| 能力 | 原版参考 | 当前实现 | 状态 |
|---|---|---|---|
| 地图列表 CRUD 与打开 | `MapListOpsMixin` | 新建、编辑/重命名、复制、粘贴、删除；双击/Enter 打开 | ✅ |
| 地图属性 | `MapEditDialog.py` | `Views/MapEditWindow`：尺寸、音频、雾、环境光等 | ✅ |
| 多层地图绘制 | `EditorPanel.py`, `LayerBarMixin` | 图层 CRUD/排序/Shader、绘制/擦除、overview、缩放/平移、网格/光标 | ✅ |
| Tileset / AutoTile | `TilesetEditor.py`, `AutoTilePanel.py` | `TilesetEditorWindow`, `TilesetImageEditor`, `AutoTilePreview`, `AutoTileRenderer` | ✅ |
| Actor 放置与编辑 | `EditorPanel.py`, `ActorInfo.py` | 放置、选中、拖移、复制/粘贴/删除、队列闭环、`ActorInfoPanel` 属性编辑 | ✅ |
| Light 放置与编辑 | `EditorPanel.py`, `LightPanel.py` | 新建、选中、移动、半径、删除、`LightInfoPanel` 属性编辑 | ✅ |
| Light 粘贴 | 原版对应入口为空实现 | 不额外扩展，保持原版行为 | ✅ |
| 模式快捷键 | `EditModeToggle` | Ctrl+1 / Ctrl+2 / Ctrl+3 | ✅ |

### 数据库与图编辑器

| 能力 | 当前实现 | 状态 |
|---|---|---|
| System Config (F5) | `ConfigWindow` + `ConfigDictPanel` | ✅ |
| Animation 总览/编辑 (F6/F10) | `AnimationOverviewWindow`, `AnimationWindow`, `AnimationEditor`, `AnimationTimeline` | ✅ |
| Tilesets (F7) | `TilesetEditorWindow` | ✅ |
| Common Functions (F8) | 1200×800 模态窗口、排序列表、单图、CRUD、整理、深拷贝剪贴板、F2/Delete/Ctrl+C/V/S/Z/Y | ✅ |
| General Data (F9) | 类型/成员 CRUD、字段/参数编辑、General 图编辑、引用选择器 | ✅ |
| Curve (F11) | `CurveWindow`, `CurveEditor` | ✅ |
| Blueprint 编辑器 | parent/attrs/components、事件列表、Preview、节点图、节点选择、参数编辑、整理、快捷键和保存 | ✅ |
| Blueprint 类与 metadata 解析 | `LuaMetadataService`, `BlueprintClassResolver`, shared node definition catalog/context | ✅ |
| 新建 Blueprint | 强类型 creation request、路径/父类选择、默认字段拍平、继承事件初始化、只写内存 | ✅ |
| Blueprint 校验 | parent/循环、graph/node/link/startNodes、nodeFunction、pin/default 范围校验；手动校验与保存前校验 | ✅ |
| Blueprint 专用工具栏 | AI 占位按钮已替换为“校验 Blueprint”；General Data 图不显示该按钮；AI 入口独立位于插件提供的 `Game → Blueprint AI` | ✅ |

### 引用与文件浏览器

| 能力 | 当前实现 | 状态 |
|---|---|---|
| 懒构建全局引用索引 | `ReferenceIndexService`，支持 path→node、incoming/outgoing、深度树和缺失目标 | ✅ |
| 引用扫描范围 | Config/Tileset/AutoTile/Map/Blueprint/Common Function/Animation/Curve/General/General Member/Asset，含 metadata PathVars 等规则 | ✅ |
| 引用树 | 非模态双向树、深度 5、循环标记、缩放、Tooltip、双击系统打开 | ✅ |
| 文件浏览器基础操作 | 多选 copy/cut/delete、内部拖拽移动、重命名、duplicate、新建目录、聚合错误 | ✅ |
| 文件浏览器数据菜单 | New Blueprint/Animation/Curve、派生 Blueprint、复制 Blueprint class name、引用树、系统打开 | ✅ |
| 文件移动边界 | 仅项目根内，拒绝 root、自身、子孙目录和目标冲突 | ✅ |
| 引用处理语义 | 引用树只展示；重命名/删除不自动改写引用，也不按引用阻断 | ✅ |

### Game Config、运行器、Console 与性能监控

| 能力 | 当前实现 | 状态 |
|---|---|---|
| Game Config (F2) | 原版 560×532 布局；script 只读；language/scale/FPS/vsync/音频开关与音量 | ✅ |
| `Main.ini` round-trip | 大小写兼容读取，保存保留未知 section/key，确认只写 pending | ✅ |
| editor-mode 配置路径 | `LUDORK_EDITOR=1` 时使用项目根 `./Main.ini`；普通/打包运行使用用户数据目录 | ✅ |
| 子进程运行器 | Debug 构建、可执行文件启动/停止、嵌入与独立窗口模式；通过新版环境变量传递 `RuntimeLaunchOptions` | ✅ |
| C++ 显示与场景调度 | C++ 校验窗口模式/HWND，统一创建 embedded/individual/普通桌面/iOS 窗口；`System.run()` 与 `SceneBase::systemMain()` 接管主循环 | ✅ |
| 编辑器/runtime IPC | protocol v2 TCP 命令通道、generation、批量输入转发、协议握手及通用命名 bool control | ✅ |
| Console | stdout/stderr 展示、命令发送、历史、可用态提示 | ✅ |
| 原生性能 marker | C++ 每 30 个主帧输出 v2 `__LUDORK_PERF__:`；主线程帧与逻辑 tick 独立记录，合法样本从 Console 抑制，畸形内容保留日志 | ✅ |
| 性能窗口 | 单实例非模态 800×400；上方显示最近约 5 秒逐主帧 FPS 曲线，下方显示实时、预览或点击锁定帧的 Main/Logic 分项耗时，明确区分 Update/LateUpdate/FixedUpdate | ✅ |
| 性能采样生命周期 | 原生 `performanceMonitor` control 在打开/命令就绪/重跑/关闭/停止时启停并重置，运行 generation 隔离旧命令 | ✅ |
| 应用图标 | Windows `Main.rc` 编译现有 ICO；模板携带 RC/ICO；macOS bundler 优先使用预生成 `icon.icns`，缺失或生成失败明确报错 | ✅ |
| 项目打包 | `ProjectPackService`, `PackSelectionDialog`, `PackLogDialog` | ✅ |

### Lua/C++ runtime 与发布链

| 能力 | 当前实现 | 状态 |
|---|---|---|
| Core runtime 与模块边界 | `Standard`、`Engine`、`GlobalCore`、`GlobalFunctions`、`CoreSystem` 独立 CMake 目标；Lua 根模块按固定顺序加载 | ✅ |
| Standard 原生运行时容器 | callable `list(...)` / `tuple(...)` / `dict(...)` full userdata；nil 槽/value、tuple value hash、dict 插入序、循环安全复制/比较、显式 `toTable()` 与 LuaLS stub；Core/SFML 边界仍只接收声明的原生类型 | ✅ |
| Class、Blueprint 与节点执行 | `ClassRuntime`、Graph/Node runtime、Event、ExecSplit、Latent、Loop、继承/回调、metadata 驱动参数与默认值 | ✅ |
| Sample gameplay 主干 | 地图、Actor、组件、动画、碰撞、UI、战斗、音频、存档、本地化、Direct Script Mixin 与示例 Blueprint | ✅ |
| 桌面显示与编辑器桥接 | C++ 窗口/画布/SceneBase 主循环，Windows embedded/individual、protocol v2、输入转发、Console 与性能采样 | ✅ |
| FFmpeg 视频 | 可选裁剪 FFmpeg、H.264/AAC 解码、音画同步、缩放居中、静音/跳过与无 FFmpeg 明确报错 | ✅ |
| Lua 与资源发布处理 | `ScriptTools`、Lua 5.5 `luac`、源码/bytecode 包、开发文件裁剪、可选数据与 Shader 加密、C++ 统一 Shader loader | ✅ |
| 桌面项目发布 | Windows 10 或更高版本的 x64、macOS 13.3 或更高版本的 Apple Silicon 项目包；Cpp/Standalone 与 FFmpeg 变体、图标、许可证和包结构校验 | ✅ |
| 编辑器发布 | Windows 10 或更高版本的 x64 自包含目录包、WiX MSI；macOS 13.3 或更高版本的 Apple Silicon `.app`、Info.plist、ICNS、Finder 文档类型与包结构校验 | ✅ |
| iOS 交付源码链 | iOS 15.0 或更高版本的 arm64 CMake/Xcode、静态依赖、平台窗口/触摸/资源与用户数据路径、签名、`.app`/`.ipa` 校验、真机安装和启动命令 | ✅ |
| HarmonyOS 交付源码链 | HarmonyOS 6.0.1 / API 21 或更高版本的 arm64-v8a CMake/OHOS 构建、平台窗口/触摸/资源路径与未签名 HAP 校验 | ✅ |

## 快捷键校准

| 功能 | 原版 | 当前状态 |
|---|---|---|
| Help | F1 | ✅ |
| Game Config | F2 | ✅ |
| Reload Module | F3 | ➖ 当前 Lua 模块模型不适用 |
| New Blueprint | F4 | ✅ |
| System Config | F5 | ✅ |
| Animation Overview | F6 | ✅ |
| Tilesets | F7 | ✅ |
| Common Functions | F8 | ✅ |
| General Data | F9 | ✅ |
| New Animation | F10 | ✅ |
| New Curve | F11 | ✅ |
| Save / Undo / Redo | Ctrl+S / Ctrl+Z / Ctrl+Y | ✅ |
| Tile / Light / Actor | Ctrl+1 / Ctrl+2 / Ctrl+3 | ✅ |

## 待专项验收

以下项目均已有源码实现，不再作为“功能未完成”统计；已执行的探针和 runtime smoke 单独写明，未执行部分不得写成“运行通过”。

| 验收项 | 需要执行的验证 |
|---|---|
| Official Blueprint AI | 已确证宿主能力探针、fake transport、真实 DeepSeek 只读请求、提案 Discard、提案 Apply/单步 Undo、Windows 源码目录复制/导入编译/登记/重启编译加载及 OfficialLocaleTools 回归；仍未在 macOS 实测 Keychain、在其他平台实测环境变量凭据，也未完成跨平台重启/卸载后的历史恢复 |
| 编辑器完整交互回归 | 新建 Blueprint、Common Functions、校验/保存、引用树、文件同步、Game Config、性能窗口等已有定向探针；仍需按原版完成一次连续人工工作流和像素/交互核对 |
| runtime 系统级回归 | 代表性地图与 Sample 主流程、节点 Event/ExecSplit/Latent/Loop、动画、战斗、存档、渲染和长时间运行仍需形成一轮可重复的综合验收记录 |
| 性能与视觉基线 | 代表性地图的 p95/stall/memory、长时间运行、OpenGL/Shader 像素或帧 hash 尚未形成跨平台基线 |

## 明确不迁移

| 项目 | 决策 |
|---|---|
| Python pickle / `.dat` | ➖ 编辑器坚持 JSON-only；不实现 pickle 解析、写入或 JSON 伪兼容 API |
| F3 Reload Module | ➖ 当前 Lua 模块模型不采用原版 Python 模块重载语义，不增加不可靠的兼容层 |
| Python package `__init__.py` 聚合模型 | ➖ Lua 模块按明确路径加载，不创建 `init.lua` 聚合入口 |
| Python `TypeAdapter` | ➖ runtime 直接接收约定的 `sf.*` 类型，不在 Lua metadata 中模拟 tuple/list 适配 |
| 引用自动修复/删除保护 | ➖ 原版引用树为展示工具；本迁移不因重命名/删除自动改写或阻断 |
| Light paste 扩展 | ➖ 原版入口为空实现，本批不增加超出原版的新交互 |

## 待办

| 优先级 | 项目 | 范围 |
|---|---|---|
| P1 | 编辑器/runtime 综合回归 | 以原版 Ludork 为基准连续走完项目编辑、保存、运行、重跑、打包和打包产物启动，覆盖代表性 Sample 地图 |
| P2 | Official Blueprint AI 跨平台验收 | 补齐 macOS Keychain、其他平台环境变量凭据，以及跨平台重启/卸载后的历史恢复 |
| P2 | 视频跨平台验收 | Windows 代表性 MP4 已实跑；补齐 macOS、静音/跳过交互、打包许可和 iOS 禁用/替代策略验收 |
| P3 | 性能与视觉回归基线 | 记录代表性地图的 p95/stall/memory、长时间运行以及 Shader/渲染帧 hash |

## 公共接口基线

本批新增或明确的编辑器侧接口：

- `EditorDataCreationRequest(EditorDataKind, DestinationPath?, ParentClass?, DataType?, InitialDirectory?)`
- `GameDataService.DataChanged`
- `GameDataService.GetModifiedBlueprintKeys()` 及 Blueprint/Common Function mutation API
- `BlueprintValidationService.ValidateBlueprint(...)` / `ValidateBlueprints(...)`
- `BlueprintValidationResult(BlueprintKey, IsValid, Errors)`
- `ReferenceIndexService` 的 path/node/incoming/outgoing/tree 查询
- `ProjectSaveService.TrySave(allowInvalidBlueprints)` / `ProjectSaveAttempt`
- `PerformanceSample(Fps, MemoryMegabytes)`，以及 `MainFrameTiming` / `LogicTickTiming` 明细属性
- `ProjectRunnerService.SetPerformanceMonitoringAsync(bool enabled, long generation)`
- 统一源码插件 manifest，以及 `IPluginRegistrar.PluginDataDirectory`
- `PluginMenuContext.SecretStore` / `IPluginSecretStore`
- `PluginMenuContext.BlueprintAssistantHost` / `IBlueprintAssistantHost`
- `IBlueprintAssistantSession` / `IBlueprintAssistantWorkspace` 及 Blueprint proposal、candidate、Apply/Discard DTO
- Animation JSON 有序 `timeTags: [{ "tag": string, "time": number }]`

本批新增或明确的 runtime/bridge 接口：

- `RuntimeLaunchOptions(Editor, WindowMode, HostWindowHandle?)`
- 内部 bootstrap `System.initializeDisplay(title, gameSize, iconPath, cursorPath)`；只生成 LuaLS stub，不生成 Blueprint metadata
- `System.run()`；只生成 LuaLS stub，不生成 Blueprint metadata
- bridge protocol v2 `{"v":2,"type":"control","name":"performanceMonitor","enabled":true|false}`
- `AnimationTimeTag { tag, time }`、`AnimationSourceData.timeTags`、`AnimationData.timeTags`
- `AnimSprite.getAllTimeTags()`；返回时间稳定排序后的副本，`GlobalCore.Animation` 通过继承直接获得
- Standard `string.utf8Length(value)` / `string.utf8Slice(value, start, finish)`
- Standard `table.contains(values, target)` / `table.orderedStringKeys(values, preferredOrder?)`

runtime 不再暴露 `setPerformanceMonitorEnabled`、`isPerformanceMonitorEnabled` 或 `recordPerformance` Lua API；性能控制和采样完全归 C++。

## 原版结构映射

| 原版 | 当前迁移位置 |
|---|---|
| `EditorGlobal/StartWindow.py` | `Views/StartWindow` |
| `EditorGlobal/MainWindow.py` | `Views/MainWindow` + `ViewModels/MainViewModel` |
| `EditorGlobal/Data.py` | `Services/GameDataService`, `ProjectSaveService`, `ReferenceIndexService` |
| `EditorGlobal/MainUtils/MapListOps.py` | `MainWindow` + `MainViewModel` 地图 CRUD/打开交互 |
| `Widgets/EditorPanel.py` | `Controls/MapPanel` |
| `Widgets/ActorInfo.py` / `LightPanel.py` | `Controls/ActorInfoPanel`, `LightInfoPanel` |
| `Widgets/FileExplorer.py` | `Controls/FileExplorerPanel` + `FileExplorerViewModel` |
| `Widgets/BlueprintEditor.py`, `NodeGraph/` | `Views/BlueprintEditorWindow`, `Views/Utils/BlueprintGraph` |
| `Widgets/CommonFunctionWindow.py` | `Views/CommonFunctionWindow` |
| `Widgets/ReferenceTreeDialog.py` | `Views/ReferenceTreeWindow` |
| `Widgets/Utils/BlueprintValidation.py` | `Services/BlueprintValidationService` |
| `Widgets/Utils/GameConfigDialog.py` | `Views/GameConfigWindow`, `Services/GameConfigService` |
| `Widgets/Utils/PerformanceMonitorWindow.py` | `Views/PerformanceMonitorWindow`, `ProjectRunnerService` sample 解析 |
| `Widgets/GeneralDataEditor.py` | `Views/GeneralDataEditorWindow` |
| `Widgets/AnimationWindow.py`, `Timeline.py` | `Views/AnimationWindow`, `Controls/AnimationEditor`, `AnimationTimeline` |
| `Widgets/CurveWindow.py` | `Views/CurveWindow`, `Controls/CurveEditor` |
| `Widgets/Console.py`, `GameRunnerMixin` | `MainWindow` Console、`ProjectRunnerService`, `GamePanel` |
| `Utils/PluginSystem.py` | `Ludork.Plugin.Abstractions`, `Services/Plugins` |
| `Plugins/OfficialLocaleTools` | `Plugins/OfficialLocaleTools` |
| `Widgets/Utils/AiConfigDialog.py`, `AiChatDialog.py`, `agent/` | `Plugins/OfficialBlueprintAI` + `Services/BlueprintAssistant` 宿主边界 |

## 验证记录

| 日期 | 检查 | 结果 |
|---|---|---|
| 2026-08-03 | macOS 与 iOS 目标平台完整验收 | macOS 编辑器与项目发布、iOS 真机交付均已确认完成完整验收，相关项目已从“待专项验收”和“待办”移除；本条同步既有验收结论，不补写未提供的设备、系统版本或执行命令 |
| 2026-08-03 | General Data metadata linked type 与 Windows x64 完整发行链 | General Data linked type 已切换为 `LuaMetadataService.EnumerateTypes()` 与 `BlueprintClassResolver.IsDerivedFrom(..., "Engine.InfoBase")` 驱动；定向探针仅返回排序后的 `EnemyInfo`、`EquipInfo`、`ItemInfo`、`StateInfo`，不包含无 metadata 的 `PlayerInfo`、`Engine.InfoBase` 或非 Info 类型，短类型名 JSON 协议保持不变；生产源码不再扫描 `Source/Infos/*.py`，C++ 静态初值和 `shutdown()` 均使用 `Scripts/Entry.lua`。C# Release、ScriptTools 15 项测试、162 个 Lua、128 个 JSON、UI Asset/Adapter、Sample C++ Debug/Release 与 Release UiPreviewHost 均通过。最新 `pack_editor.bat` 完整产出四套模板、148+148 份双语文档及唯一位于 `tools/UiPreviewHost` 的 Host；发行包不含 Python/PDB/运行日志/PreviewHost 模板副本或构建机绝对路径。WiX build/validate 生成 177293568 字节的 `Ludork-1.0.0-win-x64.msi`，SHA-256 为 `46FD1995D53383C97B43A18C8DB1002E1BFE7B2D14C898FC3732CADA8E457EA1`。MSI 静默安装后 16403 个 `dist` 文件逐文件 SHA-256 一致，额外安装文件仅为关联图标；安装登记版本、桌面快捷方式、`.proj` ProgID/图标/打开命令均正确。已安装编辑器无参数启动及通过 `.proj` 关联打开临时模板工程均响应正常并正常关闭；从安装目录复制的 Standalone 普通包、Standalone-ffmpeg Lua 编译与 Data/Shader 加密包、Cpp fresh Release 最终包均打包和启动成功，未强制结束且无启动错误日志。静默卸载后安装登记、快捷方式、ProgID 与 `.proj` 关联均恢复到安装前状态，既有 `%LOCALAPPDATA%\Ludork\Ludork.ini` 保持 143 字节且 SHA-256 `2072CBBFD4E4C8AFC67DC421073C53340AA3196036A85EAA69BC0B5A1117AE6E` 不变，临时工程与测试数据已清理，正式 `dist` 与 MSI 保留。本项目尚未发布，本轮未执行也不声称旧版本升级兼容 |
| 2026-08-03 | Standard 原生容器、存档转换与 SFML 严格边界 | `LudorkStandard` 与 `Main` 的完整 Debug 依赖构建及无变更增量构建通过；临时 Lua probe 覆盖三类容器构造器及全部方法、nil、tuple value hash、dict 插入序、循环 GC/比较/deepcopy、`asizeof`、迭代失效、非法 key、`toTable()` alias/cycle/碰撞/空数组与 cjson round-trip，并验证地形旧 record/string/tuple-key 存档兼容、terrain/added actor/actor position 的 `sf.Vector2i` 运行时缓存、telepoint 的 `sf.Vector2u` 缓存、nil/覆盖/稳定序列化与 cjson round-trip。地图 JSON 的 actor position、ambient light 与 light 已在加载边界原生化；`Light.new` 拒绝 table，`floodFillTransparent` 返回 `sf.Vector2i[]`。全部 Lua 通过 `luac -p`，UI Catalog、adapter consistency 与 whitespace 检查通过；本项未将定向 runtime probe 记录为完整 Sample 手工交互验收，临时 probe 未落盘 |
| 2026-08-02 | 标准 `init` 自动准备 `UiPreviewHost` | Windows 无参数 `tools\init.bat` 实际执行通过：依赖与 ScriptTools 就绪后自动完成 Release Host 增量构建，`.tools/UiPreviewHost/bin/Release/UiPreviewHost.exe` 存在，执行前后 `Sample/bin/Release/Main.exe` 时间戳不变，确认未编译 Sample 应用；显式指定其他 C++ 项目时只初始化该项目依赖并明确跳过根 Sample Host，避免跨项目误用。macOS 对应接线已完成并通过 shell 语法检查，未在 Apple Silicon 实跑 |
| 2026-08-02 | `UiPreviewHost` 编辑器级工具拆分与 runtime smoke | 根级独立工程 Debug/Release 均构建通过，产物只写入 `.tools/UiPreviewHost`；Sample 应用开关关闭后的工程不含 `Main`、`lua_cjson` 或 Host 外的应用 target；仓库/普通项目两种 `ui-adapter-check` 均通过。真实 protocol v2/schema 1 握手包含 `ui`/`actor` capabilities；UI frame 为 320×64、15 nodes，actor atlas 为 34×34 且 `shaderError=false`，进程退出码 0、共享内存无残留；包内 Host 与独立 Release 产物哈希一致 |
| 2026-08-02 | `UiPreviewHost` Windows 分发边界与最终 MSI | 完整 `pack_editor.bat` 通过，Cpp 与 Cpp-ffmpeg 的 `Main` 真实 Release 构建通过；Cpp、Cpp-ffmpeg、Standalone、Standalone-ffmpeg 四套 Template 中 Host/Resolver 与 `*CopyProbe*` 均为 0，编辑器包只在 `tools/UiPreviewHost` 保留一份 Host。Standalone 污染探针验证可清理到 0，伪 Host 被占用时以退出码 1 拒绝完成；WiX build/validate 通过，最终 MSI SHA-256 为 `58EBC11C5A2BEDCE7563892277B67801C1742855D56A3731D07B7A15A8D0DF4A`，数据库中 Host 恰好 1 个、位于标准工具目录且同目录 14 个依赖完整，`*CopyProbe*` 为 0；临时探针已删除。本项未执行 MSI 安装/升级/卸载 |
| 2026-08-02 | 本批构建、测试与跨平台边界 | `dotnet build Ludork.csproj -c Release --no-restore` 通过，0 warning / 0 error；ScriptTools 36 项测试、UI Catalog、仓库/普通项目 adapter fingerprint 均通过；Windows 下已核对 macOS SONAME/工具路径接线并通过变更 shell 脚本语法检查，但未在 Apple Silicon 执行 `.app`、dylib/SONAME 或启动验收 |
| 2026-07-30 | 动画编辑器与时间 Tag 定向探针 | 通过：普通/Ctrl/Shift 复选、乱序选择后的网格索引顺序、重复文件名、图片/音频混排、首项 0.1 秒吸附和精确首尾衔接、整批冲突/坏音频全量拒绝、单次数据提交、旧名称载荷、单图 0.05 秒、单音频旧 fallback/截断、30/60 FPS、Tag 稳定排序/重名/同时间/添加/改名/删除/拖动吸附；探针及外部临时工程已删除；本项未代替真实编辑器鼠标手工流程 |
| 2026-07-30 | Animation Time Tag、Standard 与 Lua 回调运行探针 | Debug/Release 均通过：raw source → compressed JSON → `Data` cache/deep copy → `Animation.new()` → `getAllTimeTags()` 全链、旧动画空 Tag、Tag 不延长时长、空动画不因 Tag 产生帧；UTF-8、contains 洞与 `__eq`、有序字符串键边界；Timer、MovementSpecials、Enemy 战斗完成匿名回调；全部临时 Lua probe 已删除 |
| 2026-07-30 | 本批构建、生成物与语法检查 | `dotnet build Ludork.csproj -c Release --no-restore` 通过，0 warning / 0 error；`tools/build_cpp.bat Sample Debug` 与 `Sample Release` 通过；生成的 Engine binding/metadata/LuaLS stub 含 `AnimationTimeTag`、`timeTags`、`getAllTimeTags`；en_GB/zh_CN Locale 各 629 项且包含 3 个新键；23 个变更/生成 Lua 文件通过 `luac -p`，变更 JSON 可解析，`git diff --check` 无 whitespace error |
| 2026-07-30 | TODO 源码状态重审 | 对照当前入口重新核对编辑器、runtime、插件、资源预览、Windows/macOS 编辑器发布、Windows MSI、桌面项目打包与 iOS `ios-pack` 链；未发现仍为空白或占位的原版主功能，剩余工作统一收敛为目标平台与综合流程验收；本项为静态核对，不代表 macOS/iOS 实跑 |
| 2026-07-30 | 当前 HEAD 编辑器与 runtime 构建 | `dotnet build Ludork.csproj -c Release --no-restore` 通过，0 warning / 0 error；`tools/build_cpp.bat Debug` 通过，生成当前 `LudorkStandard`、`CoreSystem`、`Engine`、`GlobalCore`、`GlobalFunctions` 与 `Sample/bin/Debug/Main.exe`；本轮未执行完整 Sample 交互 |
| 2026-07-29 | 统一插件源码目录导入回归 | 删除 Official AI 的 `bin/obj` 后，真实选择 `Plugins/OfficialBlueprintAI` 与 `Plugins/OfficialLocaleTools` 纯源码目录，均在 staging 副本完成 Roslyn 编译和入口验证后复制、登记，并由新 Host 再次从源码编译加载；AI 只注册 `Game` 菜单，Locale Tools 保留 BeforePack hook；语法错误插件被拒绝且没有登记或残留目标目录；实际 Debug 编辑器携带当前注册表启动存活 8 秒，无旧 DLL/构造函数错误 |
| 2026-07-29 | Official Blueprint AI fake transport 与持久化安全探针 | 当前 OpenAI Responses fake HTTP/SSE 已验证 `store:false`、禁止并行工具、单工具上限、认证与 organization/project header、文本增量、function call 及下一轮 tool output 延续；此前还通过兼容 Chat SSE、密钥脱敏、Agent provider 路由及 5/8/30 预算、prompt injection 能力边界、历史新建/重命名/中断/损坏尾恢复/项目隔离/摘要删除和 endpoint 篡改拒绝；旧配置迁移断言已随该功能删除，不再计入当前验收 |
| 2026-07-29 | Official Blueprint AI 本机真实 DeepSeek 端到端 | `DeepSeek` / `deepseek-v4-flash` profile 的真实 connection test、最小只读请求、提案 Discard、提案 Apply/Undo 均通过，Apply 后可见窗口刷新次数为 1、只产生一个 Undo、未 Save 前磁盘内容不变；macOS Keychain 与其他平台环境变量凭据未在本机实测 |
| 2026-07-29 | 一次性 Blueprint Assistant 宿主能力探针 | 通过 42 项断言：允许与拒绝路径、symlink/INI/插件数据/敏感文件、严格 patch/candidate 校验、无效提案不 Apply、revision conflict 不覆盖、有效 Apply 只产生一个 Undo、保持 Dirty、磁盘不变、刷新可见目标及 Undo 恢复；探针和运行目录已删除 |
| 2026-07-29 | `dotnet build Ludork.csproj -c Release --no-restore` | 通过，0 warning / 0 error；只证明当前主编辑器及宿主 contract 能构建，不代表最终 AI 插件包导入或真实 provider 请求已验收 |
| 2026-07-26 | ScriptTools、Lua bytecode 与编辑器包 | Windows 编辑器包及 Cpp、Cpp-ffmpeg、Standalone、Standalone-ffmpeg 四套模板生成通过；C++ Debug/Release 与 C# Release 通过；无 Python PATH 的 Release native build 通过；普通包为 120 `.lua` / 0 `.luac`，bytecode 包为 0 `.lua` / 120 `.luac` 且启动加载成功，发布包不含 Python 源码 |
| 2026-07-26 | 发布裁剪、数据/Shader 加密与完整 Windows 编辑器包 | finalizer round-trip、损坏拒绝、开发文件清理、许可证保留和加密 Shader 输出通过；Lua/Shader 组合打包矩阵通过；完整 `pack_editor.bat` 退出码 0，四套模板与 ScriptTools、luac、GNU Make 等工具均存在 |
| 2026-07-22 | 当前迁移源码与原版入口静态映射 | 已核对；用于移除旧清单中“空窗口/未接线/未开始”等失真描述 |
| 2026-07-22 | `dotnet build Ludork.csproj -c Debug --no-restore` | 通过，0 warning / 0 error |
| 2026-07-22 | `dotnet build Ludork.csproj -c Release --no-restore` | 通过，0 warning / 0 error |
| 2026-07-22 | `tools/build_cpp.bat Debug` | 通过，生成 `Sample/bin/Debug/Main.exe` |
| 2026-07-22 | `tools/build_cpp.bat Release` | 首次遇到 MSVC 并行扫描 `.d.json` 锁冲突；单次增量重试通过，生成 `Sample/bin/Release/Main.exe` |
| 2026-07-22 | FFmpeg 视频真实播放 | 可选 FFmpeg 的完整 Sample Debug build 通过；实际播放 `Sample/Assets/Videos/sample-5mb.mp4`，收到 protocol v2 ready，除预期 swscale 警告外无异常，最终 `Game exited successfully.` / exit code 0 |
| 2026-07-22 | JSON 语法检查 | 通过，解析 `Locale/locale.json` 与 `Sample/Data` 共 67 个 JSON 文件 |
| 2026-07-22 | Lua 语法检查 | 通过，使用实际 Debug `lua.dll` 的 `luaL_loadfilex` 解析 `Sample/Scripts`，117 / 117 个 Lua 文件通过 |
| 2026-07-22 | 一次性功能探针 | 通过：Blueprint 创建/校验/保存、Common Function CRUD/历史、Sample 引用断言、Game Config round-trip、文件定向同步、性能 marker；探针及产物已删除 |
| 2026-07-22 | 一次性边界探针 | 通过：跨 section 类型过滤、`.json` 目录删除、重命名/目录穿越拒绝、INI 畸形 fallback 与 I/O 失败；探针及产物已删除 |
| 2026-07-22 | 真实 runtime smoke（独立窗口） | 通过：命令通道就绪；editor runtime 从项目 `Main.ini` 读取 `zh_CN`；`PrintTest` Common Function 输出符合预期；性能监控收到有效样本后成功禁用；游戏正常停止 |
| 2026-07-22 | 生成 binding / metadata / stub API 审计 | 通过：19 个已下沉或清退 API 均为 0 命中；`initializeDisplay` / `run` 仅存在于 runtime binding 与 LuaLS stub，不进入 Blueprint metadata |
| 2026-07-22 | protocol v2 与原生性能 smoke（独立窗口） | 通过：收到 v2 ready；样本含 30 个主帧、34 个独立逻辑 tick，字段均有限非负；disable + shutdown 后退出码 0 |
| 2026-07-22 | 帧级性能边界 smoke（独立窗口） | 通过：正常速度 3 批共 90 个主帧、97 个 Logic Tick；零速度 2 批共 60 个主帧、65 个 Logic Tick 均为 `fixedSteps == 0` 且 `fixedTick == 0`；每批恰 30 个主帧，字段均有限非负，临时探针已删除；当前 SceneTitle 未定义可安全替换的 Lua onLateTick/onFixedTick，未执行延迟注入 |
| 2026-07-22 | 正向 embedded HWND / 输入 smoke | 通过：800×600 宿主附着、v2 ready、鼠标输入查询、30 主帧/45 逻辑 tick 样本均正常；runtime 退出后宿主 HWND 仍有效 |
| 2026-07-22 | embedded HWND 启动边界 | 通过：零值、`uintptr_t` 溢出、尾随字符和已销毁 HWND 均在约 0.12–0.14 秒内退出并返回码 1 |
| 2026-07-22 | protocol mismatch 与 disable 停流 | 通过：v1 control 被 v2 runtime 拒绝并断开；重新连接 v2 后正常；disable 命令屏障后 1.5 秒内新增 marker 为 0 |
| 2026-07-22 | 本批 C++ Release 重建 | 通过，耗时约 183.5 秒，生成 `Sample/bin/Release/Main.exe` |
| 2026-07-22 | `tools/create_templates.bat Release` | 通过，包含 C++ template clean Release build，并生成 Standalone template；耗时约 590.7 秒 |
| 2026-07-22 | Windows PE 图标资源探针 | 通过：Sample Release 与 Standalone template `Main.exe` 均含 `RT_GROUP_ICON` type 14 / ID 101；C++ template 的 RC/CMake 与 ICO/ICNS 已核对一致；临时探针已删除 |
| 2026-07-22 | macOS bundler / ICNS 静态检查 | `macos_bundle.py` 通过 `py_compile`，预生成 ICNS 可解析；当前 Windows 环境未执行两种 macOS 实际打包、`plutil`、Finder 或 Dock 验收 |
| 2026-07-22 | `git diff --check` | 通过；仅输出工作区既有 LF→CRLF 提示，无 whitespace error |
| 2026-07-22 | 完整编辑器/runtime 手工流程 | 未执行，不记为通过 |

后续每次更新必须把“实现状态”和“验证状态”分开记录；只有实际执行过的 build、smoke 或手工流程才能写入验证记录。
