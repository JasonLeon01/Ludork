# Source.Scenes.SceneMap

`SceneMap` 是默认玩法的地图场景控制器与公开玩法入口，负责管理当前地图、主玩家、对话与菜单窗口、环境配置以及持久化记录。

**元数据来源**：`Scripts/Source/Scenes/SceneMap_meta.lua`

## Scene

**继承**：`Source.Gameplay.GameplayScene`（详见 [GameplayScene 契约](<Gameplay.md#gameplayscene>)）。

`SceneMap` 作为公开玩法入口持有当前的 `inst` 与 `player`，可通过 `getGameInstance()`、`getPlayer()` 和 `getGameMap()` 读取当前对象。场景负责协调生命周期与每帧执行顺序、地图输入阻塞、Game Over 请求、新手引导及本地化刷新，并直接创建 HUD 与区域标题（Region Title）；其余可选窗口的规则见[窗口、菜单、输入与控件](<../默认玩法/窗口、菜单、输入与控件.md>)。

每个 `SceneMap` 实例分别持有 `Source.Scenes.SceneMap` 下以下四个模块的独立实例：

| 模块 | 状态与职责 |
|---|---|
| `MapSession` | 管理已加载地图、地图路径、区域、构建器及待执行传送；负责地图替换、目的地解析、楼层预览与持久化命令。 |
| `Dialogue` | 管理消息窗口、当前操作、语音播放、移动恢复状态及对话本地化快照。 |
| `Windows` | 管理其余可选窗口、焦点组、窗口操作、移动恢复状态及延后执行的菜单与保存请求。 |
| `Environment` | 管理地图音频与远景配置、返回场景时的环境恢复以及大地图环境光过渡。 |

场景将当前对象传给各组件，或通过 getter 与命令提供访问。各组件独立维护私有状态，玩法脚本应统一通过场景公开入口交互。

加载普通地图时，场景先绑定自身与当前地图身份，再恢复地形与 Actor 记录，最后绑定玩家并执行存活 Actor 的 `onCreate`。替换地图或销毁场景时，会释放离开地图的全部 `ConditionalActor` 订阅，既不派发 Actor 销毁事件，也不修改持久化记录。

### 函数与事件

实例方法省略隐式接收者 `self`（`Source.Scenes.SceneMap`，元数据类名为 `Scene`）；在蓝图中，该参数作为首个目标引脚暴露。

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `getGameMap` | — | `gameMap: Global.GameMap` | 纯函数（Pure）。返回已加载的 `GameMap` 或兼容的 `WorldGameMap`。 |
| `openPlayerName` | — | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Closed`（`true`）。打开玩家改名窗口。 |
| `showMessage` | `name: string`, `message: string`, `refActor: Engine.Actor`, `localeArgs: Dict[string, any] = {}` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`FinishedDialogue`（`true`）。 |
| `showSelection` | `name: string`, `options: string[]`, `refActor: Engine.Actor`, `allowCancel: bool = true`, `localeArgs: Dict[string, any] = {}` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Selected0`（`0`）、`Selected1`（`1`）、`Selected2`（`2`）、`Selected3`（`3`）、`Cancelled`（`-1`）。 |
| `applyLoadedGame` | `inst: Source.GameInstance` | — | 按 `inst` 重新绑定玩家、HUD 与窗口，并强制重新加载地图。默认执行出口。 |
| `showEnemyBook` | — | — | 打开怪物手册窗口。默认执行出口。 |
| `showFloorTeleporter` | — | — | 打开楼层传送器窗口。默认执行出口。 |
| `showShopRecords` | — | — | 打开已访问商店列表（Lua 调用方会收到表示是否打开成功的布尔值）。默认执行出口。 |
| `openMenu` | — | — | 打开游戏内菜单。默认执行出口。 |
| `recordAddedActor` | `actor: Engine.Actor` | — | 在地图持久化状态中记录新增的 Actor。默认执行出口。 |
| `recordActorPosition` | `actor: Engine.Actor`, `position: sf.Vector2i = nil` | — | 记录 Actor 当前位置；传入显式 `position` 时记录指定位置。默认执行出口。 |
| `recordDestroyedActor` | `actor: Engine.Actor` | — | 在地图持久化状态中记录已销毁的 Actor。默认执行出口。 |

### 脚本操作与生命周期

`showShopRecords()` 按照与按键 `V` 相同的条件打开[已访问商店列表](<../默认玩法/窗口、菜单、输入与控件.md#已访问商店>)，并向 Lua 返回表示是否成功打开的布尔值。`openShop(buyItemIDs, canSell, actor?)` 接受可选的商店 Actor 参数用于记录访问；直接调用场景方法时需显式传入该 Actor。

`showMessage`、`showSelection`、`openPlayerName`、`openShop` 和 `openAttrShop` 均返回 `Engine.AsyncOperation`。完成结果与窗口替换规则与 [Scene 节点](<../节点函数/Scene.md#lua-等待与结果>)一致。

Lua 还可调用 `showVoiceMessage(name, message, voiceFileName, refActor, minDistance, spatial)`。当 `spatial = true` 时，语音以 `refActor` 为声源位置并将 `minDistance` 传给音频管理器；为 `false` 时播放非空间语音。无论是否启用空间音频，`refActor` 均用于定位消息窗口。

若 Lua 包装函数需要传入节点局部格式化变量，可调用 `showMessageWithContext`、`showSelectionWithContext` 或 `showVoiceMessageWithContext`。它们在对应普通方法的参数之后追加 `localVars: table<string, any>`，对话启动时会复制该表，并返回相同的 `Engine.AsyncOperation`。普通场景方法与 Scene 节点会在委派调用前捕获当前节点上下文；`WithContext` 方法不提供蓝图节点。

退出场景、销毁场景或替换地图时，系统会取消所有未完成的新手引导、对话、商店、改名窗口与场景计时器，关闭窗口并解除输入阻塞，同时关闭已访问商店列表并释放远程商店 Actor。离开地图的 Actor 上已注册的操作（包括移动与门体动画）也会一并取消；等待中的 Lua 任务与蓝图事件随即中止，不再执行后续语句或完成分支。

退出或销毁场景还会丢弃延后执行的菜单、存读档、快速保存请求及已排队的地图传送。取消排队中的传送会解除背景冻结，并恢复该请求所捕获玩家的移动状态。`applyLoadedGame(inst)` 与 `applyPrimaryPlayer()` 会先取消旧交互及待执行请求，再重新绑定玩家、HUD 与现有窗口并强制重新加载地图，因此移动恢复仅作用于离场玩家，不会改动新绑定玩家的移动状态。

压入其他场景时，系统会停止当前地图的 BGM/BGS 并清除天气与远景。通过 `SceneManager.popScene()` 返回时，系统先清除离场场景的 BGM/BGS 与天气，再重新应用地图配置的音频与远景：普通地图使用加载时捕获的配置，大地图遵循[组合式大地图环境规则](<../默认玩法/资源、音频、视频与特效.md#组合式大地图环境>)。音乐按地图过滤器的起始偏移与循环区间重新开始播放，不续接离场前的进度；脚本临时修改的音乐、过滤器、天气与远景也不会恢复。

地图、Actor、迷雾及环境光过渡状态在返回后继续保持存活，不会重新加载地图，也不会重复执行 Actor 的 `onCreate`。

### 组合式大地图行为

`getGameMap` 返回 `Global.GameMap` 或兼容的 `WorldGameMap`。`gotoMapAndPos` 接受清单路径加世界坐标，或已摆放子地图路径加局部坐标，并将世界清单路径保存为主玩家所在地图。`blockTransition` 仅跳过转场遮罩，不跳过目标就绪检查。

大地图的 Actor 记录归属世界清单，地形改动则按子地图路径与局部格坐标存储。`recordActorPosition` 会更新已移动根 Actor 的归属关系；可选参数 `position` 默认为 `nil`（记录当前位置），显式传入 `sf.Vector2i` 时记录指定位置。`recordDestroyedActorTag` 会释放匹配到的驻留项的条件订阅并抑制其子树，不派发销毁事件也不触发子地图 I/O；`recordDestroyedActor` 仅写入记录。
