# Global.GameMap

`Global.GameMap` 声明项目地图层级的 API，涵盖玩家与 Actor 查询、Actor 创建与销毁、通行判定、摄像机绑定、图块地图、地形图块以及地图环境。本页实例方法表格省略隐式 `self` 接收者（`Global.GameMap.GameMap`），蓝图节点将其作为首个目标引脚暴露。

**元数据来源**：`Scripts/Global/GameMap_meta.lua`

## 地图数据与区域构建器

共享地图数据类型声明位于各自对应 stub 的 `Global.GameMap.*` 与 `Global.WorldGameMap.*` 下。这些名称仅用于 Lua 类型注解，不新增构造函数，也不改变地图文件格式。

| 命名空间 | 类型 |
|---|---|
| `Global.GameMap` | `MapDataBase`、`SerializedMapData`、`MapData`、`MapLayerData`、`MapEnvironmentData`、`SerializedMapFogSettings` |
| `Global.GameMap` | `SerializedTileCell`、`SerializedAutoTileCell`、`SerializedTileRow`、`SerializedAutoTileRow` |
| `Global.WorldGameMap` | `WorldMapData`、`WorldRegionData`、`WorldRegionEnvironmentData` |

`SerializedMapData` 描述读入的地图记录，`MapData` 描述规范化后的 Actor 数据与原生环境值。项目模块 `Source.SceneComponents.MapBuilder.MapDataParser` 为普通地图与流式子地图提供相同的解析操作：

