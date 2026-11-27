# GlobalFunctions.Scene

`GlobalFunctions.Scene` 提供地图传送、计时器、新手引导、对话与商店窗口、摄像机控制、地形编辑、动态生成 Actor 及持久化记录等蓝图节点。

**元数据来源**：`Scripts/GlobalFunctions/Scene_meta.lua`

## Scene

### 函数与事件

| 函数 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `GotoMap` | `mapPath: string = ""`, `blockTransition: bool = false`, `position: sf.Vector2i` | — | 传送至 `mapPath` 的 `position` 处。`Transfer` 元数据将 `position` 与 `mapPath` 关联。 |
| `GameOver` | — | — | 触发 Game Over 流程。 |
| `AddTimer` | `interval: float`, `blocking: bool = false` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`TimeUp`（`true`）。等待 `interval` 秒。 |
| `ShowEnemyBook` | — | — | 在允许打开浮层时打开怪物手册窗口。 |
| `ShowTutorial` | `key: string` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Finished`（`true`）。显示 `key` 对应的新手引导。 |
| `ShowMessageByTag` | `name: string`, `message: string`, `refActorTag: string = ""` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`FinishedDialogue`（`true`）。相对于与 `refActorTag` 匹配的 Actor 定位并显示对话消息。 |
| `ShowMessage` | `name: string`, `message: string`, `actor: Engine.Actor` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`FinishedDialogue`（`true`）。相对于 `actor` 定位并显示对话消息。 |
| `ShowVoiceMessageByTag` | `name: string`, `message: string`, `voiceFileName: string`, `refActorTag: string = ""` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`FinishedDialogue`（`true`）。路径约束（`voiceFileName`）：`/Game/Assets/Voices`。 |
| `ShowVoiceMessage` | `name: string`, `message: string`, `voiceFileName: string`, `refActor: Engine.Actor`, `minDistance: float = 64` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`FinishedDialogue`（`true`）。路径约束（`voiceFileName`）：`/Game/Assets/Voices`。 |
| `ShowSelection` | `name: string = ""`, `options: string[] = {}`, `refActorTag: string = ""`, `allowCancel: bool = true` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Selected0`（`0`）、`Selected1`（`1`）、`Selected2`（`2`）、`Selected3`（`3`）、`Cancelled`（`-1`）。 |
| `ShowRefSelection` | `name: string = ""`, `options: string[] = {}`, `refActor: Engine.Actor`, `allowCancel: bool = true` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Selected0`（`0`）、`Selected1`（`1`）、`Selected2`（`2`）、`Selected3`（`3`）、`Cancelled`（`-1`）。 |
| `LockCamera` | — | — | 锁定场景摄像机。 |
| `UnlockCamera` | — | — | 解锁场景摄像机。 |
| `AttachCamera` | `actor: Engine.Actor` | — | 将场景摄像机挂载到 `actor`。 |
| `MoveCamera` | `delta: sf.Vector2f` | — | 按偏移量 `delta` 移动场景摄像机。 |
| `RecordTelepoint` | `mapPath: string = ""`, `x: int = 0`, `y: int = 0`, `tag: string = ""` | — | 在 `mapPath` 上记录一个楼层传送点落点。 |
| `CreateActorFromBPPath` | `bpPath: string = ""`, `layerName: string = "default"`, `position: sf.Vector2i`, `tag: string = ""`, `emitCreateEvent: bool = true` | `actor: Engine.Actor` | `bpPath` 启用蓝图类选择器（`BlueprintClassVars`）。在 `layerName` 图层生成 Actor。 |
| `CreateActorFromBPPathWithDefaults` | `bpPath: string = ""`, `defaults: any = {}`, `layerName: string = "default"`, `position: sf.Vector2i`, `tag: string = ""`, `emitCreateEvent: bool = true` | `actor: Engine.Actor` | `bpPath` 启用蓝图类选择器（`BlueprintClassVars`）。使用覆写默认值在 `layerName` 图层生成 Actor。 |
| `DestroyTerrain` | `layerName: string`, `position: sf.Vector2i`, `tileID: any` | — | 销毁 `layerName` 图层 `position` 处的地形图块。 |
| `DestroyTerrainList` | `layerName: string`, `positions: sf.Vector2i[] = {}`, `tileID: any` | — | 销毁 `layerName` 图层在 `positions` 各坐标处的地形图块。 |
| `GetTerrainTile` | `layerName: string`, `position: sf.Vector2i` | `tileID: any` | 纯函数（Pure）。返回 `layerName` 图层 `position` 处的图块 ID。 |
| `GetTerrainTilePositions` | `layerName: string`, `tileID: any` | `positions: sf.Vector2i[]` | 纯函数（Pure）。返回 `layerName` 图层上所有匹配 `tileID` 的坐标列表。 |
| `RecordAddedActor` | `actor: Engine.Actor` | — | 在地图持久化状态中记录 `actor` 为新增。 |
| `SelfRecordAdded` | — | — | 在地图持久化状态中记录当前蓝图所属 Actor 为新增。 |
| `RecordActorPosition` | `actor: Engine.Actor` | — | 在地图持久化状态中记录 `actor` 的当前位置。 |
| `SelfRecordActorPosition` | — | — | 在地图持久化状态中记录当前蓝图所属 Actor 的当前位置。 |
| `RecordDestroyedActor` | `actor: Engine.Actor` | — | 仅在地图持久化状态中记录 `actor` 已销毁，不销毁实例本身。 |
| `SelfRecordDestroyed` | — | — | 在地图持久化状态中记录当前蓝图所属 Actor 已销毁。 |
| `RecordAndDestroyActor` | `actor: Engine.Actor` | — | 将 `actor` 记录为已销毁并立即销毁它。 |
| `SelfRecordAndDestroy` | — | — | 将当前蓝图所属 Actor 记录为已销毁并立即销毁它。 |
| `OpenPlayerName` | — | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Closed`（`true`）。打开玩家改名窗口。 |
| `OpenShop` | `items: string[] = {}`, `canSell: bool = true`, `actor: Engine.Actor = nil` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Closed`（`true`）。打开物品商店窗口。 |
| `OpenAttrShop` | `actor: Engine.Actor`, `shopName: string = ""`, `shopDescription: string = ""`, `abilities: any = {}`, `price: any = 0`, `priceIncrement: int = 1`, `moneyName: string = "GOLD"` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Closed`（`true`）。为 `actor` 打开属性商店窗口。 |
| `OpenAttrShopByTag` | `actorTag: string = ""`, `shopName: string = ""`, `shopDescription: string = ""`, `abilities: any = {}`, `price: any = 0`, `priceIncrement: int = 1`, `moneyName: string = "GOLD"` | `Engine.AsyncOperation` | 潜伏节点（Latent）。分支：`Closed`（`true`）。为与 `actorTag` 匹配的 Actor 打开属性商店窗口。 |

