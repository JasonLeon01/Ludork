# GlobalFunctions.Scene

`Scene` 声明地图传送、对话、窗口、摄像机、地形与持久化记录节点。

Metadata 来源：`Scripts/GlobalFunctions/Scene_meta.lua`

## Scene

直接 metadata 基类：—

### 属性

未声明任何可编辑属性。

### 函数与事件

| 名称 | 种类 | 参数 | 返回 | 执行语义与 metadata |
|---|---|---|---|---|
| `GotoMap` | `function` | mapPath: string = ""; blockTransition: bool = false; position: sf.Vector2i | — | ExecSplit { [1] = "default", default = nil }; Meta { Transfer = { [1] = { [1] = "position", [2] = "mapPath" } } } |
| `GameOver` | `function` | — | — | ExecSplit { [1] = "default", default = nil } |
| `AddTimer` | `function` | interval: float; blocking: bool = false | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "TimeUp", TimeUp = { [1] = true } } |
| `ShowEnemyBook` | `function` | — | — | ExecSplit { [1] = "default", default = nil } |
| `ShowTutorial` | `function` | key: string | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Finished", Finished = { [1] = true } } |
| `ShowMessageByTag` | `function` | name: string; message: string; refActorTag: string = "" | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "FinishedDialogue", FinishedDialogue = { [1] = true } } |
| `ShowMessage` | `function` | name: string; message: string; actor: { "Engine", "Actor" } | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "FinishedDialogue", FinishedDialogue = { [1] = true } } |
| `ShowVoiceMessageByTag` | `function` | name: string; message: string; voiceFileName: string; refActorTag: string = "" | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "FinishedDialogue", FinishedDialogue = { [1] = true } }; Meta { PathVars = { [1] = { [1] = "voiceFileName", [2] = "/Game/Assets/Voices" } } } |
| `ShowVoiceMessage` | `function` | name: string; message: string; voiceFileName: string; refActor: { "Engine", "Actor" }; minDistance: float = 64 | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "FinishedDialogue", FinishedDialogue = { [1] = true } }; Meta { PathVars = { [1] = { [1] = "voiceFileName", [2] = "/Game/Assets/Voices" } } } |
| `ShowSelection` | `function` | name: string = ""; options: string[] = {  }; refActorTag: string = ""; allowCancel: bool = true | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Selected0", [2] = "Selected1", [3] = "Selected2", [4] = "Selected3", [5] = "Cancelled", Selected0 = { [1] = 0 }, Selected1 = { [1] = 1 }, Selected2 = { [1] = 2 }, Selected3 = { [1] = 3 }, Cancelled = { [1] = -1 } } |
| `ShowRefSelection` | `function` | name: string = ""; options: string[] = {  }; refActor: { "Engine", "Actor" }; allowCancel: bool = true | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Selected0", [2] = "Selected1", [3] = "Selected2", [4] = "Selected3", [5] = "Cancelled", Selected0 = { [1] = 0 }, Selected1 = { [1] = 1 }, Selected2 = { [1] = 2 }, Selected3 = { [1] = 3 }, Cancelled = { [1] = -1 } } |
| `LockCamera` | `function` | — | — | ExecSplit { [1] = "default", default = nil } |
| `UnlockCamera` | `function` | — | — | ExecSplit { [1] = "default", default = nil } |
| `AttachCamera` | `function` | actor: { "Engine", "Actor" } | — | ExecSplit { [1] = "default", default = nil } |
| `MoveCamera` | `function` | delta: sf.Vector2f | — | ExecSplit { [1] = "default", default = nil } |
| `RecordTelepoint` | `function` | mapPath: string = ""; x: int = 0; y: int = 0; tag: string = "" | — | ExecSplit { [1] = "default", default = nil } |
| `CreateActorFromBPPath` | `function` | bpPath: string = ""; layerName: string = "default"; position: sf.Vector2i; tag: string = ""; emitCreateEvent: bool = true | actor: { "Engine", "Actor" } | ExecSplit { [1] = "default", default = nil }; Meta { BlueprintClassVars = { [1] = "bpPath" } } |
| `CreateActorFromBPPathWithDefaults` | `function` | bpPath: string = ""; defaults: any = {  }; layerName: string = "default"; position: sf.Vector2i; tag: string = ""; emitCreateEvent: bool = true | actor: { "Engine", "Actor" } | ExecSplit { [1] = "default", default = nil }; Meta { BlueprintClassVars = { [1] = "bpPath" } } |
| `DestroyTerrain` | `function` | layerName: string; position: sf.Vector2i; tileID: any | — | ExecSplit { [1] = "default", default = nil } |
| `DestroyTerrainList` | `function` | layerName: string; positions: sf.Vector2i[] = {  }; tileID: any | — | ExecSplit { [1] = "default", default = nil } |
| `GetTerrainTile` | `function` | layerName: string; position: sf.Vector2i | tileID: any | Pure |
| `GetTerrainTilePositions` | `function` | layerName: string; tileID: any | positions: sf.Vector2i[] | Pure |
| `RecordAddedActor` | `function` | actor: { "Engine", "Actor" } | — | ExecSplit { [1] = "default", default = nil } |
| `SelfRecordAdded` | `function` | — | — | ExecSplit { [1] = "default", default = nil } |
| `RecordActorPosition` | `function` | actor: { "Engine", "Actor" } | — | ExecSplit { [1] = "default", default = nil } |
| `SelfRecordActorPosition` | `function` | — | — | ExecSplit { [1] = "default", default = nil } |
| `RecordDestroyedActor` | `function` | actor: { "Engine", "Actor" } | — | ExecSplit { [1] = "default", default = nil } |
| `SelfRecordDestroyed` | `function` | — | — | ExecSplit { [1] = "default", default = nil } |
| `RecordAndDestroyActor` | `function` | actor: { "Engine", "Actor" } | — | ExecSplit { [1] = "default", default = nil } |
| `SelfRecordAndDestroy` | `function` | — | — | ExecSplit { [1] = "default", default = nil } |
| `OpenPlayerName` | `function` | — | return: { "Engine", "AsyncOperation" } | Latent Closed |
| `OpenShop` | `function` | items: string[] = {  }; canSell: bool = true | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Closed", Closed = { [1] = true } } |
| `OpenAttrShop` | `function` | actor: { "Engine", "Actor" }; shopName: string = ""; shopDescription: string = ""; abilities: any = {  }; price: any = 0; priceIncrement: int = 1; moneyName: string = "GOLD" | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Closed", Closed = { [1] = true } } |
| `OpenAttrShopByTag` | `function` | actorTag: string = ""; shopName: string = ""; shopDescription: string = ""; abilities: any = {  }; price: any = 0; priceIncrement: int = 1; moneyName: string = "GOLD" | return: { "Engine", "AsyncOperation" } | Latent true; LatentStates { [1] = "Closed", Closed = { [1] = true } } |

