---@meta Global.WorldGameMap

---@alias Global.WorldGameMap.StreamingStats GlobalCore.WorldStreamingStats

---@class Global.WorldGameMap.TerrainEditBatch
---@field region Source.SceneComponents.WorldRegionData
---@field positions sf.Vector2i[]
---@field keys table<string, boolean>

---@class Global.WorldGameMap.ObservedRootPosition
---@field x integer
---@field y integer

---@class Global.WorldGameMap.RegionPayload
---@field tilemap           Engine.Tilemap
---@field terrain           Global.GameMap.RegionTerrain
---@field actors            table<string, Engine.Actor[]>
---@field mapData           Source.SceneComponents.WorldRegionEnvironmentData
---@field actorSet          table<Engine.Actor, boolean>
---@field definitionRegions table<Engine.Actor, string>
---@field worldRegion       Source.SceneComponents.WorldRegionData | nil
---@field actorRoots        table<Engine.Actor, Engine.Actor>
---@field activeRoots       table<Engine.Actor, boolean>
---@field estimatedRuntimeBytes integer | nil
---@field prewarmedGeometryRevision integer | nil

---@class Global.WorldGameMap.RegionBuildState
---@field completed              boolean
---@field ready                  boolean
---@field geometryRevision       integer
---@field actorPhaseComplete     boolean
---@field readyActorRoots        { actorRoot: Engine.Actor, actorLayer: string }[]
---@field actorPublishQueue      Engine.Actor[] | nil
---@field actorPublishIndex      integer | nil
---@field actorPublishRoot       Engine.Actor | nil
---@field actorPublishLayer      string | nil
---@field maximumStepMilliseconds number
---@field stageMaximumMilliseconds table<string, number>
---@field currentStage string
---@field currentStepMilliseconds number
---@field lastStepMaximumStage string
---@field lastStepMaximumMilliseconds number
---@field lastStepResumeCount integer
---@field payload                Global.WorldGameMap.RegionPayload | nil
---@field tileLayers             Engine.TileLayer[] | nil
---@field layerBuildStates       Source.SceneComponents.WorldLayerBuildState[] | nil
---@field isCellReady            fun(position: sf.Vector2i): boolean
---@field isRectReady            fun(rect: Global.WorldGeometry.CellRect): boolean
---@field areActorsReady         fun(): boolean
---@field prepareActors          fun(deadline: number): boolean
---@field prepareRect            fun(rect: Global.WorldGeometry.CellRect, deadline: number): boolean
---@field step                   fun(deadline: number): Global.WorldGameMap.RegionPayload | nil

---@class Global.WorldGameMap.RegionPublishState
---@field builder         Global.WorldGameMap.RegionBuildState | nil
---@field conversion      FileBatchDataConversion | nil
---@field contentBytes    integer | nil
---@field priorityRect    Global.WorldGeometry.CellRect | nil
---@field forceActivate   boolean
---@field phase           "convert" | "build" | "index" | "finalise"
---@field payload         Global.WorldGameMap.RegionPayload | nil
---@field layerNames      string[]
---@field layerIndex      integer
---@field rootIndex       integer
---@field actorQueue      Engine.Actor[] | nil
---@field actorIndex      integer
---@field actorLayer      string
---@field actorRoot       Engine.Actor | nil
---@field indexedActors   Engine.Actor[]
---@field definitionRoots Engine.Actor[]

