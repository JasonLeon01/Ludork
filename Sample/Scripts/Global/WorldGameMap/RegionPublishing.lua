local GlobalCore = require("GlobalCore")
local WorldGeometry = require("Global.WorldGeometry")

local System = GlobalCore.System
local WorldRegionDemand = GlobalCore.WorldRegionDemand
local WorldRegionState = GlobalCore.WorldRegionState
local STREAM_PUBLISH_BUDGET_SECONDS = 0.00025
local STREAM_CONVERSION_NODE_BUDGET = 64

---@type WorldGameMapImplState
local WorldGameMapRegionPublishing = {}

---@param world        Global.WorldGameMap.WorldGameMap
---@param stage        string
---@param milliseconds number
local function recordPublishStage(world, stage, milliseconds)
    if milliseconds > world._worldPublishSlowStageMilliseconds then
        world._worldPublishSlowStage = stage
        world._worldPublishSlowStageMilliseconds = milliseconds
    end
end

---@param world   Global.WorldGameMap.WorldGameMap
---@param builder Global.WorldGameMap.RegionBuildState
local function recordBuilderStage(world, builder)
    recordPublishStage(world, builder.lastStepMaximumStage, builder.lastStepMaximumMilliseconds)
end

---@param builder Global.WorldGameMap.RegionBuildState
local function resetBuilderStage(builder)
    builder.lastStepMaximumStage = "idle"
    builder.lastStepMaximumMilliseconds = 0.0
    builder.lastStepResumeCount = 0
end

---@param world         Global.WorldGameMap.WorldGameMap
---@param region        Source.SceneComponents.WorldRegionData
---@param forceActivate boolean
---@return Global.WorldGameMap.RegionPublishState | nil
local function beginRegionPublishState(world, region, forceActivate)
    if region.publishState ~= nil then
        if forceActivate then
            region.publishState.forceActivate = true
            world._worldStreamingState:beginPublish(region.index, true)
        end
        return nil
    end
    assert(region.payload == nil, "World region is already installed: " .. region.path)
    local state = {
        forceActivate = forceActivate,
        phase = "build",
        layerNames = {},
        layerIndex = 1,
        rootIndex = 1,
        actorIndex = 1,
        actorLayer = "",
        indexedActors = {},
        definitionRoots = {}
    }
    region.publishState = state
    world._worldStreamingState:beginPublish(region.index, forceActivate)
    return state
end

---@param region Source.SceneComponents.WorldRegionData
function WorldGameMapRegionPublishing:_cancelRegionPublish(region)
    local state = region.publishState
    if state == nil then
        return
    end
    if state.conversion ~= nil then
        asyncio.clear_file_batch_json(state.conversion)
        state.conversion = nil
    end
    local indexedRoots = {}
    for _, actor in ipairs(state.indexedActors or {}) do
        local root = self._worldActorRoots[actor]
        if root ~= nil and not indexedRoots[root] and self._worldActorRegions[root] == region then
            indexedRoots[root] = true
            self:_unindexWorldActorTree(root)
        end
    end
    for _, root in ipairs(state.definitionRoots or {}) do
        self._worldActorDefinitionRegions[root] = nil
    end
    if state.payload ~= nil then
        state.payload.actorSet = {}
        state.payload.actorRoots = {}
        state.payload.activeRoots = {}
    end
    region.publishState = nil
    if region.payload == nil then
        self._worldStreamingState:cancelPublish(region.index, false)
    end
end

---@param region        Source.SceneComponents.WorldRegionData
---@param data          Source.SceneComponents.SerializedMapData
---@param forceActivate boolean
---@param priorityRect  Global.WorldGeometry.CellRect | nil
function WorldGameMapRegionPublishing:_beginRegionPublish(region, data, forceActivate, priorityRect)
    local state = beginRegionPublishState(self, region, forceActivate)
    if state == nil then
        return
    end
    state.builder = self._worldRegionFactory(region, data, priorityRect)
    state.phase = "build"
