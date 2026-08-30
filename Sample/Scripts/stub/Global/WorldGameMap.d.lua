---@meta Global.WorldGameMap

---@class Global.WorldGameMap.StreamingStats
---@field Unloaded integer
---@field Reading  integer
---@field Prepared integer
---@field Active   integer
---@field Dormant  integer
---@field queued   integer

---@alias Global.WorldGameMap.StaticTransmissionSignature tuple<string | integer | boolean>

---@class Global.WorldGameMap.ObservedRootPosition
---@field x integer
---@field y integer

---@class Global.WorldGameMap.RegionPayload
---@field tilemap           Engine.Tilemap
---@field terrain           Global.GameMap.RegionTerrain
---@field actors            table<string, Engine.Actor[]>
---@field lights            GlobalCore.Light[]
---@field mapData           Source.SceneComponents.WorldRegionEnvironmentData
---@field actorSet          table<Engine.Actor, boolean>
---@field definitionRegions table<Engine.Actor, string>
---@field worldRegion       Source.SceneComponents.WorldRegionData | nil
---@field actorRoots        table<Engine.Actor, Engine.Actor>
---@field activeRoots       table<Engine.Actor, boolean>
---@field rootChunks        table<string, Engine.Actor[]>
---@field rootChunkKeys     table<Engine.Actor, string>
---@field estimatedRuntimeBytes integer | nil
---@field prewarmedLayerShaders table<string, boolean> | nil

