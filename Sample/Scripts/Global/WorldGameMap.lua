local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local cjson = require("cjson")
local GameMap = require("Global.GameMap")
local WorldGeometry = require("Global.WorldGeometry")
local WorldMapConstants = require("Global.WorldMapConstants")
local WorldGameMapActors = require("Global.WorldGameMap.Actors")
local WorldGameMapActorStreaming = require("Global.WorldGameMap.ActorStreaming")
local WorldGameMapRendering = require("Global.WorldGameMap.Rendering")
local WorldGameMapRegionPublishing = require("Global.WorldGameMap.RegionPublishing")
local WorldGameMapStreaming = require("Global.WorldGameMap.Streaming")
local WorldGarbageCollector = require("Global.WorldGameMap.GarbageCollector")
local Logging = require("Global.Utils.Logging")
local ActorMapService = Engine.ActorMapService
local FogController = GlobalCore.FogController

local SIGNIFICANT_PUBLISH_OVERRUN_MILLISECONDS = 4.0
---@class (partial) Global.WorldGameMap.WorldGameMap: GameMap
local WorldGameMap = {}

---@param world    Global.WorldGameMap.WorldGameMap
---@param actor    Engine.Actor
---@param position sf.Vector2i
---@return boolean
local function isActorPositionReady(world, actor, position)
    if not world:isSparseWorldGameplayPositionReady(position) then
        return false
    end
    for _, occupiedPosition in ipairs(actor:getOccupiedMapCellsAtMapPosition(position)) do
        if not world:isSparseWorldGameplayPositionReady(occupiedPosition) then
            return false
        end
    end
    return true
end

---@param world Global.WorldGameMap.WorldGameMap
---@param actor Engine.Actor
---@return GlobalCore.PathResult
local function createEmptyPathResult(world, actor)
    local invalidStart = sf.Vector2i.new(0, 0)
    local invalidGoal = sf.Vector2i.new(1, 0)
    local emptySize = sf.Vector2u.new(0, 0)
    ---@cast invalidStart sf.Vector2i
    ---@cast invalidGoal sf.Vector2i
    ---@cast emptySize sf.Vector2u
    return world:findPathExt(invalidStart, invalidGoal, emptySize, actor, {})
end

