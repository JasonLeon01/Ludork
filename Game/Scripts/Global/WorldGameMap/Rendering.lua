local GlobalCore = require("GlobalCore")
local GameMap = require("Global.GameMap")
local WorldGeometry = require("Global.WorldGeometry")

local WorldRegionDemand = GlobalCore.WorldRegionDemand
local WorldRegionState = GlobalCore.WorldRegionState

local WorldGameMapRendering = {}

---@param visibleRect Global.WorldGeometry.CellRect
---@param _drain boolean
---@param self WorldGameMapImplState
---@return boolean
function WorldGameMapRendering.PrewarmWorldViewport(self, visibleRect, _drain)
    if PLATFORM == "ohos" then
        return true
    end
    local changed = not self._worldShadersPrewarmed
    for _, region in ipairs(self._worldRegions) do
        if region.payload ~= nil and WorldGeometry.RectIntersects(region, visibleRect) then
            if region.payload.prewarmedGeometryRevision ~= region.geometryRevision then
                changed = true
                region.payload.prewarmedGeometryRevision = region.geometryRevision
            end
        end
    end
    if changed then
        local started = perfCounter()
        assert(self._renderer):prewarm(self._ambientLight, self._shaderTime)
        self._worldShadersPrewarmed = true
        self._worldPrewarmMilliseconds = self._worldPrewarmMilliseconds + (perfCounter() - started) * 1000.0
    end
    return true
end

---@param visibleRect Global.WorldGeometry.CellRect
---@return boolean
---@param self        WorldGameMapImplState
function WorldGameMapRendering.IsWorldViewportReady(self, visibleRect)
    for _, region in ipairs(self._worldRegions) do
        if WorldGeometry.RectIntersects(region, visibleRect) then
            if region.payload == nil or self._worldStreamingState:getRegionState(region.index)
                    ~= WorldRegionState.Active or region.publishState ~= nil then
                return false
            end
            if region.backgroundBuilder ~= nil and not region.backgroundBuilder.isRectReady(visibleRect) then
                return false
            end
        end
    end
    return true
end

---@param position sf.Vector2f | nil
---@return boolean
---@param self     WorldGameMapImplState
function WorldGameMapRendering.PrepareWorldCameraPosition(self, position)
    if position ~= nil then
        assert(self._camera):setViewPosition(position)
    end
    local visibleRect = self:_getVisibleCellRect()
    return self:_isWorldViewportReady(visibleRect) and self:_prewarmWorldViewport(visibleRect, false)
end

---@return boolean
---@param self WorldGameMapImplState
function WorldGameMapRendering.PrepareCameraFrame(self)
    if self._camera == nil then
        return true
    end
    self._camera:syncFollowTarget()
    local desiredPosition = self._camera:getViewPosition()
    if self:_prepareWorldCameraPosition(desiredPosition) then
        self._worldLastReadyCameraPosition = desiredPosition ~= nil and desiredPosition:copy() or nil
        return true
    end
    if self._worldStreamingCameraPosition ~= nil
        and self:_prepareWorldCameraPosition(self._worldStreamingCameraPosition) then
        self._worldLastReadyCameraPosition = self._worldStreamingCameraPosition:copy()
        return true
    end
    if self._worldLastReadyCameraPosition ~= nil then
        if self:_prepareWorldCameraPosition(self._worldLastReadyCameraPosition) then
            return true
        end
    end
    return false
end

