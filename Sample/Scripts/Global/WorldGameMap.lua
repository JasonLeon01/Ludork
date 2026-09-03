local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local cjson = require("cjson")
local GameMap = require("Global.GameMap")
local WorldMapConstants = require("Global.WorldMapConstants")
local WorldGameMapActors = require("Global.WorldGameMap.Actors")
local WorldGameMapActorStreaming = require("Global.WorldGameMap.ActorStreaming")
local WorldGameMapActorRendering = require("Global.WorldGameMap.ActorRendering")
local WorldGameMapLighting = require("Global.WorldGameMap.Lighting")
local WorldGameMapLightingPass = require("Global.WorldGameMap.LightingPass")
local WorldGameMapRendering = require("Global.WorldGameMap.Rendering")
local WorldGameMapRegionPublishing = require("Global.WorldGameMap.RegionPublishing")
local WorldGameMapStreaming = require("Global.WorldGameMap.Streaming")
local Logging = require("Global.Utils.Logging")
local ActorMapService = Engine.ActorMapService
local FogController = GlobalCore.FogController
local WorldRegionState = GlobalCore.WorldRegionState
local WorldStreamingState = GlobalCore.WorldStreamingState

local SIGNIFICANT_PUBLISH_OVERRUN_MILLISECONDS = 4.0
local NON_ACTIVE_CACHE_REGION_LIMIT = 32
local NON_ACTIVE_CACHE_BYTE_LIMIT = 256 * 1024 * 1024
---@class (partial) Global.WorldGameMap.WorldGameMap: GameMap
local WorldGameMap = {}