---@class Global.WorldGameMap.WorldGameMap: GameMap
---@field _worldConfig                 Source.SceneComponents.WorldMapData
---@field _worldManifestPath           string
---@field _worldDataRoot               string
---@field _worldBounds                 Global.WorldGeometry.CellRect
---@field _worldRegions                Source.SceneComponents.WorldRegionData[]
---@field _worldRegionFactory          fun(region: Source.SceneComponents.WorldRegionData, data: Source.SceneComponents.SerializedMapData, priorityRect: Global.WorldGeometry.CellRect | nil): Global.WorldGameMap.RegionBuildState
---@field _worldStreamingState         GlobalCore.WorldStreamingState
---@field _worldActorsByTag            table<string, Engine.Actor>
---@field _worldActorLayers            table<Engine.Actor, string>
---@field _worldActorDefinitionRegions table<Engine.Actor, string>
---@field _worldActorRoots             table<Engine.Actor, Engine.Actor>
---@field _worldActorRegions           table<Engine.Actor, Source.SceneComponents.WorldRegionData>
---@field _worldRootStates             table<Engine.Actor, "NeverActive" | "Active" | "Dormant">
---@field _worldRootSleepTimes         table<Engine.Actor, number>
---@field _worldLooseRoots             Engine.Actor[]
---@field _worldPendingRehomes         table<Engine.Actor, Source.SceneComponents.WorldRegionData>
---@field _worldObservedRootPositions  table<Engine.Actor, Global.WorldGameMap.ObservedRootPosition>
---@field _worldDestroyedRootsDirty    boolean
---@field _worldActiveChunkBounds      Global.WorldGeometry.CellRect | nil
---@field _worldActiveChunkGeneration  integer
---@field _worldActiveChunkReconcilePending boolean
---@field _worldLooseActiveChunkGeneration integer
---@field _worldActivationDeferred        boolean
---@field _worldSuppressedActorTags    table<string, boolean>
---@field _worldSuppressedActorObjects table<Engine.Actor, boolean>
---@field _worldDestroyedActorTagProvider (fun(): string[]) | nil
---@field _worldAddedActorPositionRecorder (fun(actor: Engine.Actor, position: sf.Vector2i)) | nil
---@field _worldReservedTagSources     table<string, string>
---@field _worldRuntimeTagIndices      table<string, integer>
---@field _worldLayerOrder             string[]
---@field _worldLayerNames             table<string, boolean>
---@field _worldMovedActorRecorder     fun(actor: Engine.Actor, definitionRegion: string, currentRegion: string, layerName: string, position: sf.Vector2i) | nil
---@field _worldStreamJob              FileBatchJob | nil
---@field _worldStreamJobRegions       table<string, Source.SceneComponents.WorldRegionData>
---@field _worldStreamBatchRegions     Source.SceneComponents.WorldRegionData[]
---@field _worldActiveRect             Global.WorldGeometry.CellRect | nil
---@field _worldPreparedRect           Global.WorldGeometry.CellRect | nil
---@field _worldStreamingCameraPosition sf.Vector2f | nil
---@field _worldDisposed               boolean
---@field _worldPublishMilliseconds    number
---@field _worldPublishSlowStage       string
---@field _worldPublishSlowStageMilliseconds number
---@field _worldTransitionPublishThisTick boolean
---@field _worldPublishBudgetWarningEmitted boolean
---@field _worldLastReadyCameraPosition sf.Vector2f | nil
---@field _worldShadersPrewarmed         boolean
---@field _worldPrewarmMilliseconds      number
---@field _worldPrewarmReadbackMilliseconds number
---@field new                          fun(config: Source.SceneComponents.WorldMapData, regionFactory: fun(region: Source.SceneComponents.WorldRegionData, data: Source.SceneComponents.SerializedMapData, priorityRect: Global.WorldGeometry.CellRect | nil): Global.WorldGameMap.RegionBuildState, reservedTags?: string[]): Global.WorldGameMap.WorldGameMap
local WorldGameMap = {}

---@param config        Source.SceneComponents.WorldMapData
---@param regionFactory fun(region: Source.SceneComponents.WorldRegionData, data: Source.SceneComponents.SerializedMapData, priorityRect: Global.WorldGeometry.CellRect | nil): Global.WorldGameMap.RegionBuildState
---@param reservedTags? string[]
function WorldGameMap:init(config, regionFactory, reservedTags) end

---@return boolean
function WorldGameMap:isWorldMap() end

---@return string
function WorldGameMap:getManifestPath() end

---@param movedActorRecorder fun(actor: Engine.Actor, definitionRegion: string, currentRegion: string, layerName: string, position: sf.Vector2i)
function WorldGameMap:setMovedActorPersistenceCallback(movedActorRecorder) end

---@param destroyedActorTagProvider fun(): string[]
function WorldGameMap:setDestroyedActorTagProvider(destroyedActorTagProvider) end

---@param addedActorPositionRecorder fun(actor: Engine.Actor, position: sf.Vector2i)
function WorldGameMap:setAddedActorPositionPersistenceCallback(addedActorPositionRecorder) end