### Lua 等待与结果

潜伏函数返回 `Engine.AsyncOperation`，顺序等待用法见 [Lua 异步任务](<../../快速入门/Ludork Lua 进阶.md>)。教程、消息、计时器和窗口关闭操作以 `true` 完成。选项返回从 0 开始的索引，玩家取消返回 `-1` 并进入蓝图 `Cancelled` 出口，不等同于取消操作对象。

消息与窗口在关闭动画结束后完成。新的消息或选项取消旧对话操作；重新打开商店会替换其操作，重复请求已打开的改名窗口则共享操作。语音在所属对话完成或取消时停止。场景与换图清理规则见 [SceneMap](<../源码类/SceneMap.md#脚本操作与生命周期>)。

### GotoMap 的大地图目的地

`GotoMap` 可接受普通地图、大地图清单和已摆放的子地图。清单中的位置是大地图坐标，子地图中的位置是局部坐标。目标为未摆放的子地图、数据非法或位置越界时，`GotoMap` 会失败。进入大地图时会等待目标视口完整就绪。`blockTransition` 只跳过视觉遮罩，不跳过就绪检查。

### ShowEnemyBook

玩家已持有怪物手册，且当前场景允许打开浮层时，`ShowEnemyBook()` 打开当前地图的怪物手册。

### 新手引导

`ShowTutorial(key)` 展示配置的引导，确认后以 `true` 完成。同一 key 的未完成请求共享操作，即使该 key 已被记录；没有未完成请求时，已记录 key 立即完成。不同 key 按顺序排队，未知 key 报错。

请求出队时，本地化文本为空字符串（`""`）会跳过展示与记录，直接继续 `Finished`。记录规则见[存档](<../Game 玩法/运行时数据、配置与存档.md#存档>)。

蓝图负责选择引导 key 及其执行顺序。`Map_01` 示例将有序 key 数组传给 `ForEach`，在循环体内调用 `ShowTutorial`。配置说明见[地图新手引导](<../Game 玩法/窗口、菜单、输入与控件.md#地图新手引导>)。

### OpenPlayerName

`OpenPlayerName()` 打开主 Player 的改名窗口。无论确认还是取消改名，关闭动画结束后都会继续 `Closed` 分支。