---@param region       Source.SceneComponents.WorldRegionData
---@param builder      Global.WorldGameMap.RegionBuildState
---@param requiredRect Global.WorldGeometry.CellRect
---@param self         WorldGameMapImplState
function WorldGameMapRendering.PrepareWorldRegionRect(self, region, builder, requiredRect)
    local started = perfCounter()
    while not builder.isRectReady(requiredRect) do
        self:_pumpRegionBackgroundActors(region, builder, math.huge)
        local prepareStarted = perfCounter()
        builder.prepareRect(requiredRect, math.huge)
        local prepareMilliseconds = (perfCounter() - prepareStarted) * 1000.0
        if prepareMilliseconds > self._worldPublishSlowStageMilliseconds then
            self._worldPublishSlowStage = "prepareVisibleTileChunk"
            self._worldPublishSlowStageMilliseconds = prepareMilliseconds
        end
        self:_pumpRegionBackgroundActors(region, builder, math.huge)
        region.geometryRevision = builder.geometryRevision
    end
    if builder.completed and builder.actorPublishQueue == nil and not bool(builder.readyActorRoots) then
        region.backgroundBuilder = nil
    end
    local elapsedMilliseconds = (perfCounter() - started) * 1000.0
    self._worldPublishMilliseconds = self._worldPublishMilliseconds + elapsedMilliseconds
end

---@param position sf.Vector2i
---@param self     WorldGameMapImplState
function WorldGameMapRendering.PrepareViewportAt(self, position)
    assert(
        WorldGeometry.RectContainsPosition(self._worldBounds, position),
        "World viewport destination is outside world bounds"
    )
    local player = assert(self._player, "World viewport preparation requires a player")
    local playerPosition = player:getMapPosition()
    assert(
        playerPosition.x == position.x and playerPosition.y == position.y,
        "World viewport destination must match the player position"
    )
    assert(self._camera ~= nil, "World viewport preparation requires a camera")
    self._worldTransitionPublishThisTick = true
    self:_syncStreamingCamera()
    self._worldActivationDeferred = true
    self:_refreshStreamingStates()
    local visibleRect = self:_getVisibleCellRect()
    local activeRect = assert(self._worldActiveRect, "World viewport preparation requires an Active rect")
    local initialActiveRegions = {}
    for _, region in ipairs(self._worldRegions) do
        if self._worldStreamingState:getRegionDemand(region.index) == WorldRegionDemand.Active then
            initialActiveRegions[#initialActiveRegions + 1] = region
        end
    end
    for _, region in ipairs(initialActiveRegions) do
        local regionPosition = sf.Vector2i.new(region.x, region.y)
        ---@cast regionPosition sf.Vector2i
        self:ensureRegionLoadedAt(regionPosition)
    end
    for _, region in ipairs(initialActiveRegions) do
        self:_drainRegionActors(region)
    end
    for _, region in ipairs(initialActiveRegions) do
        if region.backgroundBuilder ~= nil then
            self:_prepareWorldRegionRect(region, region.backgroundBuilder, activeRect)
        end
    end
    self._worldActivationDeferred = false
    for _, region in ipairs(initialActiveRegions) do
        if self._worldStreamingState:getRegionState(region.index) == WorldRegionState.Active then
            self:_syncRegionActorActivation(region)
        end
    end
    self:_syncLooseRootActivation()
    self._worldActiveChunkReconcilePending = false
    self:updateActorOccupancy(player)
    assert(
        self:_prewarmWorldViewport(self._worldActiveRect or visibleRect, true),
        "World viewport shader prewarm did not complete"
    )
    for _, region in ipairs(initialActiveRegions) do
        assert(
            region.payload ~= nil and self._worldStreamingState:getRegionState(region.index) == WorldRegionState.Active,
            "World viewport region was not fully prepared: " .. region.path
        )
        local builder = region.backgroundBuilder
        assert(
            builder == nil or builder.areActorsReady(),
            "World viewport region Actors were not fully prepared: " .. region.path
        )
    end
    self:show()
end

---@param target sf.RenderTarget
---@param states sf.RenderStates | nil
---@param _applyPlayerCover boolean
---@param self WorldGameMapImplState
function WorldGameMapRendering.DrawMapContent(self, target, states, _applyPlayerCover)
    assert(self._renderer):setLayerOrder(self._worldLayerOrder)
    return GameMap.drawMapContent(self, target, states, false)
end

return WorldGameMapRendering