### Lua 等待与结果

潜伏函数均返回 `Engine.AsyncOperation`，顺序等待的用法见 [Lua 异步任务](<../../快速入门/Ludork Lua 进阶.md>)。新手引导、消息框、计时器与窗口关闭操作在结束时均以 `true` 完成；选项对话框返回从 `0` 开始的选项索引，玩家取消时返回 `-1`，并触发蓝图的 `Cancelled` 分支（不会取消异步操作本身）。

消息框与各类窗口均在关闭动画结束后完成。新的消息或选项请求会取消旧的对话操作；重新打开商店会替换原有操作；改名窗口处于打开状态时，重复请求会共享同一操作。语音播放在所属对话完成或取消时立即停止。场景切换与换图时的清理规则见 [SceneMap](<../源码类/SceneMap.md#脚本操作与生命周期>)。

### 商店 Actor

`OpenShop(items, canSell, actor?)` 借助可选的 Actor 参数记录商店访问身份。在蓝图中省略该参数时，若当前蓝图所属对象为 Actor，系统会自动使用该 Actor；直接从 Lua 调用时，则需通过第三个参数显式传入商店 Actor 才能记录访问。`OpenAttrShop` 直接使用 `actor` 参数，`OpenAttrShopByTag` 会在当前地图查找匹配的 Actor。不具备地图身份的 Actor 不会创建访问记录。上述节点均以 `Closed` 分支完成；访问记录与远程打开规则见[已访问商店](<../默认玩法/窗口、菜单、输入与控件.md#已访问商店>)。

### GotoMap 的大地图目的地

`GotoMap` 支持普通地图、大地图清单与已摆放的子地图：清单使用世界坐标，子地图使用局部坐标。若目标为未摆放的子地图、数据非法或坐标越界，`GotoMap` 会执行失败。进入大地图时会等待目标视口完整就绪；`blockTransition` 仅跳过视觉遮罩，不跳过目标就绪检查。

### ShowEnemyBook

当玩家持有怪物手册且当前场景允许打开浮层窗口时，`ShowEnemyBook()` 会打开当前地图的怪物手册。

### 新手引导

`ShowTutorial(key)` 显示配置好的引导内容，玩家确认后以 `true` 完成。同一键存在尚未完成的请求时，后续请求会共享该操作（即使该键此前已被记录）；否则，已记录过的键会立即完成。不同的键按先后顺序排队展示，传入未知键会报告错误。

当队列头部的请求开始展示时，若对应的本地化文本为空字符串（`""`），系统会跳过展示与记录，并直接经由 `Finished` 分支完成。持久化记录规则见[存档](<../默认玩法/运行时数据、配置与存档.md#存档>)。

引导键及其展示顺序由蓝图决定；`Map_01` 的示例配置见[地图新手引导](<../默认玩法/窗口、菜单、输入与控件.md#地图新手引导>)。

### OpenPlayerName

`OpenPlayerName()` 打开主玩家的改名窗口。无论玩家确认修改还是取消操作，`Closed` 分支都会在关闭动画结束后继续执行。
