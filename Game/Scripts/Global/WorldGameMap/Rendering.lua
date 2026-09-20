local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local WorldGeometry = require("Global.WorldGeometry")
local RenderSupport = require("Global.WorldGameMap.RenderSupport")

---@diagnostic disable: need-check-nil, param-type-mismatch, duplicate-set-field

local FogController = GlobalCore.FogController
local WorldRegionDemand = GlobalCore.WorldRegionDemand
local WorldRegionState = GlobalCore.WorldRegionState
local WORLD_SHADER_PREWARM_SIZE = sf.Vector2u.new(1, 1)
---@cast WORLD_SHADER_PREWARM_SIZE sf.Vector2u

local WorldGameMapRendering = {}

---@param self WorldGameMapImplState
function WorldGameMapRendering.DrawMapFogOverlay(self)
    local camera = self._camera
    ---@cast camera GlobalCore.Camera
    FogController.drawWorldOverlay(camera)
end

---@param self WorldGameMapImplState
function WorldGameMapRendering.EnsureWorldLightingTargets(self)
    local targetSize = assert(assert(self._camera):getRenderTexture()):getSize()
    if self._staticTransmission ~= nil and self._surfaceMask ~= nil
        and self._staticTransmission:getSize() == targetSize and self._surfaceMask:getSize() == targetSize then
        return
    end
    self._staticTransmission = sf.RenderTexture.new(targetSize)
    self._staticTransmission:setSmooth(false)
    self._surfaceMask = sf.RenderTexture.new(targetSize)
    self._surfaceMask:setSmooth(false)
    self._staticTransmissionRevision = -1
    self._staticTransmissionSignature = nil
end

---@param target       sf.RenderTexture
---@param baseStates   sf.RenderStates
---@param layerName    string
---@param layer        Engine.TileLayer
---@param region       Source.SceneComponents.WorldRegionData
---@param viewPosition sf.Vector2f
---@param viewSize     sf.Vector2f
---@param viewRotation number
---@param self         WorldGameMapImplState
function WorldGameMapRendering.DrawWorldTileMaskLayer(
    self, target, baseStates, layerName, layer, region, viewPosition, viewSize, viewRotation
)
    self:_setTileMaskUniforms(
        RenderSupport.TileMaskCacheKey(region, layerName), layer,
        RenderSupport.CreateTileMaskConfig(target, viewPosition, viewSize, viewRotation, region),
        region.lightingRevision
    )
    local regionStates = sf.RenderStates.new(baseStates.blendMode)
    regionStates.transform = baseStates.transform:copy()
    regionStates.texture = baseStates.texture
    regionStates.shader = self._tilemapLightMaskShader
    regionStates.transform:translate(sf.Vector2f.new(region.x * Engine.GetCellSize(), region.y * Engine.GetCellSize()))
    target:draw(layer, regionStates)
end

---@param region Source.SceneComponents.WorldRegionData
---@param self   WorldGameMapImplState
function WorldGameMapRendering.ReleaseWorldRegionTileMaskCache(self, region)
    for _, layerName in ipairs(self._worldConfig.layerOrder) do
        self._layerMaskTextureCache[RenderSupport.TileMaskCacheKey(region, layerName)] = nil
    end
end