---@alias WorldGameMapImplState Global.WorldGameMap.WorldGameMap

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
    self._worldMovedActorRecorder = nil
    self:_initialiseWorldActorState(config, reservedTags)
    self._worldStreamJob = nil
    self._worldStreamJobRegions = {}
    self._worldStreamBatchRegions = {}
    self._worldActiveRect = nil
    self._worldPreparedRect = nil
    self._worldStreamingCameraPosition = nil
    self._worldDisposed = false
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
    local worldSize = sf.Vector2u.new(config.width, config.height)
    local regionRects = {}
    for _, region in ipairs(self._worldRegions) do
        region.payload = nil
        region.publishState = nil
        region.backgroundBuilder = nil
        region.wasActive = false
        region.wakeTags = nil
        region.activeChunkGeneration = nil
        regionRects[#regionRects + 1] = sf.IntRect.new(region.x, region.y, region.width, region.height)
    end
    ---@cast worldSize sf.Vector2u
    ---@cast regionRects sf.IntRect[]
    self._worldStreamingState = WorldStreamingState.new(
        regionRects, NON_ACTIVE_CACHE_REGION_LIMIT, NON_ACTIVE_CACHE_BYTE_LIMIT
    )
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

---@param movedActorRecorder fun(actor: Engine.Actor, definitionRegion: string, currentRegion: string, layerName: string, position: sf.Vector2i)
function WorldGameMap:setMovedActorPersistenceCallback(movedActorRecorder)
    self._worldMovedActorRecorder = movedActorRecorder
end

function WorldGameMap:getStreamingStats()
    local backgroundQueueDepth = 0
    for _, region in ipairs(self._worldRegions) do
        if region.backgroundBuilder ~= nil then
            backgroundQueueDepth = backgroundQueueDepth + 1
        end
    end
    return self._worldStreamingState:getStats(backgroundQueueDepth)
end

function WorldGameMap:disposeStreaming()
    if self._worldDisposed then
        return
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
    self:clearSparseWorld()
    for _, region in ipairs(self._worldRegions) do
        if region.payload ~= nil then
            FogController.removeWorldRegionFog(region.path)
            region.payload = nil
            region.backgroundBuilder = nil
        end
    end
    self._worldStreamingState:reset()
    self._worldActorsByTag = {}
    self._worldActorLayers = {}
    self._worldActorDefinitionRegions = {}
    self._worldActorRoots = {}
    self._worldActorRegions = {}
    self._worldRootStates = {}
    self._worldRootSleepTimes = {}
    self._worldLooseRoots = {}
    self._worldPendingRehomes = {}
    self._worldObservedRootPositions = {}
    self._worldDestroyedRootsDirty = false
    self._worldActiveChunkBounds = nil
    self._worldActiveChunkGeneration = 0
    self._worldActiveChunkReconcilePending = false
    self._worldLooseActiveChunkGeneration = -1
    self._worldActivationDeferred = false
    self._layerMaskTextureCache = {}
    self._worldShaderPrewarmTarget = nil
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
    assert(
        not self._worldStreamingState:isRegionLoaded(region.index), "World region is already loaded: " .. region.path
    )
    local builder = region.backgroundBuilder
    self:setSparseWorldRegion(region.index, payload.tilemap, builder == nil or builder.areActorsReady())
    local regionRect = sf.IntRect.new(region.x, region.y, region.width, region.height)
    ---@cast regionRect sf.IntRect
    local fog = payload.mapData.fog
    if fog == cjson.null then
        fog = nil
    end
    assert(fog == nil or Class.isInstance(fog, "string"), "mapData.fog must be a string")
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
    self._worldStreamingState:completePublish(region.index, sourceBytes * 4 + runtimeBytes, false)
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
    local regionIndex = self:getSparseWorldRegionIndexAt(position)
    local region = regionIndex ~= nil and self._worldRegions[regionIndex] or nil
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
    local regionIndex = self:getSparseWorldRegionIndexAt(position)
    local region = regionIndex ~= nil and self._worldRegions[regionIndex] or nil
    if region == nil or region.payload == nil then
        return nil
    end
    return region.payload.mapData
end

function WorldGameMap:ensureRegionLoadedAt(position)
    local regionIndex = self:getSparseWorldRegionIndexAt(position)
    local region = regionIndex ~= nil and self._worldRegions[regionIndex] or nil
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

function WorldGameMap:_refreshWorldLights()
    local lights = copy(self._worldRuntimeLights)
    for _, region in ipairs(self._worldRegions) do
        if region.payload ~= nil and self._worldStreamingState:getRegionState(region.index) == WorldRegionState.Active then
            for _, light in ipairs(region.payload.lights) do
                lights[#lights + 1] = light
            end
        end
    end
    self._lights = lights
end

function WorldGameMap:_initialiseWorldActorState(config, reservedTags)
    return WorldGameMapActors._initialiseWorldActorState(self, config, reservedTags)
end

function WorldGameMap:setDestroyedActorTagProvider(destroyedActorTagProvider)
    return WorldGameMapActors.setDestroyedActorTagProvider(self, destroyedActorTagProvider)
end

function WorldGameMap:setAddedActorPositionPersistenceCallback(addedActorPositionRecorder)
    return WorldGameMapActors.setAddedActorPositionPersistenceCallback(self, addedActorPositionRecorder)
end

function WorldGameMap:_refreshSuppressedActorTags()
    return WorldGameMapActors._refreshSuppressedActorTags(self)
end

function WorldGameMap:_applySuppressedActorTags()
    return WorldGameMapActors._applySuppressedActorTags(self)
end

function WorldGameMap:suppressActorTag(tag)
    return WorldGameMapActors.suppressActorTag(self, tag)
end

function WorldGameMap:_filterSuppressedRegionActors(region, payload)
    return WorldGameMapActors._filterSuppressedRegionActors(self, region, payload)
end

function WorldGameMap:_ensureWorldLayer(layerName)
    return WorldGameMapActors._ensureWorldLayer(self, layerName)
end

function WorldGameMap:_getRuntimeTagNamespace(position)
    return WorldGameMapActors._getRuntimeTagNamespace(self, position)
end

function WorldGameMap:_trackRuntimeTag(tag)
    return WorldGameMapActors._trackRuntimeTag(self, tag)
end

function WorldGameMap:_allocateRuntimeTag(position)
    return WorldGameMapActors._allocateRuntimeTag(self, position)
end

function WorldGameMap:_indexRegionActor(payload, layerName, actor, region, root)
    return WorldGameMapActors._indexRegionActor(self, payload, layerName, actor, region, root)
end

function WorldGameMap:_indexRegionActors(region)
    return WorldGameMapActors._indexRegionActors(self, region)
end

function WorldGameMap:_registerWorldActorTree(actor, layer)
    return WorldGameMapActors._registerWorldActorTree(self, actor, layer)
end

function WorldGameMap:_unindexWorldActorTree(actor)
    return WorldGameMapActors._unindexWorldActorTree(self, actor)
end

function WorldGameMap:_attachRegionRoot(region, actor, layer, definitionRegion)
    return WorldGameMapActors._attachRegionRoot(self, region, actor, layer, definitionRegion)
end

function WorldGameMap:spawnActor(actor, layer, emitCreateEvent)
    return WorldGameMapActors.spawnActor(self, actor, layer, emitCreateEvent)
end

---@param actor            Engine.Actor
---@param layer            string
---@param definitionRegion string
---@param emitCreateEvent  boolean | nil
function WorldGameMap:spawnPersistedWorldActor(actor, layer, definitionRegion, emitCreateEvent)
    return WorldGameMapActors.spawnPersistedWorldActor(self, actor, layer, definitionRegion, emitCreateEvent)
end

function WorldGameMap:getAllActors()
    return WorldGameMapActors.getAllActors(self)
end

function WorldGameMap:updateActorList()
    return WorldGameMapActors.updateActorList(self)
end

function WorldGameMap:destroyActor(actor)
    return WorldGameMapActors.destroyActor(self, actor)
end

function WorldGameMap:getActorByTag(tag)
    return WorldGameMapActors.getActorByTag(self, tag)
end

function WorldGameMap:removeActorsByTags(tags)
    return WorldGameMapActors.removeActorsByTags(self, tags)
end

function WorldGameMap:getActorLayer(actor)
    return WorldGameMapActors.getActorLayer(self, actor)
end

function WorldGameMap:recordWorldActorPosition(actor, position)
    return WorldGameMapActors.recordWorldActorPosition(self, actor, position)
end

function WorldGameMap:_recordWorldRootPosition(actor, currentRegionPath, position)
    return WorldGameMapActors._recordWorldRootPosition(self, actor, currentRegionPath, position)
end

function WorldGameMap:_rememberWorldRootPosition(root, position)
    return WorldGameMapActors._rememberWorldRootPosition(self, root, position)
end

function WorldGameMap:_getChangedWorldRootPosition(root)
    return WorldGameMapActors._getChangedWorldRootPosition(self, root)
end

function WorldGameMap:_isWorldActorLayerVisible(actor, layerName, visibleRect)
    return WorldGameMapActors._isWorldActorLayerVisible(self, actor, layerName, visibleRect)
end

function WorldGameMap:_removeWorldRoot(roots, root)
    return WorldGameMapActors._removeWorldRoot(self, roots, root)
end

function WorldGameMap:_appendWorldActorOnce(roots, root)
    return WorldGameMapActors._appendWorldActorOnce(self, roots, root)
end

function WorldGameMap:_sleepWorldRoot(root)
    return WorldGameMapActors._sleepWorldRoot(self, root)
end

function WorldGameMap:_sleepWorldRoots(roots)
    return WorldGameMapActors._sleepWorldRoots(self, roots)
end

function WorldGameMap:_activateWorldRoots(roots)
    return WorldGameMapActors._activateWorldRoots(self, roots)
end

function WorldGameMap:_removeRegionRootMetadata(payload, root)
    return WorldGameMapActors._removeRegionRootMetadata(self, payload, root)
end

function WorldGameMap:_initialiseRegionActorPayload(payload, region)
    return WorldGameMapActors._initialiseRegionActorPayload(self, payload, region)
end

function WorldGameMap:_updateWorldActiveChunkGeneration()
    return WorldGameMapActorStreaming._updateWorldActiveChunkGeneration(self)
end

function WorldGameMap:_syncWorldActiveChunkActivation()
    return WorldGameMapActorStreaming._syncWorldActiveChunkActivation(self)
end

function WorldGameMap:_syncRegionActorActivation(region)
    return WorldGameMapActorStreaming._syncRegionActorActivation(self, region)
end

function WorldGameMap:_syncLooseRootActivation()
    return WorldGameMapActorStreaming._syncLooseRootActivation(self)
end

function WorldGameMap:_activateRegion(region)
    return WorldGameMapActorStreaming._activateRegion(self, region)
end

function WorldGameMap:_deactivateRegion(region, state)
    return WorldGameMapActorStreaming._deactivateRegion(self, region, state)
end

function WorldGameMap:_evictRegion(region)
    return WorldGameMapActorStreaming._evictRegion(self, region)
end

function WorldGameMap:_refreshActorRegionDemands()
    return WorldGameMapActorStreaming._refreshActorRegionDemands(self)
end

function WorldGameMap:_getPendingWorldActorTags(_region)
    return WorldGameMapActorStreaming._getPendingWorldActorTags(self, _region)
end

function WorldGameMap:_isPendingWorldActorTag(_region, tag)
    return WorldGameMapActorStreaming._isPendingWorldActorTag(self, _region, tag)
end

function WorldGameMap:_rehomeRegionActors()
    return WorldGameMapActorStreaming._rehomeRegionActors(self)
end

function WorldGameMap:_pruneDestroyedRegionActors()
    return WorldGameMapActorStreaming._pruneDestroyedRegionActors(self)
end

function WorldGameMap:drawMapFogOverlay()
    return WorldGameMapRendering.drawMapFogOverlay(self)
end

function WorldGameMap:_ensureWorldLightingTargets()
    return WorldGameMapRendering._ensureWorldLightingTargets(self)
end

function WorldGameMap:_drawWorldTileMaskLayer(
    target, baseStates, layerName, layer, region, viewPosition, viewSize, viewRotation
)
    return WorldGameMapRendering._drawWorldTileMaskLayer(
        self, target, baseStates, layerName, layer, region, viewPosition, viewSize, viewRotation
    )
end

function WorldGameMap:_releaseWorldRegionTileMaskCache(region)
    return WorldGameMapRendering._releaseWorldRegionTileMaskCache(self, region)
end

function WorldGameMap:_rebuildStaticTransmission(activeLights, _staticActors)
    return WorldGameMapRendering._rebuildStaticTransmission(self, activeLights, _staticActors)
end

function WorldGameMap:_renderSurfaceMask()
    return WorldGameMapRendering._renderSurfaceMask(self)
end

function WorldGameMap:_getWorldShaderPrewarmTarget()
    return WorldGameMapRendering._getWorldShaderPrewarmTarget(self)
end

function WorldGameMap:_prewarmWorldShaderPrograms(target)
    return WorldGameMapRendering._prewarmWorldShaderPrograms(self, target)
end

function WorldGameMap:_prewarmWorldViewport(visibleRect, _drain)
    return WorldGameMapRendering._prewarmWorldViewport(self, visibleRect, _drain)
end

function WorldGameMap:_isWorldViewportReady(visibleRect)
    return WorldGameMapRendering._isWorldViewportReady(self, visibleRect)
end

function WorldGameMap:_prepareCameraFrame()
    return WorldGameMapRendering._prepareCameraFrame(self)
end

function WorldGameMap:prepareViewportAt(position)
    return WorldGameMapRendering.prepareViewportAt(self, position)
end

function WorldGameMap:drawMapContent(target, states, _applyPlayerCover)
    return WorldGameMapRendering.drawMapContent(self, target, states, _applyPlayerCover)
end

function WorldGameMap:_preparePlayerCover(layerKeys, playerLayerIndex)
    return WorldGameMapActorRendering._preparePlayerCover(self, layerKeys, playerLayerIndex)
end

function WorldGameMap:_resetTransparentTiles()
    return WorldGameMapActorRendering._resetTransparentTiles(self)
end

function WorldGameMap:_getPlayerLayerIndex(layerKeys)
    return WorldGameMapActorRendering._getPlayerLayerIndex(self, layerKeys)
end

function WorldGameMap:_applyPlayerCover(layer, layerIndex, playerLayerIndex, playerPosition)
    return WorldGameMapActorRendering._applyPlayerCover(self, layer, layerIndex, playerLayerIndex, playerPosition)
end

function WorldGameMap:_drawLayerActors(target, states, layerName, layerIndex, playerLayerIndex, applyPlayerCover)
    return WorldGameMapActorRendering._drawLayerActors(
        self, target, states, layerName, layerIndex, playerLayerIndex, applyPlayerCover
    )
end

function WorldGameMap:_prepareActorPixelShatterEffects()
    return WorldGameMapActorRendering._prepareActorPixelShatterEffects(self)
end

function WorldGameMap:_drawActorPixelShatterEffects(target, layerName)
    return WorldGameMapActorRendering._drawActorPixelShatterEffects(self, target, layerName)
end

function WorldGameMap:_drawActor(target, states, actor, actorAlpha)
    return WorldGameMapActorRendering._drawActor(self, target, states, actor, actorAlpha)
end

function WorldGameMap:_drawActorShaderWithHue(target, actor, actorShader, hue, actorAlpha)
    return WorldGameMapActorRendering._drawActorShaderWithHue(self, target, actor, actorShader, hue, actorAlpha)
end

function WorldGameMap:_ensureActorShaderBuffer(size)
    return WorldGameMapActorRendering._ensureActorShaderBuffer(self, size)
end

function WorldGameMap:_ensureActorHueBuffer(size)
    return WorldGameMapActorRendering._ensureActorHueBuffer(self, size)
end

function WorldGameMap:_ensureActorHueSourceSprite(texture)
    return WorldGameMapActorRendering._ensureActorHueSourceSprite(self, texture)
end

function WorldGameMap:_applyActorHueUniform(hue)
    return WorldGameMapActorRendering._applyActorHueUniform(self, hue)
end

function WorldGameMap:_initialiseWorldRendering()
    return WorldGameMapLighting._initialiseWorldRendering(self)
end

function WorldGameMap:_getMaterialShader()
    return WorldGameMapLighting._getMaterialShader(self)
end

function WorldGameMap:refreshShader()
    return WorldGameMapLighting.refreshShader(self)
end

function WorldGameMap:_lightingShadersAvailable()
    return WorldGameMapLighting._lightingShadersAvailable(self)
end

function WorldGameMap:_partitionLightBlockingActors(visibleActors)
    return WorldGameMapLighting._partitionLightBlockingActors(self, visibleActors)
end

function WorldGameMap:_staticTransmissionActorsMatch(actors)
    return WorldGameMapLighting._staticTransmissionActorsMatch(self, actors)
end

function WorldGameMap:_cacheStaticTransmissionActors(actors)
    return WorldGameMapLighting._cacheStaticTransmissionActors(self, actors)
end

function WorldGameMap:_surfaceMaskActorsMatch(actors)
    return WorldGameMapLighting._surfaceMaskActorsMatch(self, actors)
end

function WorldGameMap:_cacheSurfaceMaskActors(actors)
    return WorldGameMapLighting._cacheSurfaceMaskActors(self, actors)
end

function WorldGameMap:_renderedLightingMatches(activeLights, dynamicOccluders)
    return WorldGameMapLighting._renderedLightingMatches(self, activeLights, dynamicOccluders)
end

function WorldGameMap:_cacheRenderedLighting(activeLights, dynamicOccluders)
    return WorldGameMapLighting._cacheRenderedLighting(self, activeLights, dynamicOccluders)
end

function WorldGameMap:_renderDynamicLighting(activeLights, analyses)
    return WorldGameMapLighting._renderDynamicLighting(self, activeLights, analyses)
end

function WorldGameMap:_renderCachedLighting(activeLights, analyses)
    return WorldGameMapLighting._renderCachedLighting(self, activeLights, analyses)
end

function WorldGameMap:_getStaticTransmissionSignature()
    return WorldGameMapLighting._getStaticTransmissionSignature(self)
end

function WorldGameMap:_setTileMaskUniforms(cacheKey, layer, worldMask, regionRevision)
    return WorldGameMapLighting._setTileMaskUniforms(self, cacheKey, layer, worldMask, regionRevision)
end

function WorldGameMap:_setActorMaskUniforms(actor)
    return WorldGameMapLighting._setActorMaskUniforms(self, actor)
end

function WorldGameMap:_renderLighting(activeLights)
    return WorldGameMapLightingPass._renderLighting(self, activeLights)
end

function WorldGameMap:_ensureDynamicTransmission(activeLights)
    return WorldGameMapLightingPass._ensureDynamicTransmission(self, activeLights)
end

function WorldGameMap:_ensureDirectLight()
    return WorldGameMapLightingPass._ensureDirectLight(self)
end

function WorldGameMap:_ensureStaticDirectLight()
    return WorldGameMapLightingPass._ensureStaticDirectLight(self)
end

function WorldGameMap:_setLightPassCommonUniforms()
    return WorldGameMapLightingPass._setLightPassCommonUniforms(self)
end

function WorldGameMap:_setLightPassWorldUniforms()
    return WorldGameMapLightingPass._setLightPassWorldUniforms(self)
end

function WorldGameMap:_setLightPassCacheUniforms(target, light)
    return WorldGameMapLightingPass._setLightPassCacheUniforms(self, target, light)
end

function WorldGameMap:_ensureStaticLightCache(index, entry)
    return WorldGameMapLightingPass._ensureStaticLightCache(self, index, entry)
end

function WorldGameMap:_setLightPassTextureUniforms()
    return WorldGameMapLightingPass._setLightPassTextureUniforms(self)
end

function WorldGameMap:_setViewShaderUniforms(shader, screenSize, mapViewOffset, usesFragmentCoordinates)
    return WorldGameMapLightingPass._setViewShaderUniforms(
        self, shader, screenSize, mapViewOffset, usesFragmentCoordinates
    )
end

function WorldGameMap:_renderLight(entry, dynamicOrigin, dynamicSize, traceStatic, traceDynamic, target)
    return WorldGameMapLightingPass._renderLight(
        self, entry, dynamicOrigin, dynamicSize, traceStatic, traceDynamic, target
    )
end

function WorldGameMap:_renderStaticLights(entries, target)
    return WorldGameMapLightingPass._renderStaticLights(self, entries, target)
end

function WorldGameMap:_renderUnobstructedLights(entries, target)
    return WorldGameMapLightingPass._renderUnobstructedLights(self, entries, target)
end

function WorldGameMap:_lightsMatchCache(entries, cache)
    return WorldGameMapLightingPass._lightsMatchCache(self, entries, cache)
end

function WorldGameMap:_cacheUnobstructedLights(entries)
    return WorldGameMapLightingPass._cacheUnobstructedLights(self, entries)
end

function WorldGameMap:_cacheLightValues(light)
    return WorldGameMapLightingPass._cacheLightValues(self, light)
end

function WorldGameMap:_cacheLightList(entries)
    return WorldGameMapLightingPass._cacheLightList(self, entries)
end

function WorldGameMap:_appendLightBatch(vertices, vertex, light, index)
    return WorldGameMapLightingPass._appendLightBatch(self, vertices, vertex, light, index)
end

function WorldGameMap:_appendLightBatchVertex(vertices, vertex, x, y, textureX, textureY, colour)
    return WorldGameMapLightingPass._appendLightBatchVertex(self, vertices, vertex, x, y, textureX, textureY, colour)
end

function WorldGameMap:_renderDynamicTransmission(analysis)
    return WorldGameMapLightingPass._renderDynamicTransmission(self, analysis)
end

function WorldGameMap:_getActiveLights()
    return WorldGameMapLightingPass._getActiveLights(self)
end

function WorldGameMap:_getActorLightPosition(actor, lightComp, result)
    return WorldGameMapLightingPass._getActorLightPosition(self, actor, lightComp, result)
end

function WorldGameMap:_isLightVisible(position, radius, viewport)
    return WorldGameMapLightingPass._isLightVisible(self, position, radius, viewport)
end

function WorldGameMap:_toShaderColour(colour, applyAlpha)
    return WorldGameMapLightingPass._toShaderColour(self, colour, applyAlpha)
end

function WorldGameMap:_cancelRegionPublish(region)
    return WorldGameMapRegionPublishing._cancelRegionPublish(self, region)
end

function WorldGameMap:_beginRegionPublish(region, data, forceActivate, priorityRect)
    return WorldGameMapRegionPublishing._beginRegionPublish(self, region, data, forceActivate, priorityRect)
end

function WorldGameMap:_beginRegionConversion(region, conversion, contentBytes, forceActivate, priorityRect)
    return WorldGameMapRegionPublishing._beginRegionConversion(
        self, region, conversion, contentBytes, forceActivate, priorityRect
    )
end

function WorldGameMap:_prepareNextRegionRoot(state)
    return WorldGameMapRegionPublishing._prepareNextRegionRoot(self, state)
end

function WorldGameMap:_stepRegionPublish(region, deadline)
    return WorldGameMapRegionPublishing._stepRegionPublish(self, region, deadline)
end

function WorldGameMap:_pumpRegionBackgroundActors(region, builder, deadline)
    return WorldGameMapRegionPublishing._pumpRegionBackgroundActors(self, region, builder, deadline)
end

function WorldGameMap:_drainRegionActors(region)
    return WorldGameMapRegionPublishing._drainRegionActors(self, region)
end

function WorldGameMap:_pumpRegionBackgroundBuilds(deadline)
    return WorldGameMapRegionPublishing._pumpRegionBackgroundBuilds(self, deadline)
end

function WorldGameMap:_drainRegionPublish(region)
    return WorldGameMapRegionPublishing._drainRegionPublish(self, region)
end

function WorldGameMap:_pumpRegionPublishing(deadline)
    return WorldGameMapRegionPublishing._pumpRegionPublishing(self, deadline)
end

function WorldGameMap:_publishRegion(region, data, activate)
    return WorldGameMapRegionPublishing._publishRegion(self, region, data, activate)
end

function WorldGameMap:_enforceCacheBudget()
    return WorldGameMapRegionPublishing._enforceCacheBudget(self)
end

function WorldGameMap:_getActiveActorCount()
    return WorldGameMapRegionPublishing._getActiveActorCount(self)
end

function WorldGameMap:_getVisibleTileChunkCount()
    return WorldGameMapRegionPublishing._getVisibleTileChunkCount(self)
end

function WorldGameMap:_recordStreamingProfile()
    return WorldGameMapRegionPublishing._recordStreamingProfile(self)
end

function WorldGameMap:_syncStreamingCamera()
    return WorldGameMapStreaming._syncStreamingCamera(self)
end

function WorldGameMap:_getVisibleCellRect()
    return WorldGameMapStreaming._getVisibleCellRect(self)
end

function WorldGameMap:_getGameplayCellRect()
    return WorldGameMapStreaming._getGameplayCellRect(self)
end

function WorldGameMap:_refreshStreamingStates()
    return WorldGameMapStreaming._refreshStreamingStates(self)
end

function WorldGameMap:_isRegionDemanded(region)
    return WorldGameMapStreaming._isRegionDemanded(self, region)
end

function WorldGameMap:_streamBatchHasDemand()
    return WorldGameMapStreaming._streamBatchHasDemand(self)
end

function WorldGameMap:_finishStreamingBatch(requeue)
    return WorldGameMapStreaming._finishStreamingBatch(self, requeue)
end

function WorldGameMap:_cancelExpiredStreamingBatch()
    return WorldGameMapStreaming._cancelExpiredStreamingBatch(self)
end

function WorldGameMap:_startStreamingBatch()
    return WorldGameMapStreaming._startStreamingBatch(self)
end

function WorldGameMap:_consumeStreamingItem(item)
    return WorldGameMapStreaming._consumeStreamingItem(self, item)
end

function WorldGameMap:_pumpStreaming()
    return WorldGameMapStreaming._pumpStreaming(self)
end

return class(WorldGameMap, GameMap)