---@param config        Source.SceneComponents.WorldMapData
---@param regionFactory fun(region: Source.SceneComponents.WorldRegionData, data: Source.SceneComponents.SerializedMapData, priorityRect: Global.WorldGeometry.CellRect | nil): Global.WorldGameMap.RegionBuildState
---@param reservedTags  string[] | nil
function WorldGameMap:init(config, regionFactory, reservedTags)
    self._worldConfig = config
    self._worldManifestPath = config.manifestPath
    self._worldDataRoot = config.dataRoot
    self._worldBounds = { x = 0, y = 0, width = config.width, height = config.height }
    self._worldRegions = config.regions
    self._worldRegionFactory = regionFactory
    self._worldRegionBuckets = {}
    self._worldLoadedRegions = {}
    self._worldMovedActorRecorder = nil
    self:_initialiseWorldActorState(config, reservedTags)
    self._worldStreamQueue = {}
    self._worldStreamQueued = {}
    self._worldStreamJob = nil
    self._worldStreamJobRegions = {}
    self._worldStreamBatchRegions = {}
    self._worldPublishQueue = {}
    self._worldPublishQueued = {}
    self._worldDemandGeneration = 0
    self._worldPreviousCameraCenterX = nil
    self._worldPreviousCameraCenterY = nil
    self._worldActiveRect = nil
    self._worldPreparedRect = nil
    self._worldStreamingCameraPosition = nil
    self._worldDisposed = false
    self._worldGarbageCollectorRegistered = false
    self._worldCacheBytes = 0
    self._worldCacheBytesDirty = true
    self._worldPublishMilliseconds = 0.0
    self._worldPublishSlowStage = "idle"
    self._worldPublishSlowStageMilliseconds = 0.0
    self._worldTransitionPublishThisTick = false
    self._worldPublishBudgetWarningEmitted = false
    self._worldUrgentBuildCursor = 0
    self._worldBackgroundBuildCursor = 0
    self._worldShaderPrewarmTarget = nil
    self._worldShadersPrewarmed = false
    self._worldPrewarmMilliseconds = 0.0
    self._worldPrewarmReadbackMilliseconds = 0.0
    self:_indexRegions()
    local worldSize = sf.Vector2u.new(config.width, config.height)
    local regionRects = {}
    for _, region in ipairs(self._worldRegions) do
        regionRects[#regionRects + 1] = sf.IntRect.new(region.x, region.y, region.width, region.height)
    end
    ---@cast worldSize sf.Vector2u
    ---@cast regionRects sf.IntRect[]
    ---@type Global.GameMap.SparseWorldConfig
    local sparseWorldConfig = { size = worldSize, layerOrder = config.layerOrder, regionRects = regionRects }
    GameMap.init(self, config.worldName, Engine.Tilemap.new({}), nil, false, sparseWorldConfig)
    self._layerNames = self._worldLayerOrder
    self._worldLastReadyCameraPosition = nil
    self._worldRuntimeLights = {}
    FogController.applyWorldFromMapData(config)
end

---@diagnostic disable-next-line: unused
function WorldGameMap:isWorldMap()
    return true
end

function WorldGameMap:getManifestPath()
    return self._worldManifestPath
end

function WorldGameMap:activateStreamingGarbageCollector()
    assert(not self._worldDisposed, "Cannot activate garbage collection for a disposed world")
    if self._worldGarbageCollectorRegistered then
        return
    end
    WorldGarbageCollector.Acquire()
    self._worldGarbageCollectorRegistered = true
end

function WorldGameMap.StepGarbageCollector()
    WorldGarbageCollector.Step()
end

---@param movedActorRecorder fun(actor: Engine.Actor, definitionRegion: string, currentRegion: string, layerName: string, position: sf.Vector2i)
function WorldGameMap:setMovedActorPersistenceCallback(movedActorRecorder)
    self._worldMovedActorRecorder = movedActorRecorder
end

function WorldGameMap:getStreamingStats()
    local result = {
        Unloaded = 0,
        Reading = 0,
        Prepared = 0,
        Active = 0,
        Dormant = 0,
        queued = #self._worldStreamQueue + #self._worldPublishQueue
    }
    for _, region in ipairs(self._worldRegions) do
        result[region.state] = result[region.state] + 1
        if region.backgroundBuilder ~= nil then
            result.queued = result.queued + 1
        end
    end
    return result
end

function WorldGameMap:disposeStreaming()
    if self._worldDisposed then
        return
    end
    if self._worldGarbageCollectorRegistered then
        WorldGarbageCollector.Release()
        self._worldGarbageCollectorRegistered = false
    end
    self._worldDisposed = true
    if self._worldStreamJob ~= nil then
        asyncio.cancel_file_batch(self._worldStreamJob)
        self._worldStreamJob = nil
    end
    for _, region in ipairs(self._worldRegions) do
        self:_cancelRegionPublish(region)
        region.backgroundBuilder = nil
        region.activeChunkGeneration = nil
    end
    self._worldPublishQueue = {}
    self._worldPublishQueued = {}
    self:clearSparseWorld()
    for region in pairs(self._worldLoadedRegions) do
        assert(region.payload ~= nil, "Loaded world region has no payload: " .. region.path)
        FogController.removeWorldRegionFog(region.path)
        region.payload = nil
        region.payloadBytes = nil
        region.backgroundBuilder = nil
        region.state = "Unloaded"
    end
    self._worldLoadedRegions = {}
    self._worldActorsByTag = {}
    self._worldActorLayers = {}
    self._worldActorDefinitionRegions = {}
    self._worldActorRoots = {}
    self._worldActorRegions = {}
    self._worldRootStates = {}
    self._worldRootSleepTimes = {}
    self._worldLooseRoots = {}
    self._worldLooseRootChunks = {}
    self._worldLooseRootChunkKeys = {}
    self._worldPendingRehomes = {}
    self._worldActorDemandRegions = {}
    self._worldObservedRootPositions = {}
    self._worldDestroyedRootsDirty = false
    self._worldActiveChunkBounds = nil
    self._worldActiveChunkGeneration = 0
    self._worldActiveChunkReconcilePending = false
    self._worldLooseActiveChunkGeneration = -1
    self._worldActivationDeferred = false
    self._layerMaskTextureCache = {}
    self._worldShaderPrewarmTarget = nil
    self._worldCacheBytes = 0
    self._worldCacheBytesDirty = false
end

---@param region   Source.SceneComponents.WorldRegionData
---@param payload  Global.WorldGameMap.RegionPayload
---@param activate boolean
function WorldGameMap:installRegion(region, payload, activate)
    assert(region.payload == nil, "World region is already installed: " .. region.path)
    assert(region.publishState == nil, "World region is already being published: " .. region.path)
    self:_filterSuppressedRegionActors(region, payload)
    region.payload = payload
    self:_indexRegionActors(region)
    self:_completeRegionInstall(region, activate)
end

---@param region       Source.SceneComponents.WorldRegionData
---@param activate     boolean
---@param payloadBytes integer | nil
function WorldGameMap:_completeRegionInstall(region, activate, payloadBytes)
    local payload = assert(region.payload)
    self:_filterSuppressedRegionActors(region, payload)
    assert(not self._worldLoadedRegions[region], "World region is already in the loaded set: " .. region.path)
    self._worldLoadedRegions[region] = true
    region.state = "Prepared"
    region.lastUsed = perfCounter()
    local builder = region.backgroundBuilder
    self:setSparseWorldRegion(region.index, payload.tilemap, builder == nil or builder.areActorsReady())
    local regionRect = sf.IntRect.new(region.x, region.y, region.width, region.height)
    ---@cast regionRect sf.IntRect
    local fog = payload.mapData.fog
    if fog == cjson.null then
        fog = nil
    end
    assert(fog == nil or type(fog) == "string", "mapData.fog must be a string")
    local fogPower = payload.mapData.fogPower
    if fogPower == nil or fogPower == cjson.null then
        fogPower = 0
    end
    if bool(fog) and fogPower > 0 then
        local fogOx = payload.mapData.fogOx
        local fogOy = payload.mapData.fogOy
        local fogDistort = payload.mapData.fogDistort
        if fogOx == nil or fogOx == cjson.null then
            fogOx = 0
        end
        if fogOy == nil or fogOy == cjson.null then
            fogOy = 0
        end
        if fogDistort == nil or fogDistort == cjson.null then
            fogDistort = 0
        end
        ---@cast fog string
        FogController.setWorldRegionFog(region.path, regionRect, fog, fogPower, fogOx, fogOy, fogDistort)
    else
        FogController.removeWorldRegionFog(region.path)
    end
    local sourceBytes = payloadBytes or asizeof(payload)
    local runtimeBytes = payload.estimatedRuntimeBytes
        or WorldMapConstants.REGION_FIXED_CACHE_BYTES
            + region.width * region.height
                * #self._worldConfig.layerOrder * WorldMapConstants.REGION_LAYER_CELL_CACHE_BYTES
    region.payloadBytes = sourceBytes * 4 + runtimeBytes
    self._worldCacheBytesDirty = true
    if activate then
        self:_activateRegion(region)
    end
    self:_enforceCacheBudget()
    self:markPassabilityDirty()
end

---@param path string
---@return Source.SceneComponents.WorldRegionData | nil
function WorldGameMap:getRegionByPath(path)
    for _, region in ipairs(self._worldRegions) do
        if region.path == path then
            return region
        end
    end
    return nil
end

---@param position sf.Vector2i
---@return Source.SceneComponents.WorldRegionData | nil, sf.Vector2i | nil
function WorldGameMap:getRegionPosition(position)
    local region = self:_findRegionAt(position)
    if region == nil then
        return nil, nil
    end
    local localX = position.x - region.x
    local localY = position.y - region.y
    ---@cast localX integer
    ---@cast localY integer
    local localPosition = sf.Vector2i.new(localX, localY)
    ---@cast localPosition sf.Vector2i
    return region, localPosition
end

---@param position sf.Vector2i
---@return Source.SceneComponents.WorldRegionEnvironmentData | nil
function WorldGameMap:getEnvironmentDataAt(position)
    local region = self:_findRegionAt(position)
    if region == nil or region.payload == nil then
        return nil
    end
    return region.payload.mapData
end

function WorldGameMap:ensureRegionLoadedAt(position)
    local region = self:_findRegionAt(position)
    if region == nil then
        return nil
    end
    if region.payload == nil then
        if region.publishState == nil then
            local data = Engine.getJSONData(os.path.join(self._worldDataRoot, region.map))
            local priorityRect = self._camera ~= nil and self:_getVisibleCellRect()
                or { x = position.x, y = position.y, width = 1, height = 1 }
            self:_beginRegionPublish(region, data, true, priorityRect)
        else
            region.publishState.forceActivate = true
        end
        self:_drainRegionPublish(region)
    else
        self:_activateRegion(region)
    end
    return region
end

function WorldGameMap:updateAutoTileAnimation(deltaTime)
    for _, region in ipairs(self._worldRegions) do
        if region.payload ~= nil then
            region.payload.tilemap:updateAutoTileAnimation(deltaTime)
        end
    end
end

function WorldGameMap:onTick(deltaTime)
    self._worldPublishMilliseconds = 0.0
    self._worldPublishSlowStage = "idle"
    self._worldPublishSlowStageMilliseconds = 0.0
    self._worldTransitionPublishThisTick = false
    if self:_refreshSuppressedActorTags() then
        self:_applySuppressedActorTags()
    end
    self:_rehomeRegionActors()
    self:_syncStreamingCamera()
    self:_refreshStreamingStates()
    self:_syncWorldActiveChunkActivation()
    self:_pumpStreaming()
    self:_enforceCacheBudget()
    GameMap.onTick(self, deltaTime)
    self:_rehomeRegionActors()
    self:_pruneDestroyedRegionActors()
    self:_enforceCacheBudget()
    if not self._worldTransitionPublishThisTick and not self._worldPublishBudgetWarningEmitted
        and self._worldPublishMilliseconds >= SIGNIFICANT_PUBLISH_OVERRUN_MILLISECONDS then
        self._worldPublishBudgetWarningEmitted = true
        Logging.warning(
            "World streaming publish exceeded the %.0f ms hitch threshold: %.3f ms; slowest builder stage " .. "%s %.3f ms (%s)",
            SIGNIFICANT_PUBLISH_OVERRUN_MILLISECONDS, self._worldPublishMilliseconds, self._worldPublishSlowStage,
            self._worldPublishSlowStageMilliseconds, self._worldManifestPath
        )
    end
    self:_recordStreamingProfile()
end

function WorldGameMap:getLights()
    return self._lights
end

function WorldGameMap:setLights(lights)
    self._worldRuntimeLights = copy(lights)
    self:_refreshWorldLights()
end

function WorldGameMap:addLight(light)
    self._worldRuntimeLights[#self._worldRuntimeLights + 1] = light
    self:_refreshWorldLights()
end

function WorldGameMap:removeLight(light)
    local index = table.index(self._worldRuntimeLights, light)
    if index ~= nil then
        table.remove(self._worldRuntimeLights, index)
        self:_refreshWorldLights()
        return
    end
    error("Light not found in world map", 2)
end

function WorldGameMap:isPassable(actor, targetPosition)
    if not isActorPositionReady(self, actor, targetPosition) then
        return false
    end
    return ActorMapService.isPassable(self, actor, targetPosition)
end

function WorldGameMap:findPathResult(start, goal, actor, excludedAnchors)
    self:_syncActorsForPathfinding()
    local result
    if isActorPositionReady(self, actor, start) and isActorPositionReady(self, actor, goal) then
        result = self:findPathExt(start, goal, self:getSize(), actor, excludedAnchors or {})
    else
        result = createEmptyPathResult(self, actor)
    end
    self:_clearActorsPathfindingBlocks()
    return result
end

function WorldGameMap:getTopMaterial(position)
    return ActorMapService.getTopMaterial(self, position)
end

function WorldGameMap:getTerrainTile(layerName, position)
    local region, localPosition = self:getRegionPosition(position)
    if region == nil or not self:isSparseWorldCellReady(position) then
        return nil
    end
    local payload = assert(region.payload)
    ---@cast localPosition sf.Vector2i
    return payload.terrain:getTerrainTile(layerName, localPosition)
end

function WorldGameMap:getTerrainTilePositions(layerName, tileID)
    local positions = {}
    for _, region in ipairs(self._worldRegions) do
        local builder = region.backgroundBuilder
        if region.payload ~= nil and region.publishState == nil
            and (builder == nil
                or builder.completed and builder.actorPublishQueue == nil and not bool(builder.readyActorRoots)) then
            for _, localPosition in ipairs(region.payload.terrain:getTerrainTilePositions(layerName, tileID)) do
                local worldX = localPosition.x + region.x
                local worldY = localPosition.y + region.y
                ---@cast worldX integer
                ---@cast worldY integer
                positions[#positions + 1] = sf.Vector2i.new(worldX, worldY)
            end
        end
    end
    return positions
end

function WorldGameMap:setTerrainTile(layerName, position, tileID)
    return bool(self:setTerrainTiles(layerName, { position }, tileID))
end

function WorldGameMap:setTerrainTiles(layerName, positions, tileID)
    local changed = {}
    for _, position in ipairs(positions) do
        local region, localPosition = self:getRegionPosition(position)
        if region ~= nil and self:isSparseWorldCellReady(position)
            and assert(region.payload).terrain:setTerrainTile(layerName, assert(localPosition), tileID) then
            local payload = assert(region.payload)
            payload.tilemap = payload.terrain:getTilemap()
            self:setSparseWorldRegion(region.index, payload.tilemap, true)
            if payload.prewarmedLayerShaders ~= nil then
                payload.prewarmedLayerShaders[layerName] = nil
            end
            changed[#changed + 1] = position
        end
    end
    if bool(changed) then
        self:markPassabilityDirty()
    end
    return changed
end

---@param position sf.Vector2i
---@return Source.SceneComponents.WorldRegionData | nil
function WorldGameMap:_findRegionAt(position)
    if not WorldGeometry.RectContainsPosition(self._worldBounds, position) then
        return nil
    end
    local key = WorldGeometry.CellChunkKey(position, WorldMapConstants.SPATIAL_CHUNK_SIZE)
    for _, region in ipairs(self._worldRegionBuckets[key] or {}) do
        if WorldGeometry.RectContainsPosition(region, position) then
            return region
        end
    end
    return nil
end

function WorldGameMap:_indexRegions()
    for _, region in ipairs(self._worldRegions) do
        region.state = "Unloaded"
        region.payload = nil
        region.publishState = nil
        region.backgroundBuilder = nil
        region.wasActive = false
        region.wakeTags = nil
        region.activeChunkGeneration = nil
        local firstX = math.floor(region.x / WorldMapConstants.SPATIAL_CHUNK_SIZE)
        local firstY = math.floor(region.y / WorldMapConstants.SPATIAL_CHUNK_SIZE)
        local lastX = math.floor((region.x + region.width - 1) / WorldMapConstants.SPATIAL_CHUNK_SIZE)
        local lastY = math.floor((region.y + region.height - 1) / WorldMapConstants.SPATIAL_CHUNK_SIZE)
        for bucketY = firstY, lastY do
            for bucketX = firstX, lastX do
                local key = WorldGeometry.GridKey(bucketX, bucketY)
                self._worldRegionBuckets[key] = self._worldRegionBuckets[key] or {}
                self._worldRegionBuckets[key][#self._worldRegionBuckets[key] + 1] = region
            end
        end
    end
end

function WorldGameMap:_refreshWorldLights()
    local lights = copy(self._worldRuntimeLights)
    for _, region in ipairs(self._worldRegions) do
        if region.payload ~= nil and region.state == "Active" then
            for _, light in ipairs(region.payload.lights) do
                lights[#lights + 1] = light
            end
        end
    end
    self._lights = lights
end

return class(
    WorldGameMap, WorldGameMapActors, WorldGameMapActorStreaming, WorldGameMapRendering, WorldGameMapRegionPublishing,
    WorldGameMapStreaming, GameMap
)