---@param activeLights  Global.GameMap.ActiveLight[]
---@param _staticActors Engine.Actor[]
---@param self          WorldGameMapImplState
function WorldGameMapRendering.RebuildStaticTransmission(self, activeLights, _staticActors)
    self:_ensureWorldLightingTargets()
    ---@cast self._staticTransmission sf.RenderTexture
    ---@cast self._transmissionTileRenderStates sf.RenderStates
    local visibleRect = self:_getVisibleCellRect()
    local lightingRect = RenderSupport.GetLightingCellRect(
        visibleRect, self._worldPreparedRect or self._worldActiveRect or visibleRect, self:getSize(), activeLights
    )
    ---@type (string | integer | boolean)[]
    local signatureValues = { lightingRect.x, lightingRect.y, lightingRect.width, lightingRect.height }
    for _, region in ipairs(self._worldRegions) do
        if region.payload ~= nil and WorldGeometry.RectIntersects(region, lightingRect) then
            signatureValues[#signatureValues + 1] = region.path
            signatureValues[#signatureValues + 1] = region.geometryRevision or 0
            for _, layerName in ipairs(self._worldConfig.layerOrder) do
                local layer = region.payload.tilemap:getLayer(layerName)
                if layer ~= nil then
                    signatureValues[#signatureValues + 1] = layerName
                    signatureValues[#signatureValues + 1] = bool(layer.visible)
                end
            end
        end
    end
    local signature = tuple(table.unpack(signatureValues))
    ---@diagnostic disable-next-line: cast-type-mismatch, table.unpack cannot express that this contiguous array has no nil slots
    ---@cast signature Global.WorldGameMap.StaticTransmissionSignature
    if self._staticTransmissionRevision == self._materialRevision and self._staticTransmissionSignature == signature then
        return
    end
    local viewPosition = sf.Vector2f.new(lightingRect.x * Engine.GetCellSize(), lightingRect.y * Engine.GetCellSize())
    local viewSize = sf.Vector2f.new(
        lightingRect.width * Engine.GetCellSize(), lightingRect.height * Engine.GetCellSize()
    )
    local viewCentre = sf.Vector2f.new(viewPosition.x + viewSize.x * 0.5, viewPosition.y + viewSize.y * 0.5)
    ---@cast viewPosition sf.Vector2f
    ---@cast viewSize sf.Vector2f
    ---@cast viewCentre sf.Vector2f
    self._staticTextureOrigin = viewPosition:copy()
    self._staticTextureSize = viewSize:copy()
    self._staticOccupancyOrigin = sf.Vector2f.new(lightingRect.x, lightingRect.y)
    self._staticOccupancySize = sf.Vector2f.new(lightingRect.width, lightingRect.height)
    self._staticTransmission:setView(sf.View.new(viewCentre, viewSize))
    self._staticTransmission:clear(sf.Color.White)
    self._tilemapLightMaskShader:setUniform("transmissionMode", 1.0)
    for _, layerName in ipairs(self._worldConfig.layerOrder) do
        for _, region in ipairs(self._worldRegions) do
            if region.payload ~= nil and WorldGeometry.RectIntersects(region, lightingRect) then
                local layer = region.payload.tilemap:getLayer(layerName)
                if layer ~= nil and layer.visible then
                    self:_drawWorldTileMaskLayer(
                        self._staticTransmission, self._transmissionTileRenderStates, layerName, layer, region,
                        viewPosition, viewSize, 0.0
                    )
                end
            end
        end
    end
    self._staticTransmission:display()
    local occupancyOrigin = sf.Vector2i.new(lightingRect.x, lightingRect.y)
    local occupancySize = sf.Vector2u.new(lightingRect.width, lightingRect.height)
    ---@cast occupancyOrigin sf.Vector2i
    ---@cast occupancySize sf.Vector2u
    self._staticOccupancy = assert(self:rebuildStaticLightOccupancy(occupancyOrigin, occupancySize, {}))
    self._staticTransmissionRevision = self._materialRevision
    self._staticTransmissionSignature = signature
    self._staticTransmissionGeneration = self._staticTransmissionGeneration + 1
end

---@return Engine.Actor[]
---@param self WorldGameMapImplState
function WorldGameMapRendering.RenderSurfaceMask(self)
    self:_ensureWorldLightingTargets()
    assert(self._camera ~= nil, "World surface mask requires a camera")
    ---@cast self._surfaceMask sf.RenderTexture
    ---@cast self._surfaceTileRenderStates sf.RenderStates
    ---@cast self._surfaceActorRenderStates sf.RenderStates
    local viewSize = assert(self._camera:getViewSize())
    local viewPosition = assert(self._camera:getViewPosition())
    local cameraRotation = self._camera:getViewRotation()
    local viewRotation = sf.Angle.asDegrees(cameraRotation)
    ---@cast viewSize sf.Vector2f
    ---@cast viewPosition sf.Vector2f
    self._surfaceMask:setView(self._camera:getView())
    self._surfaceMask:clear(sf.Color.Transparent)
    self._tilemapLightMaskShader:setUniform("transmissionMode", 0.0)
    self._lightMaskShader:setUniform("transmissionMode", 0.0)
    local visibleRect = self:_getVisibleCellRect()
    local visibleActors = {}
    local visibleActorSet = {}
    for _, layerName in ipairs(self._worldLayerOrder) do
        for _, region in ipairs(self._worldRegions) do
            if region.payload ~= nil and WorldGeometry.RectIntersects(region, visibleRect) then
                local layer = region.payload.tilemap:getLayer(layerName)
                if layer ~= nil and layer.visible then
                    self:_drawWorldTileMaskLayer(
                        self._surfaceMask, self._surfaceTileRenderStates, layerName, layer, region, viewPosition,
                        viewSize, viewRotation
                    )
                end
            end
        end
        for _, actor in ipairs(self._actors[layerName] or {}) do
            if not actor:isDestroyed() and actor:isVisibleInHierarchy()
                and self:_isWorldActorLayerVisible(actor, layerName, visibleRect) then
                if not visibleActorSet[actor] then
                    visibleActorSet[actor] = true
                    visibleActors[#visibleActors + 1] = actor
                end
                self:_setActorMaskUniforms(actor)
                self._surfaceMask:draw(actor, self._surfaceActorRenderStates)
            end
        end
    end
    self._surfaceMask:display()
    return visibleActors
end

---@return sf.RenderTexture
---@param self WorldGameMapImplState
function WorldGameMapRendering.GetWorldShaderPrewarmTarget(self)
    if self._worldShaderPrewarmTarget == nil then
        self._worldShaderPrewarmTarget = sf.RenderTexture.new(WORLD_SHADER_PREWARM_SIZE)
        self._worldShaderPrewarmTarget:setSmooth(false)
    end
    return self._worldShaderPrewarmTarget
end

---@param target               sf.RenderTexture
---@param sourceTexture        sf.Texture
---@param shader               sf.Shader | nil
---@param prewarmed            boolean
---@param readbackMilliseconds number
---@return boolean, number
local function prewarmShader(target, sourceTexture, shader, prewarmed, readbackMilliseconds)
    if shader == nil then
        return prewarmed, readbackMilliseconds
    end
    local sprite = sf.Sprite.new(sourceTexture)
    local states = sf.RenderStates.new()
    states.shader = shader
    target:clear(sf.Color.Transparent)
    target:draw(sprite, states)
    target:display()
    local readbackStarted = perfCounter()
    target:getTexture():copyToImage()
    return true, readbackMilliseconds + (perfCounter() - readbackStarted) * 1000.0
end

---@param target sf.RenderTexture
---@return boolean
---@param self   WorldGameMapImplState
function WorldGameMapRendering.PrewarmWorldShaderPrograms(self, target)
    if self._worldShadersPrewarmed then
        return false
    end
    self:_ensureWorldLightingTargets()
    self:_ensureDirectLight()
    self:refreshShader()
    local sourceTexture = assert(self._camera):getTexture()
    local programsStarted = false
    programsStarted, self._worldPrewarmReadbackMilliseconds = prewarmShader(
        target, sourceTexture, self._materialShader, programsStarted, self._worldPrewarmReadbackMilliseconds
    )
    programsStarted, self._worldPrewarmReadbackMilliseconds = prewarmShader(
        target, sourceTexture, self._tilemapLightMaskShader, programsStarted, self._worldPrewarmReadbackMilliseconds
    )
    programsStarted, self._worldPrewarmReadbackMilliseconds = prewarmShader(
        target, sourceTexture, self._lightMaskShader, programsStarted, self._worldPrewarmReadbackMilliseconds
    )
    programsStarted, self._worldPrewarmReadbackMilliseconds = prewarmShader(
        target, sourceTexture, self._lightPassShader, programsStarted, self._worldPrewarmReadbackMilliseconds
    )
    programsStarted, self._worldPrewarmReadbackMilliseconds = prewarmShader(
        target, sourceTexture, self._unobstructedLightPassShader, programsStarted,
        self._worldPrewarmReadbackMilliseconds
    )
    self._worldShadersPrewarmed = true
    return programsStarted
end

---@param visibleRect Global.WorldGeometry.CellRect
---@param _drain      boolean
---@return boolean
---@param self        WorldGameMapImplState
function WorldGameMapRendering.PrewarmWorldViewport(self, visibleRect, _drain)
    if PLATFORM == "ohos" then
        return true
    end
    local started = perfCounter()
    local target = self:_getWorldShaderPrewarmTarget()
    local prewarmed = self:_prewarmWorldShaderPrograms(target)
    local sourceTexture = assert(self._camera):getTexture()
    for _, region in ipairs(self._worldRegions) do
        if region.payload ~= nil and WorldGeometry.RectIntersects(region, visibleRect) then
            local payload = region.payload
            for _, layerName in ipairs(self._worldLayerOrder) do
                local layer = payload.tilemap:getLayer(layerName)
                if layer ~= nil and layer.visible then
                    local shader = layer:getShader()
                    if shader ~= nil
                        and (payload.prewarmedLayerShaders == nil or not payload.prewarmedLayerShaders[layerName]) then
                        prewarmed, self._worldPrewarmReadbackMilliseconds = prewarmShader(
                            target, sourceTexture, shader, prewarmed, self._worldPrewarmReadbackMilliseconds
                        )
                        payload.prewarmedLayerShaders = payload.prewarmedLayerShaders or {}
                        payload.prewarmedLayerShaders[layerName] = true
                    end
                end
            end
        end
    end
    if prewarmed then
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
        if region.lightingRevision ~= builder.lightingRevision then
            region.lightingRevision = builder.lightingRevision
            self:_refreshWorldLights()
        end
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
    self:_refreshWorldLights()
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

---@param self WorldGameMapImplState
function WorldGameMapRendering.DrawMapContent(self, target, states, _applyPlayerCover)
    states = states or sf.RenderStates.new()
    local visibleRect = self:_getVisibleCellRect()
    for _, layerName in ipairs(self._worldLayerOrder) do
        for _, region in ipairs(self._worldRegions) do
            if region.payload ~= nil and WorldGeometry.RectIntersects(region, visibleRect) then
                local layer = region.payload.tilemap:getLayer(layerName)
                if layer ~= nil and layer.visible then
                    local regionStates = sf.RenderStates.new(states.blendMode)
                    regionStates.transform = states.transform:copy()
                    regionStates.texture = states.texture
                    regionStates.shader = layer.shader
                    regionStates.transform:translate(
                        sf.Vector2f.new(region.x * Engine.GetCellSize(), region.y * Engine.GetCellSize())
                    )
                    target:draw(layer, regionStates)
                end
            end
        end
        for _, actor in ipairs(self._actors[layerName] or {}) do
            if actor:isVisibleInHierarchy() and self:_isWorldActorLayerVisible(actor, layerName, visibleRect)
                and self._actorPixelShatterByActor[actor] == nil then
                actor:drawEmitter(target, states, true)
                self:_drawActor(target, states, actor, 255)
                actor:drawEmitter(target, states, false)
            end
        end
        self:_drawActorPixelShatterEffects(target, layerName)
    end
end

return WorldGameMapRendering
