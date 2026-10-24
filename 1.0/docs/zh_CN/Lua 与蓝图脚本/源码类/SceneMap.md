# Source.Scenes.SceneMap

`SceneMap` 声明地图 Scene 的玩法命令、对话窗口、菜单入口与持久化记录。

Metadata 来源：`Scripts/Source/Scenes/SceneMap_meta.lua`

## Scene

直接 metadata 基类：`{ "Source.Gameplay.GameplayScene", "GameplayScene" }`

玩法命令遵循 [GameplayScene 契约](<Gameplay.md#gameplayscene>)。

`SceneMap` 是公开的玩法入口，持有当前的 `inst` 与 `player`。`getGameInstance()`、`getPlayer()` 和 `getGameMap()` 返回当前对象。Scene 协调生命周期和每帧执行顺序、地图输入屏蔽、Game Over 请求、教程及语言刷新，并直接创建 HUD 与 Region Title；可选窗口遵循[窗口、菜单、输入与控件](<../Game 玩法/窗口、菜单、输入与控件.md>)中的规则。

每个 Scene 分别持有 `Source.Scenes.SceneMap` 下这些模块的实例：

| 模块 | 状态与职责 |
|---|---|
| `MapSession` | 已加载地图、地图路径、区域、构建器及待执行传送；负责替换地图、解析目的地、楼层预览和持久化命令。 |
| `Dialogue` | 消息窗口、当前操作、语音、移动恢复状态及对话本地化快照。 |
| `Windows` | 其他可选窗口、焦点组、窗口操作、移动恢复状态及延后执行的菜单与保存请求。 |
| `Environment` | 地图音频与远景配置、返回场景时的环境恢复，以及大地图环境光过渡。 |

Scene 将当前对象作为参数传给组件，或通过 getter 和命令提供访问。各组件独立持有私有状态；玩法脚本使用 Scene 的公开入口。

加载普通地图时，先绑定 Scene 与当前地图身份，再恢复地形与 Actor 记录，最后绑定 Player 并执行存活 Actor 的 `onCreate`。替换地图或销毁 Scene 时，会释放离开地图的 ConditionalActor 订阅，既不派发 Actor 销毁事件，也不改动持久记录。

### 函数与事件

| 名称 | 种类 | 参数 | 返回 | 执行语义与 metadata |
|---|---|---|---|---|
| `getGameMap` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self" | gameMap: { "Global.GameMap", "GameMap" } | Pure |
| `openPlayerName` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self" | return: { "Engine", "AsyncOperation" } | Latent Closed |
| `showMessage` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; name: string; message: string; refActor: { "Engine", "Actor" }; localeArgs: Dict[string, any] = {  } | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "FinishedDialogue", FinishedDialogue = { [1] = true } } |
| `showSelection` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; name: string; options: string[]; refActor: { "Engine", "Actor" }; allowCancel: bool = true; localeArgs: Dict[string, any] = {  } | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Selected0", [2] = "Selected1", [3] = "Selected2", [4] = "Selected3", [5] = "Cancelled", Selected0 = { [1] = 0 }, Selected1 = { [1] = 1 }, Selected2 = { [1] = 2 }, Selected3 = { [1] = 3 }, Cancelled = { [1] = -1 } } |
| `applyLoadedGame` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; inst: { "Source.GameInstance", "GameInstance" } | — | ExecSplit { [1] = "default", default = nil } |
| `showEnemyBook` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self" | — | ExecSplit { [1] = "default", default = nil } |
| `showFloorTeleporter` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self" | — | ExecSplit { [1] = "default", default = nil } |
| `openMenu` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self" | — | ExecSplit { [1] = "default", default = nil } |
| `recordAddedActor` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; actor: { "Engine", "Actor" } | — | ExecSplit { [1] = "default", default = nil } |
| `recordActorPosition` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; actor: { "Engine", "Actor" }; position: sf.Vector2i = nil | — | ExecSplit { [1] = "default", default = nil } |
| `recordDestroyedActor` | `function` | self: { "Source.Scenes.SceneMap", "Scene" } = "self"; actor: { "Engine", "Actor" } | — | ExecSplit { [1] = "default", default = nil } |

### 脚本操作与生命周期

`showMessage`、`showSelection`、`openPlayerName`、`openShop` 和 `openAttrShop` 返回 `Engine.AsyncOperation`。完成结果与窗口替换规则同 [Scene 节点](<../节点函数/Scene.md#lua-等待与结果>)。

Lua 还可调用 `showVoiceMessage(name, message, voiceFileName, refActor, minDistance, spatial)`。`spatial = true` 时，语音以 `refActor` 定位，并将 `minDistance` 传给音频管理器；传 `false` 时播放非空间语音。参考 Actor 仍用于定位消息窗口。

需要传入节点局部格式化变量的 Lua 包装函数，可以调用 `showMessageWithContext`、`showSelectionWithContext` 或 `showVoiceMessageWithContext`。它们在对应普通方法的参数之后追加 `localVars: table<string, any>`，在开始对话时复制这张表，并返回相同的 `Engine.AsyncOperation`。普通 Scene 方法和 Scene 节点在转交调用前捕获当前节点上下文；`WithContext` 方法不提供蓝图节点。

退出场景、销毁场景和替换地图会取消未完成的教程、对话、商店、改名窗口及场景计时器，关闭窗口并解除输入阻塞。离开地图的 Actor 所注册的操作（包括移动与门动画）也会取消。等待中的 Lua 任务与蓝图事件随之停止，不执行后续语句或完成出口。

退出或销毁场景还会丢弃延后执行的菜单、存读档、快速保存请求及已排队的地图传送。取消排队中的传送时，会解除对应的背景冻结，并恢复请求发起时保存的玩家移动状态。`applyLoadedGame(inst)` 和 `applyPrimaryPlayer()` 先取消旧交互及待执行请求，再重新绑定玩家、HUD 和已有窗口，并强制重新加载地图。因此，移动恢复作用于离开的玩家，不会改写新绑定玩家的移动状态。

压入另一个 Scene 时，会停止地图 BGM/BGS 并清除天气和远景。通过 `SceneManager.popScene()` 返回时，先清除离场 Scene 的 BGM/BGS 和天气，再重新应用地图配置中的音频与远景。普通地图使用加载时保存的配置；大地图遵循[组合式大地图环境规则](<../Game 玩法/资源、音频、视频与特效.md#组合式大地图环境>)。音乐按地图过滤器的起始偏移与循环区间重新播放，不续接离场时的播放位置。脚本临时修改的音乐、过滤器、天气和远景不会恢复。

原有地图、Actor、雾和环境光过渡状态继续保留。返回不会重新加载地图或再次执行 Actor 的 `onCreate`。

### 组合式大地图行为

`getGameMap` 返回 `GameMap` 或兼容的 `WorldGameMap`。`gotoMapAndPos` 接受“清单 + 大地图坐标”或“已摆放子地图 + 局部坐标”，并把清单存为主控 player 的地图。`blockTransition` 跳过遮罩，但不跳过目标就绪检查。

大地图的 Actor 记录使用清单。地形使用子地图路径与局部格。`recordActorPosition` 更新已移动根 Actor 的归属。可选的 `position` 输入默认为 nil，表示记录 Actor 的当前位置；显式传入 `sf.Vector2i` 时记录指定位置。`recordDestroyedActorTag` 释放匹配到的驻留项的条件订阅，并抑制其子树，不产生销毁事件，也不做子地图 I/O。`recordDestroyedActor` 仍然只负责记录。
