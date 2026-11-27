# Lua 运行时与模块

Ludork 运行 Lua 5.5，内置 Standard 扩展、LuaSF 以及项目原生模块。本页介绍项目脚本架构、模块边界、热重载、错误处理与工作区声明。

初次接触 Lua 的读者可先阅读 [Lua 基础语法](<../快速入门/Lua 基础语法.md>)，再查阅 [Ludork Lua 进阶](<../快速入门/Ludork Lua 进阶.md>)，了解 Ludork 提供的语言扩展（`class`、`bool`、`list`、`dict`、复制、文件系统辅助函数与 `asyncio`）。

| 模型 | 适用场景 | 主要来源 |
|---|---|---|
| 蓝图图模式 | 可视化事件与执行流 | `Data/Blueprints/*.json` |
| Lua 类 | 可复用系统与玩法类型 | `Scripts/**/*.lua` |
| Script Mixin | 直接附加到蓝图类上的 Lua 行为 | `Scripts/Mixins/**/*.lua` 及同名 `_meta.lua` |
| 声明式 UI | JSON 资产与生成的类型化 View 树 | `Data/UI/**/*.json`、生成的 `Scripts/Internal/UI/**/*.lua` 及对应 Stub |
| UI 行为 | 手写基础模块与业务 Controller | `Scripts/Internal/UIBase`；窗口模块内的私有 Controller；独立的行与场景 `Controller.lua` 模块 |

> **重要**：同一条蓝图继承链只能使用图模式或 Script Mixin 模式，不得在同一链中混用或切换。

## 入口与模块顺序

原生宿主在启动时执行命令行第一个参数指定的脚本；未传入参数时默认运行 `Scripts/Entry.lua`。启动流程不会读取项目本地配置或 `script` 设置来决定入口文件。

请按以下顺序加载根原生模块，先完成全部直接 `require` 调用，再声明局部别名：

```lua
local Engine = require("Engine")
Engine.setAppName("MyGame")

local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Logging = require("Global.Utils.Logging")

local Input = Engine.Input
local System = GlobalCore.System
local ManagerFunctions = GlobalFunctions.Manager
```

### 模块边界与加载规则