---@param tag string
function WorldGameMap:suppressActorTag(tag) end

---@return Global.WorldGameMap.StreamingStats
function WorldGameMap:getStreamingStats() end

function WorldGameMap:disposeStreaming() end

---@param path string
---@return Source.SceneComponents.WorldRegionData | nil
function WorldGameMap:getRegionByPath(path) end

---@param position sf.Vector2i
---@return Source.SceneComponents.WorldRegionData | nil, sf.Vector2i | nil
function WorldGameMap:getRegionPosition(position) end

---@param position sf.Vector2i
---@return Source.SceneComponents.WorldRegionEnvironmentData | nil
function WorldGameMap:getEnvironmentDataAt(position) end

---@param position sf.Vector2i
---@return Source.SceneComponents.WorldRegionData | nil
function WorldGameMap:ensureRegionLoadedAt(position) end

---@brief Synchronously prepare the initial Active neighbourhood and render the camera viewport at the player destination.
---
---The player must already be at `position`. Prepared-only placements remain asynchronous. Missing or malformed required child data raises an error; legal holes need no region payload.
---@param position sf.Vector2i
function WorldGameMap:prepareViewportAt(position) end

---@return Engine.Actor[]
function WorldGameMap:getAllActors() end

---@brief Get a detached Active-area rectangle, or the visible area before Active bounds exist, without loading regions.
---@return Global.WorldGeometry.CellRect
function WorldGameMap:getGameplayCellRect() end

---@param actor Engine.Actor
---@return string | nil
function WorldGameMap:getActorLayer(actor) end

---@param actor            Engine.Actor
---@param layer            string
---@param definitionRegion string
---@param emitCreateEvent  boolean | nil
function WorldGameMap:spawnPersistedWorldActor(actor, layer, definitionRegion, emitCreateEvent) end

---@param actor    Engine.Actor
---@param position sf.Vector2i | nil
function WorldGameMap:recordWorldActorPosition(actor, position) end

---@private
---@param root              Engine.Actor
---@param destinationRegion Source.SceneComponents.WorldRegionData
---@param sourceRegion      Source.SceneComponents.WorldRegionData | nil
---@param position          sf.Vector2i
---@param touchedRegions    table<Source.SceneComponents.WorldRegionData, boolean> | nil
---@return table<Source.SceneComponents.WorldRegionData, boolean> | nil, boolean
function WorldGameMap:_queuePendingWorldActorRehome(root, destinationRegion, sourceRegion, position, touchedRegions) end

---@private
---@param root              Engine.Actor
---@param sourceRegion      Source.SceneComponents.WorldRegionData | nil
---@param destinationRegion Source.SceneComponents.WorldRegionData | nil
---@param layerName         string
---@param position          sf.Vector2i
---@param touchedRegions    table<Source.SceneComponents.WorldRegionData, boolean> | nil
---@return table<Source.SceneComponents.WorldRegionData, boolean> | nil, boolean
function WorldGameMap:_transferWorldActorRoot(root, sourceRegion, destinationRegion, layerName, position, touchedRegions) end

---@private
---@param touchedRegions table<Source.SceneComponents.WorldRegionData, boolean> | nil
---@param looseTouched   boolean
---@return table<Source.SceneComponents.WorldRegionData, boolean> | nil, boolean
function WorldGameMap:_advancePendingWorldActorRehomes(touchedRegions, looseTouched) end

---@private
---@param root           Engine.Actor
---@param position       sf.Vector2i
---@param touchedRegions table<Source.SceneComponents.WorldRegionData, boolean> | nil
---@param looseTouched   boolean
---@return table<Source.SceneComponents.WorldRegionData, boolean> | nil, boolean
function WorldGameMap:_rehomeChangedWorldActorRoot(root, position, touchedRegions, looseTouched) end

---@private
---@param position sf.Vector2f | nil
---@return boolean
function WorldGameMap:_prepareWorldCameraPosition(position) end

---@private
---@param region       Source.SceneComponents.WorldRegionData
---@param builder      Global.WorldGameMap.RegionBuildState
---@param requiredRect Global.WorldGeometry.CellRect
function WorldGameMap:_prepareWorldRegionRect(region, builder, requiredRect) end

return WorldGameMap