end

---@param region        Source.SceneComponents.WorldRegionData
---@param conversion    FileBatchJsonConversion
---@param contentBytes  integer
---@param forceActivate boolean
---@param priorityRect  Global.WorldGeometry.CellRect | nil
function WorldGameMapRegionPublishing:_beginRegionConversion(
    region, conversion, contentBytes, forceActivate, priorityRect
)
    local state = beginRegionPublishState(self, region, forceActivate)
    if state == nil then
        asyncio.clear_file_batch_json(conversion)
        return
    end
    state.conversion = conversion
    state.contentBytes = contentBytes
    state.priorityRect = priorityRect
    state.phase = "convert"
end

---@param state Global.WorldGameMap.RegionPublishState
---@return boolean
function WorldGameMapRegionPublishing:_prepareNextRegionRoot(state)
    local payload = assert(state.payload, "World region payload is not built")
    while state.layerIndex <= #state.layerNames do
        local layerName = state.layerNames[state.layerIndex]
        local roots = payload.actors[layerName] or {}
        local root = roots[state.rootIndex]
        if root ~= nil then
            state.rootIndex = state.rootIndex + 1
            local definitionRegion = payload.definitionRegions[root]
            if definitionRegion ~= nil then
                self._worldActorDefinitionRegions[root] = definitionRegion
                state.definitionRoots[#state.definitionRoots + 1] = root
            end
            state.actorQueue = { root }
            state.actorIndex = 1
            state.actorLayer = layerName
            state.actorRoot = root
            return true
        end
        state.layerIndex = state.layerIndex + 1
        state.rootIndex = 1
    end
    return false
end

---@param region   Source.SceneComponents.WorldRegionData
---@param deadline number
---@return boolean
function WorldGameMapRegionPublishing:_stepRegionPublish(region, deadline)
    local state = assert(region.publishState)
    if state.phase == "convert" then
        local remainingMilliseconds = (deadline - perfCounter()) * 1000.0
        if remainingMilliseconds <= 0.0 then
            return false
        end
        local conversion = assert(state.conversion, "World region JSON conversion is unavailable")
        local completed, _, data = asyncio.step_file_batch_json(
            conversion, STREAM_CONVERSION_NODE_BUDGET, remainingMilliseconds
        )
        if not completed then
            return false
        end
        state.conversion = nil
        data = assert(data, "World region JSON root must be an object: " .. region.path)
        ---@cast data Source.SceneComponents.SerializedMapData
        state.builder = self._worldRegionFactory(region, data, state.priorityRect)
        state.priorityRect = nil
        state.phase = "build"
    end
    if state.phase == "build" then
        if perfCounter() >= deadline then
            return false
        end
        local builder = assert(state.builder, "World region builder is unavailable")
        local payload = builder.step(deadline)
        recordBuilderStage(self, builder)
        if payload == nil then
            return false
        end
        state.payload = payload
        self:_filterSuppressedRegionActors(region, payload)
        payload.actorSet = {}
        state.layerNames = {}
        for layerName in pairs(payload.actors) do
            state.layerNames[#state.layerNames + 1] = layerName
        end
        state.layerIndex = 1
        state.rootIndex = 1
        state.phase = "index"
    end
    local payload = assert(state.payload, "World region payload is not built")
    while state.phase == "index" and perfCounter() < deadline do
        if state.actorQueue == nil and not self:_prepareNextRegionRoot(state) then
            state.phase = "finalise"
            break
        end
        local actorQueue = assert(state.actorQueue, "World region Actor queue is not prepared")
        local actor = actorQueue[state.actorIndex]
        ---@cast actor Engine.Actor
        local actorStarted = perfCounter()
        self:_indexRegionActor(payload, state.actorLayer, actor, region, assert(state.actorRoot))
        state.indexedActors[#state.indexedActors + 1] = actor
        for _, child in ipairs(actor:getChildren()) do
            actorQueue[#actorQueue + 1] = child
        end
        state.actorIndex = state.actorIndex + 1
        if state.actorIndex > #actorQueue then
            state.actorQueue = nil
            state.actorRoot = nil
        end
        recordPublishStage(self, "publishInitialActorTree", (perfCounter() - actorStarted) * 1000.0)
    end
    if state.phase ~= "finalise" or perfCounter() >= deadline then
        return false
    end
    region.payload = payload
    region.geometryRevision = state.builder ~= nil and state.builder.geometryRevision or 0
    region.lightingRevision = state.builder ~= nil and state.builder.lightingRevision or 0
    if state.builder ~= nil and not state.builder.completed then
        region.backgroundBuilder = state.builder
    end
    region.publishState = nil
    local installStarted = perfCounter()
    self:_completeRegionInstall(
        region,
        state.forceActivate or self._worldStreamingState:getRegionDemand(region.index) == WorldRegionDemand.Active,
        state.contentBytes
    )
    recordPublishStage(self, "installRegion", (perfCounter() - installStarted) * 1000.0)
    return true
end

---@param payload Global.WorldGameMap.RegionPayload
---@param root    Engine.Actor
---@return boolean
local function payloadContainsRoot(payload, root)
    for _, roots in pairs(payload.actors) do
        if table.contains(roots, root) then
            return true
        end
    end
    return false
end

---@param region   Source.SceneComponents.WorldRegionData
---@param builder  Global.WorldGameMap.RegionBuildState
---@param deadline number
---@return boolean
function WorldGameMapRegionPublishing:_pumpRegionBackgroundActors(region, builder, deadline)
    local payload = assert(region.payload)
    local started = perfCounter()
    local worked = builder.actorPublishQueue ~= nil
    while perfCounter() < deadline do
        if builder.actorPublishQueue == nil then
            local record = table.remove(builder.readyActorRoots, 1)
            if record == nil then
                if worked then
                    recordPublishStage(self, "publishActorTree", (perfCounter() - started) * 1000.0)
                end
                return true
            end
            worked = true
            self:_filterSuppressedRegionActors(region, payload)
            if payloadContainsRoot(payload, record.actorRoot) then
                builder.actorPublishRoot = record.actorRoot
                builder.actorPublishLayer = record.actorLayer
                builder.actorPublishQueue = { record.actorRoot }
                builder.actorPublishIndex = 1
            end
        end
        local actorQueue = builder.actorPublishQueue
        if actorQueue ~= nil then
            local actorPublishIndex = assert(builder.actorPublishIndex)
            local actor = actorQueue[actorPublishIndex]
            ---@cast actor Engine.Actor
            self:_indexRegionActor(
                payload, assert(builder.actorPublishLayer), actor, region, assert(builder.actorPublishRoot)
            )
            for _, child in ipairs(actor:getChildren()) do
                actorQueue[#actorQueue + 1] = child
            end
            actorPublishIndex = actorPublishIndex + 1
            builder.actorPublishIndex = actorPublishIndex
            if actorPublishIndex > #actorQueue then
                builder.actorPublishQueue = nil
                builder.actorPublishRoot = nil
                builder.actorPublishLayer = nil
            end
        end
    end
    if worked then
        recordPublishStage(self, "publishActorTree", (perfCounter() - started) * 1000.0)
    end
    return builder.actorPublishQueue == nil and not bool(builder.readyActorRoots)
end

---@param region Source.SceneComponents.WorldRegionData
function WorldGameMapRegionPublishing:_drainRegionActors(region)
    local builder = region.backgroundBuilder
    if builder == nil then
        return
    end
    local started = perfCounter()
    while not builder.areActorsReady() do
        self:_pumpRegionBackgroundActors(region, builder, math.huge)
        if not builder.areActorsReady() then
            builder.prepareActors(math.huge)
            recordBuilderStage(self, builder)
        end
    end
    self:setSparseWorldRegionActorsReady(region.index)
    if builder.completed then
        region.backgroundBuilder = nil
    end
    self._worldPublishMilliseconds = self._worldPublishMilliseconds + (perfCounter() - started) * 1000.0
end

---@param deadline number
function WorldGameMapRegionPublishing:_pumpRegionBackgroundBuilds(deadline)
    if perfCounter() >= deadline then
        return
    end
    local visibleRect = self._camera ~= nil and self:_getVisibleCellRect() or nil
    local urgent = {}
    local background = {}
    for _, region in ipairs(self._worldRegions) do
        local builder = region.backgroundBuilder
        if builder ~= nil and self:_isRegionDemanded(region) then
            if self._worldStreamingState:getRegionDemand(region.index) == WorldRegionDemand.Active and visibleRect
                    ~= nil and not builder.isRectReady(visibleRect) then
                urgent[#urgent + 1] = region
            end
            background[#background + 1] = region
        end
    end
    if bool(urgent) then
        local index = self._worldUrgentBuildCursor % #urgent + 1
        self._worldUrgentBuildCursor = self._worldUrgentBuildCursor + 1
        local region = urgent[index]
        local builder = assert(region.backgroundBuilder)
        ---@cast builder Global.WorldGameMap.RegionBuildState
        local started = perfCounter()
        resetBuilderStage(builder)
        local actorsWereReady = builder.areActorsReady()
        local previousGeometryRevision = region.geometryRevision
        if self:_pumpRegionBackgroundActors(region, builder, deadline) and perfCounter() < deadline then
            local prepareStarted = perfCounter()
            builder.prepareRect(assert(visibleRect), deadline)
            recordPublishStage(self, "prepareVisibleTileChunk", (perfCounter() - prepareStarted) * 1000.0)
            self:_pumpRegionBackgroundActors(region, builder, deadline)
        end
        recordBuilderStage(self, builder)
        region.geometryRevision = builder.geometryRevision
        local actorsAreReady = builder.areActorsReady()
        if not actorsWereReady and actorsAreReady then
            self:setSparseWorldRegionActorsReady(region.index)
        end
        if region.lightingRevision ~= builder.lightingRevision then
            region.lightingRevision = builder.lightingRevision
            self:_refreshWorldLights()
        end
        if self._worldStreamingState:getRegionState(region.index) == WorldRegionState.Active
            and (region.geometryRevision ~= previousGeometryRevision or not actorsWereReady and actorsAreReady) then
            self:_syncRegionActorActivation(region)
        end
        self._worldPublishMilliseconds = self._worldPublishMilliseconds + (perfCounter() - started) * 1000.0
        return
    end
    if bool(background) then
        local index = self._worldBackgroundBuildCursor % #background + 1
        self._worldBackgroundBuildCursor = self._worldBackgroundBuildCursor + 1
        local region = background[index]
        local builder = assert(region.backgroundBuilder)
        ---@cast builder Global.WorldGameMap.RegionBuildState
        local started = perfCounter()
        resetBuilderStage(builder)
        local actorsWereReady = builder.areActorsReady()
        local previousGeometryRevision = region.geometryRevision
        if self:_pumpRegionBackgroundActors(region, builder, deadline) and perfCounter() < deadline then
            builder.step(deadline)
            self:_pumpRegionBackgroundActors(region, builder, deadline)
        end
        recordBuilderStage(self, builder)
        region.geometryRevision = builder.geometryRevision
        local actorsAreReady = builder.areActorsReady()
        if not actorsWereReady and actorsAreReady then
            self:setSparseWorldRegionActorsReady(region.index)
        end
        if region.lightingRevision ~= builder.lightingRevision then
            region.lightingRevision = builder.lightingRevision
            self:_refreshWorldLights()
        end
        if self._worldStreamingState:getRegionState(region.index) == WorldRegionState.Active
            and (region.geometryRevision ~= previousGeometryRevision or not actorsWereReady and actorsAreReady) then
            self:_syncRegionActorActivation(region)
        end
        if builder.completed and builder.actorPublishQueue == nil and not bool(builder.readyActorRoots) then
            region.backgroundBuilder = nil
        end
        self._worldPublishMilliseconds = self._worldPublishMilliseconds + (perfCounter() - started) * 1000.0
    end
end

---@param region Source.SceneComponents.WorldRegionData
function WorldGameMapRegionPublishing:_drainRegionPublish(region)
    local started = perfCounter()
    while region.publishState ~= nil do
        local deadline = region.publishState.phase == "convert" and perfCounter() + STREAM_PUBLISH_BUDGET_SECONDS
            or math.huge
        self:_stepRegionPublish(region, deadline)
    end
    self._worldPublishMilliseconds = self._worldPublishMilliseconds + (perfCounter() - started) * 1000.0
end

---@param deadline number
function WorldGameMapRegionPublishing:_pumpRegionPublishing(deadline)
    while perfCounter() < deadline do
        local regionIndex = self._worldStreamingState:takePublishItem()
        if regionIndex == nil then
            break
        end
        local region = assert(self._worldRegions[regionIndex], "Native world publish queue returned an invalid region")
        if region.publishState ~= nil then
            if self:_isRegionDemanded(region) or region.publishState.forceActivate then
                local started = perfCounter()
                local completed = self:_stepRegionPublish(region, deadline)
                self._worldPublishMilliseconds = self._worldPublishMilliseconds + (perfCounter() - started) * 1000.0
                if not completed then
                    self._worldStreamingState:deferPublish(region.index)
                end
            else
                self:_cancelRegionPublish(region)
            end
        end
    end
end

---@param region   Source.SceneComponents.WorldRegionData
---@param data     Source.SceneComponents.SerializedMapData
---@param activate boolean
function WorldGameMapRegionPublishing:_publishRegion(region, data, activate)
    self:_beginRegionPublish(region, data, activate, self._camera ~= nil and self:_getVisibleCellRect() or nil)
    self:_drainRegionPublish(region)
end

function WorldGameMapRegionPublishing:_enforceCacheBudget()
    for _, regionIndex in ipairs(self._worldStreamingState:getEvictionList()) do
        self:_evictRegion(assert(self._worldRegions[regionIndex], "Native world eviction list returned an invalid region"))
    end
end

---@return integer
function WorldGameMapRegionPublishing:_getActiveActorCount()
    local actors = {}
    local count = 0
    for _, actorList in pairs(self._actors) do
        for _, actor in ipairs(actorList) do
            if not actor:isDestroyed() and not actors[actor] then
                actors[actor] = true
                count = count + 1
            end
        end
    end
    return count
end

---@return integer
function WorldGameMapRegionPublishing:_getVisibleTileChunkCount()
    local visible = self:_getVisibleCellRect()
    local count = 0
    for _, region in ipairs(self._worldRegions) do
        if region.payload ~= nil and WorldGeometry.RectIntersects(region, visible) then
            for _, layerName in ipairs(self._worldConfig.layerOrder) do
                local layer = region.payload.tilemap:getLayer(layerName)
                if layer ~= nil and layer.visible then
                    count = count + layer:getLastVisibleChunkCount()
                end
            end
        end
    end
    return count
end

function WorldGameMapRegionPublishing:_recordStreamingProfile()
    if not System.isPerformanceProfilerEnabled() then
        return
    end
    local stats = self:getStreamingStats()
    System.recordWorldStreamingPerformance(
        stats.queued, stats.Reading, stats.Prepared, stats.Active, stats.Dormant, stats.cacheBytes,
        self._worldPublishMilliseconds, self:_getVisibleTileChunkCount(), self:_getActiveActorCount()
    )
end

return WorldGameMapRegionPublishing