- **原生与 Lua `GlobalFunctions`**：原生 `GlobalFunctions` 模块暴露 `Components`、`UI`、`NodeGraph` 与 `Manager`。`Scripts/GlobalFunctions` 下的 Lua 蓝图函数库是独立模块，需通过完整路径加载（例如 `require("GlobalFunctions.Math")`），不属于原生根表成员。
- **蓝图函数库限制**：`Scripts/GlobalFunctions` 下的模块不得在模块作用域调用 `require`（包括加载原生模块），应在函数首次使用时按需解析依赖，避免循环加载。其模块路径与导出名称不得与 C++ `GlobalFunctions` 绑定冲突。运行 `ludork-check global-functions <repository-root>` 可静态校验这些规则；若存在语法非法的字符串、长注释或数字字面量，命令也会输出文件、行号与列号并失败。
- **禁止聚合入口与随意延迟加载**：项目模块一律按完整点分路径加载，不要添加目录级 `init.lua` 聚合模块。除蓝图函数库外，常规依赖应直接在模块顶层 `require`；仅在打破真实循环依赖，或在明确的首次使用边界（例如可选窗口工厂）处延迟 `require`，且其生命周期必须由所属场景或功能显式管理。
- **层级依赖方向**：静态依赖自根原生模块流向 `Scripts/Global`，再流向 `Scripts/Source`。`Global` 的实现与声明不得 `require` `Source` 或引用其类型（包括通过依赖项目的 `GlobalFunctions` Lua 函数库间接引入）；原生根模块 `GlobalFunctions` 属于下层 API，`Scripts/Internal` 与 `Scripts/GlobalFunctions` 可依赖 `Source`。
- **向 `Global` 注入项目行为**：通用基础设施可按数据提供的不透明模块名动态加载模块，但不得硬编码上层业务模块。请在启动时通过 [`Global.Data.InitializeRuntime(options)`](<默认玩法/运行时数据、配置与存档.md#数据服务>) 注入项目回调与类对象。
- **共享常量**：原生资源后缀由 `Engine.ResourceFileConstants` 暴露，地图标识与玩法标签分别定义在 `Source.Configs.MapConstants` 与 `Source.Configs.GameplayConstants` 中，公开枚举位于 `Enums.*`。
- **类与封装**：每个可复用类应独占一个模块并直接返回该类。窗口模块可定义私有 Controller 并通过 `Ui.DefineWindow` 导出 UI（参见[声明式 UI 编写工作流](<声明式 UI/编写工作流.md>)）；工具模块可归拢相关辅助函数。私有成员仅允许通过 `self` 或同实例的 `super(..., self)` 分派访问；访问其他对象时必须使用公开的读取方法、命令或数据，链式访问与动态索引同样遵守该约束。

### 脚本与数据存储边界

- **脚本根目录布局**：游戏根目录仅允许存在一种脚本存储形态——开发阶段使用未归档目录 `Scripts/`，打包发行版使用根级 `Scripts.ldpak` 归档。混用、缺失或无效布局均会导致启动失败。`Entry` 调用 `Engine.setAppName` 后，用户数据目录中的未归档文件会按[运行时资源查找](<../编辑器用户指南/项目设置与资产布局.md#运行时资源查找>)规则覆盖上述任一布局。
- **模块解析与 Lua 加载器**：每个 Lua 状态机均通过 `package.preload` 与动态搜索器暴露项目模块，不会遮蔽原生模块。用户目录中新增的模块会在首次 `require` 时加载；已缓存于 `package.loaded` 的模块不会仅因文件变更而重新执行。Script Mixin 每次执行时均重新加载，音频状态机也读取相同的有效脚本来源。`loadfile` 与 `dofile` 在支持普通物理路径的同时接受 `/Game/Scripts/...`，保留标准返回值、错误与环境语义。通用文件系统 API 不暴露脚本归档内容。
- **数据查找**：未归档目录与归档中的 `Data` 均通过逻辑路径 `Data/...` 与 `/Game/Data/...` 解析。`Engine.loadData` 与 `Engine.dataExists` 会在各来源内将缺失的 `.json` 路径回退为二进制 `.ldc` 文件，且用户数据优先于根目录数据。JSON 平台 API 仅读取精确路径。此接口属于只读运行时边界，并非通用归档文件系统。

## 枚举模块

每个公开枚举在 `Scripts/Enums` 下独占一个模块，常量、EmmyLua 注解与 API 说明直接保留在源码中，不维护镜像 Stub。手写枚举需声明一个以字符串为键、标量字面量为值的局部表并直接返回，不得包含依赖加载、函数定义或类构造：

```lua
-- Scripts/Enums/StairDirection.lua
---@enum Enums.StairDirection
local StairDirection = {
    None = "None",
    Up = "Up",
    Down = "Down",
}

return StairDirection
```

通过 `local StairDirection = require("Enums.StairDirection")` 加载枚举并读取 `StairDirection.Up`。蓝图元数据通过 `type = { enum = "Enums.StairDirection" }` 引用枚举模块，参见[枚举 Schema](<蓝图脚本/元数据结构与 Decorator.md#枚举-schema>)。

| Game 项目枚举模块 | 值或用途 |
|---|---|
| `Enums.StairDirection` | `None`、`Up`、`Down`（以同名字符串存储） |
| `Enums.BattleResultCode` | `WIN = 1`、`CANNOT_DAMAGE = 2`、`LETHAL_COUNTER_DAMAGE = 3` |
| `Enums.CriticalResultCode` | `VALUE = 1`、`NOT_NEEDED = 2`、`UNKNOWN = 3` |
| `Enums.DamageHintLevel` | `NONE = 0`、`BATTLE = 1`、`MAP = 2` |
| `Enums.WindowTransitionProfile` | `DEFAULT = "Default"`、`MENU = "Menu"` |
| `Enums.ShopMode` | `BUY = "buy"`、`SELL = "sell"` |
| `Enums.EventKey` | `LocaleChanged`、`AbilitySystemChanged`、`PlayerChanged` 事件名 |
| `Enums.AbilitySystemChangeKind` | `Attribute`、`State` 载荷类型 |
| `Enums.PlayerChangeKind` | `Inventory`、`Name`、`Map` 载荷类型 |
| `Enums.MovementLatentOutput` | `STARTED = 0`、`FINISHED = 1` 执行输出引脚索引 |
| `Enums.CombatResolutionCode` | [战斗计算的中间结果](<源码类/Gameplay.md#motabattleability>) |
| `Enums.MovementResultCode` | [移动危险查询与提交结果](<源码类/Gameplay.md#movementspecials-与移动危险>) |
| `Enums.PlayerNameValidationError` | [玩家名称校验错误](<源码类/Player.md#lua-运行时-api>) |
| `Enums.SaveLoadMode`、`Enums.SaveLoadCloseReason` | [存读档窗口模式与关闭原因](<默认玩法/运行时数据、配置与存档.md#存档>) |
| `Enums.WorldActorState` | 世界 Actor 生命周期标记：`NEVER_ACTIVE = "NeverActive"`、`ACTIVE = "Active"`、`DORMANT = "Dormant"` |
| `Enums.WorldPublishPhase` | 区域发布阶段标记：`CONVERT = "convert"`、`BUILD = "build"`、`INDEX = "index"`、`FINALISE = "finalise"` |

原生构建会生成 `Enums.Engine.*`、`Enums.GlobalCore.*`、`Enums.GlobalFunctions.*` 与 `Enums.sf.*`。保存项目数据时会生成 `Enums.GeneralDataKey`、每个数据类型对应的 `Enums.GeneralData.<TypeName>`，以及 `Enums.Animation` 与 `Enums.Particle`。编辑器直接从当前项目目录解析这些受管字符串枚举（含未保存的修改），运行时 `require` 则加载生成模块。请以所属原生声明或数据库记录为唯一数据源，修改源定义后重新生成，切勿直接编辑生成的枚举文件。

## 原生模块与值

`os.createDirectories`、`os.removeFile`、`zlib`、`base64` 等 Standard 全局函数由宿主注入，无需 `require`。宿主还会根据 `LUDORK_SAVE_AS_LDC` 设置 `SAVE_AS_LDC` 默认值，项目覆盖方式见[存档](<默认玩法/运行时数据、配置与存档.md#存档>)。`LudorkRuntime` 是原生库而非 Lua 模块——Core 绑定仅通过 `Engine`、`GlobalCore` 与 `GlobalFunctions` 加载，不提供 `Runtime` 或 `CoreSystem` 别名。

LuaSF 注册全局命名空间 `sf`。当 API 期望 `sf.Vector2i`、`sf.Vector2f`、`sf.Color` 等类型时，必须传入精确的原生值（如 `sf.Vector2i.new(...)`、`sf.Vector2f.new(...)`、`sf.Color.new(...)`）；原生容器与普通 Lua 表不会隐式转换为 SFML 向量。

## 异步操作更新

默认的 `Scripts/Entry.lua` 会调用 `GlobalFunctions.NodeGraph.initLatent()` 驱动异步操作更新。自定义入口脚本同样必须调用该函数，即使项目仅使用纯脚本玩法。详见[协作任务与文件批次](<../快速入门/Ludork Lua 进阶.md#协作任务与文件批次>)与[执行流、事件与变量](<蓝图脚本/执行流、事件与变量.md>)。

## 会话生命周期

同一 Lua VM 中的主线程与全部协程共享同一个原生会话。捕获的回调与不透明句柄可跨越协程 `yield` 存活，但不能跨 VM 传递，且会在会话关闭后失效。原生宿主接入方式见[会话与模块生命周期](<../C++ 原生开发/构建与模块布局.md#会话与模块生命周期>)。

## 实时调试

实时调试允许在编辑器启动的游戏中直接修改 Actor 状态（包括运行时动态生成的 Actor）。这些修改均为临时生效，既不会写回项目数据，也不会在实例重建后重新应用。编辑器操作说明见[实时调试](<../编辑器用户指南/运行、调试与打包.md#实时调试>)。

## Lua 热重载

当游戏从编辑器以 Debug 或 Release 模式启动，且直接从未归档目录 `Scripts/` 运行 Lua 源码时，保存修改后在控制台输入 `$r` 即可触发热重载。运行时会在下一次控制台更新时（玩法回调间隙）更新已加载的项目模块与已使用的 Script Mixin，同时保持当前场景、存活对象与 Lua VM 不变。已打包或仅含编译产物的脚本不支持热重载。

### 更新范围与需重启的情形

- **就地更新**：替换已有的具名函数，同时保留模块与类的身份、实例字段、模块数据、常量、默认值及已捕获的非函数值。已缓存的匹配函数引用（含原生回调）会自动指向新函数，允许增删普通全局访问。
- **重建前保持原状**：已有的匿名回调与挂起的协程帧继续运行原有函数体（但内部调用的匹配辅助函数会更新）。构造函数与生命周期回调不会重新执行。已创建的[音频处理器](<默认玩法/资源、音频、视频与特效.md#音频>)继续沿用原代码，直至下次重新播放。
- **需要重启**：修改类默认值、删除已加载模块或已使用的 Script Mixin、更改导出名、修改类基类、切换字段与函数的成员种类、改动原生回调成员、破坏捕获状态兼容性，或使用了无法匹配的自定义 `_ENV`。
- **生效范围与排除项**：应用变更前，运行时会预先编译全部有效 Lua 源码（含用户目录覆盖文件）。`Entry` 仅做检查而不重新执行；新增或尚未加载的模块仍通过 `require` 按需加载。元数据、声明文件、JSON 数据、UI 资产与原生代码均不参与热重载。

> **警告**：语法错误、兼容性冲突或候选代码执行失败时，原有定义保持不变，控制台会输出错误信息；但候选代码执行产生的外部副作用无法回滚。若出错后可能已修改原生资源或共享对象，请重启游戏。模块顶层应仅包含定义与依赖加载，资源创建与订阅应放入显式启动函数或生命周期回调中（参见[入口与模块](<默认玩法/默认模板概述.md#入口与模块>)）。

## 日志与错误

运行时日志统一通过 `Global.Utils.Logging` 输出：`debug` 记录诊断细节，`info` 记录主要阶段，`warning` 记录可恢复的数据问题，`error` 记录不可恢复的失败。记录日志不会抛出异常；需要中止执行时请调用 `error` 或 `assert`。

- **调度回调隔离**：当场景输入/帧回调、Actor 帧回调，或 UI 输入/帧/动画完成回调抛出 Lua 错误时，引擎会中止本次调用并在控制台打印回调名称与 Lua 调用栈。出错回调内部的同步调用随之展开，错误发生前的状态变更保持生效。其他已调度回调继续运行，周期性回调在下一次触发时仍可正常执行；鼠标按下回调出错时，输入视为未消费。
- **启动致命错误与异步任务**：从入口脚本或必需初始化流程逃逸的错误会中止启动。协程与计时器的错误处理规则分别见[协作任务与文件批次](<../快速入门/Ludork Lua 进阶.md#协作任务与文件批次>)与[场景计时器](<全局与 Core 模块/Core Modules/GlobalCore/场景与系统.md#场景计时器>)。
- **运行时提供程序**：引擎提供程序必须在[启动](<默认玩法/默认模板概述.md#入口与模块>)阶段显式注册，不得作为 `require` 的副作用自动注册。提供程序缺失或重复、会话已停止，以及返回值数量或类型不合法都会抛出错误；不返回值与返回单个 `nil` 语义不同。

## 运行时、声明与元数据

Lua 文件按职责划分为三类：

- **`.lua` 运行时模块**：承载运行时行为与仅供实现内部使用的注解。简短的纯常量表与简单配置表与纯数据枚举一致，直接在源码中维护公开注解与文档，无需镜像 Stub；包含业务方法或生命周期行为的模块不适用此例外。
- **`Scripts/stub/**/*.d.lua` 声明文件**：承载其余全部公开文档与 LuaLS 类型声明，不得包含运行时副作用或 `require` 调用。无运行时实现的纯类型声明即使很短也存放于此。原生 API 枚举类型归属各自声明文件（如 `Engine.d.lua`、`LuaSF.d.lua`）。
- **`*_meta.lua` 元数据文件**：仅保存编辑器元数据，绝不作为玩法脚本执行。

可 `require` 的声明文件沿用对应运行时模块的镜像路径，并在文件头声明 `---@meta <module.path>`（例如 `Global/Pool.d.lua` 声明 `---@meta Global.Pool`）。全局声明文件（`Standard.d.lua`、`Class.d.lua`、`LuaSF.d.lua`）与纯声明文件 `Scripts/stub/Source/Configs/EventChangePayload.d.lua` 使用不带模块名的 `---@meta`。源码内自维护的声明省略 `---@meta`。语言服务器直接从工作区读取声明，无需运行时 `require`。

EmmyLua 工作区将 `Scripts/stub` 配置为模块根与库目录。`.emmyrc.json` 中的 `workspace.moduleMap` 将 `Scripts/Enums` 模块映射为 `Enums.*`，并将五个源码自维护模块（`Global.WorldMapConstants`、`Source.Configs.HotKey`、`Source.Configs.Tutorial`、`Source.Configs.MapConstants` 与 `Source.Configs.GameplayConstants`）精确映射至运行时导入路径，其余公开导入均解析到 Stub 声明：

```json
"moduleMap": [
    {
        "pattern": "^Scripts[.]Enums[.](.*)$",
        "replace": "Enums.$1"
    },
    {
        "pattern": "^Scripts[.](Global[.]WorldMapConstants|Source[.]Configs[.](HotKey|Tutorial|MapConstants|GameplayConstants))$",
        "replace": "$1"
    }
]
```

各项目模板与游戏包自带的声明范围见[项目模板](<../快速入门/项目模板.md>)。

Standalone 与 C++ Source 模板均附带 Windows VS Code 启动配置 `Ludork: EmmyLua Attach (Windows)`，用于将 `tangzx.emmylua` 的 `emmylua_attach` 调试器附加至运行中的 `Main.exe`（仅支持 Windows）。C++ Source 模板还包含原生的 `cppvsdbg` 与 `lldb` 启动和附加配置。