- `ValidateMapData(data)` 按[地图图层顺序规则](<../../../编辑器用户指南/图块集、自动图块与地图.md#图块集与自动图块>)校验 `Global.GameMap.SerializedMapData` 记录。数据无效时抛出错误，且不修改传入记录。
- `NormaliseActorData(actorData)` 返回新的 `Global.Data.ActorData` 记录，保留 `bp` 与 `tag`，并将 `position` 转换为原生无符号格子坐标（`sf.Vector2u`）。位置或其中某个坐标缺失时默认为零。

普通地图的 `NormaliseMap` 会依次执行这两项操作。流式区域在构建启动时校验地图数据，随后按增量构建逐步规范化每个预置 Actor，而不在初始化阶段一次性处理全部 Actor。

大地图门面接收 `WorldMapData` 与按以下签名注入的区域工厂：

```lua
---@param region Global.WorldGameMap.WorldRegionData
---@param data Global.GameMap.SerializedMapData
---@param priorityRect Global.WorldGeometry.CellRect | nil
---@return Global.WorldGameMap.RegionBuildState
```

`RegionBuildState` 暴露构建进度、区域载荷以及 `step`、`prepareActors`、`prepareRect`、`areActorsReady`、`isCellReady` 和 `isRectReady` 方法。`Global` 通过该接口消费构建器，无需导入项目侧构建器实现。`Source.SceneComponents.MapBuilder.WorldRegionBuildState.State` 继承该接口，并扩展类型为 `Source.SceneComponents.WorldLayerBuildState` 数组的 `layerBuildStates` 字段。项目 Actor 描述符、持久化记录、图层构建状态与楼层预览缓存均留在 `Source` 中。

区域构建器还须实现资源生命周期契约：

| 成员 | 契约 |
|---|---|
| `waitingForResources` | 上一次构建步骤因请求资源仍处于挂起状态而返回时为 `true`。后续步骤可继续重试；等待不会将区域标记为就绪。 |
| `setResourcePriority(priority)` | 更新当前挂起订阅以及后续发现依赖的优先级。大地图对 Active 或显式强制加载区域传入 `1`，对 Prepared 区域传入 `0`。 |
| `resolvePendingResources()` | 在显式清空流程中同步完成已发现的待加载资源。此操作不推进图层构建、Actor 或发布，它们仍由正常构建步骤推进。 |
| `cancel()` | 释放资源订阅并关闭构建协程。取消发布、淘汰区域或销毁大地图时可重复调用；已取消的构建器不能再次推进。 |

地图与区域构建期间的纹理加载遵循以下规则：

- **普通地图**：先跨所有图层汇集去重后的图块集与自动图块纹理并统一发起请求，再在构建图层前等待解析完成。
- **流式区域**：在扫描依赖时按发现顺序请求图块集与自动图块纹理，包括存档地形修改引入的自动图块。默认构建器先收集各图层依赖再构建图层，复用重复请求，并在纹理就绪后才读取自动图块帧数。请求、错误与内存语义由 [TextureManager](<../Core Modules/GlobalCore/资源管理器.md#texturemanager>) 负责。
- **后台流送预算**：遇到资源等待时结束当前构建步骤，同一逻辑更新内等待中的区域最多尝试一次，以保证其他区域保留预算。必需纹理加载失败会直接向上传播，不会生成不完整图层。
- **同步清空**：`ensureRegionLoadedAt` 与显式视口准备在推进构建器的同步流程中解析待加载资源，而后台准备沿用常规时间预算。

## GameMap

**继承**：`GlobalCore.GameMapBase`

`getGameMap` 的输出可连接到声明为 `GlobalCore.GameMapBase` 的参数，例如 `Camera.setMap`。`setTerrainTile` 与 `setTerrainTiles` 会修改地形，因此在蓝图中带有执行引脚，并通过结果引脚返回改动情况。

### 属性

| 属性 | 类型 | 默认值 | 说明 |
|---|---|---|---|
| `HideDisconnectedRegions` | `bool` | `false` | — |
| `DefaultCoverAlpha` | `int` | — | — |
| `MapViewRect` | `sf.IntRect` | `[192, 32, 416, 416]` | — |

### 组合式大地图门面

对于大地图清单，`WorldGameMap` 对外提供世界坐标，底层由原生稀疏区域索引为已摆放子地图提供通行性、Dir4、材质与地形数据。空洞区域可通行，查询操作不会加载远处子地图。

- `getTilemap()` 返回稀疏底层存储，而非扁平化后的大地图。
- `getActorByTag` 不执行 I/O，仅在 Active 或曾经激活过的驻留根节点中查找。
- `getGameplayCellRect()` 返回复制后的格子矩形而不触发区域加载：普通地图返回完整边界，大地图返回 Active 边界（Active 区域尚未建立时退回可见边界）。
- `prepareViewportAt` 统一排队加载目标摄像机邻域内的未加载子地图，复用进行中的读取请求，完成所需 Actor 与区域的发布，并在绘制目的地后返回。外围摆放区域继续异步流送；必需子地图、Actor 或发布环节失败时，错误会向上传播。

### 渲染与雾效

普通地图、组合式大地图与楼层预览都将地图内容交由共用的原生 `MapRenderer` 绘制。游戏适配层保留 Actor 回调、粒子、Billboard、图层特效以及普通地图的玩家遮挡。大地图按全局图层顺序绘制各区域，Actor 光源与图块遮光共享世界坐标，因此光照可跨越摆放边界。

地图光源来自 `Actor.lightComp`，环境光由 `getAmbientLight` 与 `setAmbientLight` 控制。

每个 `GameMap` 各自持有一个独立的雾控制器。运行时 Lua 方法 `getFogSettings()` 返回 `Engine.MapFogSettings` 的副本，`setFogSettings(settings)` 替换当前地图的雾配置。`getRegionFogSettings(path)` 返回大地图子区域的雾配置，无驻留配置时返回 `nil`。

> **注意**：直接修改 `getFogSettings()` 返回的配置副本不会影响地图。修改完成后请将其传给 `setFogSettings(settings)`。

### Actor 可见性

`getAllActors` 与标签查询会包含隐藏实例。位置查询、范围查询、碰撞、重叠（Overlap）与寻路均排除因自身或祖先 `visible` 标记为 `false` 而隐藏的 Actor，渲染也会排除它们附带的光源与遮光效果。

Game 项目会将隐藏 Actor 从敌人列表与移动危险判定中过滤掉。可见性变更后，下一次查询前即会刷新危险缓存，无需等待 Tick。

### 非连通区域隐藏

`GameMap.HideDisconnectedRegions` 为普通有限地图启用原生区域隐藏。可通过 [Gameplay 中的 `hideDisconnectedRegions`](<../../默认玩法/运行时数据、配置与存档.md#共享玩法配置>) 配置启动默认值，或在构造地图前设置此模块默认值；已有地图可调用 `setHideDisconnectedRegions(enabled)` 控制。`GameMap` 对 `sparseWorldConfig` 跳过该默认开关；若直接对稀疏大地图调用原生设置方法并传入 `true`，将抛出错误。

图块连通性与替换遵循以下规则：

- **连通性**：仅依据图块通行性按四方向连通划分区域，每格由最上层可见且包含图块内容的图层决定通行性。地图边缘限定连通范围，门与其他 Actor 不划分区域。
- **隐藏格替换**：玩家所在区域保持可见，其他可通行格显示为最近不可通行格的图块图层（采用曼哈顿距离，等距时按自上而下、自左向右打破平局），并保留源格纹理、自动图块形态与动画。不可通行格保持原样显示。
- **缺失观察者**：当观察者无效时（无玩家、坐标越界或观察格不可通行），所有可通行格与全部 Actor 均隐藏，仅墙格保持可见。预览模式会显式提供观察格。

Actor 可见性遵循以下几何规则：

- **可通行锚点**：锚点位于可通行格上的 Actor 跟随其锚点格所属区域显示或隐藏。
- **不可通行锚点**：将 Actor 的局部图像矩形依次应用位置、平移、原点、旋转与缩放变换，所得四边形必须与可见区域内的可通行格产生正面积交集才显示（仅相切于边界不算覆盖）。完全位于墙内或面积为零的图像保持隐藏。借此规则，墙上火把可随画面伸入的房间一并显隐。
- **层级与范围**：判定基于几何矩形而非逐像素透明度；跨越多个区域的图像只要与可见区域重叠即完整绘制。每个祖先都须各自通过该规则及自身 `visible` 标记检查，父级隐藏则所有后代一并隐藏。

进入区域时显示，离开后再次隐藏，探索状态不写入存档。渲染会抑制隐藏区域内的 Actor 及其后代、附属粒子与光源以及隐藏格上的伤害文字，且不改变 Actor 的 `visible` 标记、地形数据、碰撞、寻路或事件。怪物手册在分组前通过 `isActorVisibleOnMap(actor)` 进行过滤。

地形编辑或观察者移动后，可见性查询会立即刷新，无需等待渲染帧。原生观察者与可见性 API 见 [GameMapBase](<../Core Modules/GlobalCore/地图与雾效.md#gamemapbase>)。

### 函数与事件

| 方法 | 参数 | 返回值 | 说明 |
|---|---|---|---|
| `getPlayer` | — | `player: Engine.Actor` | 纯函数（Pure）。 |
| `setPlayer` | `player: Engine.Actor` | — | 执行引脚（`default`）。 |
| `getAllActors` | — | `actors: Engine.Actor[]` | 纯函数（Pure）。 |
| `getActorsByPosition` | `position: sf.Vector2i` | `actors: Engine.Actor[]` | 纯函数（Pure）。 |
| `getActorByLayerAndPosition` | `layer: string`, `position: sf.Vector2i` | `actor: Engine.Actor` | 纯函数（Pure）。 |
| `getActorsByRange` | `position: sf.Vector2i`, `radius: int` | `actors: Engine.Actor[]` | 纯函数（Pure）。 |
| `getActorByTag` | `tag: string` | `actor: Engine.Actor` | 纯函数（Pure）。 |
| `isPassable` | `actor: Engine.Actor`, `targetPosition: sf.Vector2i` | `passable: bool` | 纯函数（Pure）。 |
| `spawnActor` | `actor: Engine.Actor`, `layer: string`, `emitCreateEvent: bool = true` | — | 执行引脚（`default`）。 |
| `createActor` | `actorClass: Engine.Actor`, `layer: string`, `kwargs: any`, `emitCreateEvent: bool = true` | `actor: Engine.Actor` | 执行引脚（`default`）。 |
| `destroyActor` | `actor: Engine.Actor` | — | 执行引脚（`default`）。 |
| `getCamera` | — | `camera: GlobalCore.Camera` | 纯函数（Pure）。 |
| `setCamera` | `camera: GlobalCore.Camera` | — | 执行引脚（`default`）。 |
| `getTilemap` | — | `tilemap: Engine.Tilemap` | 纯函数（Pure）。 |
| `getTerrainTile` | `layerName: string`, `position: sf.Vector2i` | `tileID: any` | 纯函数（Pure）。 |
| `getTerrainTilePositions` | `layerName: string`, `tileID: any` | `positions: sf.Vector2i[]` | 纯函数（Pure）。 |
| `setTerrainTile` | `layerName: string`, `position: sf.Vector2i`, `tileID: any` | `success: bool` | 执行引脚（`default`）。 |
| `setTerrainTiles` | `layerName: string`, `positions: sf.Vector2i[]`, `tileID: any` | `positions: sf.Vector2i[]` | 执行引脚（`default`）。 |
| `getAmbientLight` | — | `ambientLight: sf.Color` | 纯函数（Pure）。 |
| `setAmbientLight` | `ambientLight: sf.Color` | — | 执行引脚（`default`）。 |
| `getSize` | — | `size: sf.Vector2u` | 纯函数（Pure）。 |
| `getTopMaterial` | `pos: sf.Vector2i` | `topMaterial: string?` | 纯函数（Pure）。 |
| `findPath` | `start: sf.Vector2i`, `goal: sf.Vector2i`, `actor: Engine.Actor`, `excludedAnchors: sf.Vector2i[] = {}` | `path: sf.Vector2i[]` | 纯函数（Pure）。 |
| `isPathfindingPassable` | `actor: Engine.Actor`, `targetPosition: sf.Vector2i` | `passable: bool` | 纯函数（Pure）。 |
| `hasPathBlockingOverlapActor` | `actor: Engine.Actor`, `targetPosition: sf.Vector2i` | `hasActor: bool` | 纯函数（Pure）。 |
| `getScene` | — | `scene: GlobalCore.SceneBase` | 纯函数（Pure）。 |
| `addCommonTip` | `text: string` | — | 执行引脚（`default`）。 |
| `addDamageText` | `text: string`, `position: sf.Vector2f` | — | 执行引脚（`default`）。 |