---@class Global.WorldGameMap.RegionBuildState
---@field completed              boolean
---@field ready                  boolean
---@field geometryRevision       integer
---@field lightingRevision       integer
---@field actorPhaseComplete     boolean
---@field lightPhaseComplete     boolean
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
---@field conversion      FileBatchJsonConversion | nil
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
---@field _staticTransmissionSignature Global.WorldGameMap.StaticTransmissionSignature | nil
---@field _tilemapLightMaskShader       sf.Shader | nil
---@field _lightMaskShader              sf.Shader | nil
---@field _lightPassShader              sf.Shader | nil
---@field _unobstructedLightPassShader  sf.Shader | nil
---@field _materialShader               sf.Shader | nil
---@field _actorHueShader               sf.Shader | nil
---@field _lightBlockSize               sf.Vector2f
---@field _shaderMapSize                sf.Vector2f
---@field _playerCoverColour            sf.Color
---@field _staticTransmission           sf.RenderTexture | nil
---@field _staticOccupancy              sf.Texture | nil
---@field _dynamicTransmission          sf.RenderTexture | nil
---@field _directLight                  sf.RenderTexture | nil
---@field _directLightCleared           boolean
---@field _staticDirectLight            sf.RenderTexture | nil
---@field _surfaceMask                  sf.RenderTexture | nil
---@field _useStaticDirectLight         boolean
---@field _lightPassQuad                sf.RectangleShape | nil
---@field _unobstructedLightVertices    sf.VertexArray | nil
---@field _unobstructedLightVertex      sf.Vertex | nil
---@field _surfaceTileRenderStates      sf.RenderStates | nil
---@field _surfaceActorRenderStates     sf.RenderStates | nil
---@field _transmissionTileRenderStates sf.RenderStates | nil
---@field _transmissionActorRenderStates sf.RenderStates | nil
---@field _lightPassRenderStates        sf.RenderStates | nil
---@field _unobstructedLightPassRenderStates sf.RenderStates | nil
---@field _actorShaderBuffer            sf.RenderTexture | nil
---@field _actorHueBuffer               sf.RenderTexture | nil
---@field _actorHueSourceSprite         sf.Sprite | nil
---@field _cachedActiveLights           Global.GameMap.LightCacheEntry[] | nil
---@field _unobstructedLightCache       Global.GameMap.LightCacheEntry[] | nil
---@field _cachedLightMaterialRevision  integer
---@field _cachedLightTransmissionSignature Global.GameMap.StaticTransmissionSignature | nil
---@field _staticTransmissionRevision   integer
---@field _staticTransmissionActorCache table[] | nil
---@field _staticTransmissionGeneration integer
---@field _surfaceMaskRevision          integer
---@field _surfaceMaskSignature         Global.GameMap.StaticTransmissionSignature | nil
---@field _surfaceMaskActorCache        table[] | nil
---@field _renderedLightingLights       Global.GameMap.LightCacheEntry[] | nil
---@field _renderedLightingOwners       (Engine.Actor | boolean)[] | nil
---@field _renderedLightingActors       table[] | nil
---@field _renderedLightingStaticGeneration integer
---@field _renderedLightingView         number[] | nil
---@field _renderedLightingTargetSize   integer[] | nil
---@field _staticLightCaches            table[]
---@field _staticTextureOrigin          sf.Vector2f
---@field _staticTextureSize            sf.Vector2f
---@field _staticOccupancyOrigin        sf.Vector2f
---@field _staticOccupancySize          sf.Vector2f
---@field _dynamicTransmissionPixelSize integer
---@field _zeroShaderOffset             sf.Vector2f
---@field _identityShaderRotation        sf.Vector2f
---@field _shaderViewSinCos             sf.Vector2f
---@field _shaderColour                 sf.Vector3f
---@field _layerMaskTextureCache        table<string, Global.GameMap.LayerMaskTextureCacheEntry>
---@field _transparentTiles             table[]
---@field _coverLayerStates             GameMapCoverLayerState[] | nil
---@field _coverPlayerX                 integer | nil
---@field _coverPlayerY                 integer | nil
---@field _coverPlayerLayerIndex        integer | nil
---@field _coverAlpha                   integer | nil
---@field _coverMaterialRevision        integer | nil
---@field _worldConfig                 Source.SceneComponents.WorldMapData
---@field _worldManifestPath           string
---@field _worldDataRoot               string
---@field _worldBounds                 Global.WorldGeometry.CellRect
---@field _worldRegions                Source.SceneComponents.WorldRegionData[]
---@field _worldRegionFactory          fun(region: Source.SceneComponents.WorldRegionData, data: Source.SceneComponents.SerializedMapData, priorityRect: Global.WorldGeometry.CellRect | nil): Global.WorldGameMap.RegionBuildState
---@field _worldRegionBuckets          table<string, Source.SceneComponents.WorldRegionData[]>
---@field _worldLoadedRegions          table<Source.SceneComponents.WorldRegionData, boolean>
---@field _worldActorsByTag            table<string, Engine.Actor>
---@field _worldActorLayers            table<Engine.Actor, string>
---@field _worldActorDefinitionRegions table<Engine.Actor, string>
---@field _worldActorRoots             table<Engine.Actor, Engine.Actor>
---@field _worldActorRegions           table<Engine.Actor, Source.SceneComponents.WorldRegionData>
---@field _worldRootStates             table<Engine.Actor, "NeverActive" | "Active" | "Dormant">
---@field _worldRootSleepTimes         table<Engine.Actor, number>
---@field _worldLooseRoots             Engine.Actor[]
---@field _worldLooseRootChunks        table<string, Engine.Actor[]>
---@field _worldLooseRootChunkKeys     table<Engine.Actor, string>
---@field _worldPendingRehomes         table<Engine.Actor, Source.SceneComponents.WorldRegionData>
---@field _worldActorDemandRegions     table<Source.SceneComponents.WorldRegionData, boolean>
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
---@field _worldStreamQueue            Source.SceneComponents.WorldRegionData[]
---@field _worldStreamQueued           table<Source.SceneComponents.WorldRegionData, boolean>
---@field _worldStreamJob              FileBatchJob | nil
---@field _worldStreamJobRegions       table<string, Source.SceneComponents.WorldRegionData>
---@field _worldStreamBatchRegions     Source.SceneComponents.WorldRegionData[]
---@field _worldPublishQueue           Source.SceneComponents.WorldRegionData[]
---@field _worldPublishQueued          table<Source.SceneComponents.WorldRegionData, boolean>
---@field _worldDemandGeneration       integer
---@field _worldPreviousCameraCenterX  number | nil
---@field _worldPreviousCameraCenterY  number | nil
---@field _worldActiveRect             Global.WorldGeometry.CellRect | nil
---@field _worldPreparedRect           Global.WorldGeometry.CellRect | nil
---@field _worldStreamingCameraPosition sf.Vector2f | nil
---@field _worldDisposed               boolean
---@field _worldCacheBytes             integer
---@field _worldCacheBytesDirty        boolean
---@field _worldPublishMilliseconds    number
---@field _worldPublishSlowStage       string
---@field _worldPublishSlowStageMilliseconds number
---@field _worldTransitionPublishThisTick boolean
---@field _worldPublishBudgetWarningEmitted boolean
---@field _worldRuntimeLights          GlobalCore.Light[]
---@field _worldLastReadyCameraPosition sf.Vector2f | nil
---@field _worldShaderPrewarmTarget      sf.RenderTexture | nil
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

return WorldGameMap
