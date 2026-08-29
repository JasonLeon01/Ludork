local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local WorldGeometry = require("Global.WorldGeometry")
local RenderSupport = require("Global.WorldGameMap.RenderSupport")

---@diagnostic disable: need-check-nil, param-type-mismatch, duplicate-set-field

local FogController = GlobalCore.FogController
local WORLD_SHADER_PREWARM_SIZE = sf.Vector2u.new(1, 1)
---@cast WORLD_SHADER_PREWARM_SIZE sf.Vector2u

---@type WorldGameMapImplState
local WorldGameMapRendering = {}

function WorldGameMapRendering:drawMapFogOverlay()
    local camera = self._camera
    ---@cast camera GlobalCore.Camera
    FogController.drawWorldOverlay(camera)
end

function WorldGameMapRendering:_ensureWorldLightingTargets()
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
function WorldGameMapRendering:_drawWorldTileMaskLayer(
    target, baseStates, layerName, layer, region, viewPosition, viewSize, viewRotation
)
    self:_setTileMaskUniforms(
        RenderSupport.TileMaskCacheKey(region, layerName), layer,
        RenderSupport.CreateTileMaskConfig(target, viewPosition, viewSize, viewRotation, region)
    )
    local regionStates = sf.RenderStates.new(baseStates.blendMode)
    regionStates.transform = copy(baseStates.transform)
    regionStates.texture = baseStates.texture
    regionStates.shader = self._tilemapLightMaskShader
    regionStates.transform:translate(sf.Vector2f.new(region.x * Engine.CellSize, region.y * Engine.CellSize))
    target:draw(layer, regionStates)
end

---@param region Source.SceneComponents.WorldRegionData
function WorldGameMapRendering:_releaseWorldRegionTileMaskCache(region)
    for _, layerName in ipairs(self._worldConfig.layerOrder) do
        self._layerMaskTextureCache[RenderSupport.TileMaskCacheKey(region, layerName)] = nil
    end
end

---@param activeLights  Global.GameMap.ActiveLight[]
---@param _staticActors Engine.Actor[]
function WorldGameMapRendering:_rebuildStaticTransmission(activeLights, _staticActors)
    self:_ensureWorldLightingTargets()
    ---@cast self._staticTransmission sf.RenderTexture
    ---@cast self._transmissionTileRenderStates sf.RenderStates
    local lightingRect = RenderSupport.GetLightingCellRect(self, activeLights)
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
    local viewPosition = sf.Vector2f.new(lightingRect.x * Engine.CellSize, lightingRect.y * Engine.CellSize)
    local viewSize = sf.Vector2f.new(lightingRect.width * Engine.CellSize, lightingRect.height * Engine.CellSize)
    local viewCentre = sf.Vector2f.new(viewPosition.x + viewSize.x * 0.5, viewPosition.y + viewSize.y * 0.5)
    ---@cast viewPosition sf.Vector2f
    ---@cast viewSize sf.Vector2f
    ---@cast viewCentre sf.Vector2f
    self._staticTextureOrigin = copy(viewPosition)
    self._staticTextureSize = copy(viewSize)
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
function WorldGameMapRendering:_renderSurfaceMask()
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
            if not actor:isDestroyed() and self:_isWorldActorLayerVisible(actor, layerName) then
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
function WorldGameMapRendering:_getWorldShaderPrewarmTarget()
    if self._worldShaderPrewarmTarget == nil then
        self._worldShaderPrewarmTarget = sf.RenderTexture.new(WORLD_SHADER_PREWARM_SIZE)
        self._worldShaderPrewarmTarget:setSmooth(false)
    end
    return self._worldShaderPrewarmTarget
end

---@param world         Global.WorldGameMap.WorldGameMap
---@param target        sf.RenderTexture
---@param sourceTexture sf.Texture
---@param shader        sf.Shader | nil
---@return boolean
local function prewarmShader(world, target, sourceTexture, shader)
    if shader == nil then
        return false
    end
    local sprite = sf.Sprite.new(sourceTexture)
    local states = sf.RenderStates.new()
    states.shader = shader
    target:clear(sf.Color.Transparent)
    target:draw(sprite, states)
    target:display()
    local readbackStarted = perfCounter()
    target:getTexture():copyToImage()
    world._worldPrewarmReadbackMilliseconds = world._worldPrewarmReadbackMilliseconds
        + (perfCounter() - readbackStarted) * 1000.0
    return true
end

---@param target sf.RenderTexture
---@return boolean
function WorldGameMapRendering:_prewarmWorldShaderPrograms(target)
    if self._worldShadersPrewarmed then
        return false
    end
    self:_ensureWorldLightingTargets()
    self:_ensureDirectLight()
    self:refreshShader()
    local sourceTexture = assert(self._camera):getTexture()
    local programsStarted = prewarmShader(self, target, sourceTexture, self._materialShader)
    programsStarted = prewarmShader(self, target, sourceTexture, self._tilemapLightMaskShader) or programsStarted
    programsStarted = prewarmShader(self, target, sourceTexture, self._lightMaskShader) or programsStarted
    programsStarted = prewarmShader(self, target, sourceTexture, self._lightPassShader) or programsStarted
    programsStarted = prewarmShader(self, target, sourceTexture, self._unobstructedLightPassShader) or programsStarted
    self._worldShadersPrewarmed = true
    return programsStarted
end

---@param visibleRect Global.WorldGeometry.CellRect
---@param _drain      boolean
---@return boolean
function WorldGameMapRendering:_prewarmWorldViewport(visibleRect, _drain)
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
                        and (payload.prewarmedLayerShaders == nil or payload.prewarmedLayerShaders[layerName] ~= shader) then
                        prewarmed = prewarmShader(self, target, sourceTexture, shader) or prewarmed
                        payload.prewarmedLayerShaders = payload.prewarmedLayerShaders or {}
                        payload.prewarmedLayerShaders[layerName] = shader
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
function WorldGameMapRendering:_isWorldViewportReady(visibleRect)
    for _, region in ipairs(self._worldRegions) do
        if WorldGeometry.RectIntersects(region, visibleRect) then
            if region.payload == nil or region.state ~= "Active" or region.publishState ~= nil then
                return false
            end
            if region.backgroundBuilder ~= nil and not region.backgroundBuilder.isRectReady(visibleRect) then
                return false
            end
        end
    end
    return true
end

---@param world    Global.WorldGameMap.WorldGameMap
---@param position sf.Vector2f | nil
---@return boolean
local function prepareCameraPosition(world, position)
    if position ~= nil then
        assert(world._camera):setViewPosition(position)
    end
    local visibleRect = world:_getVisibleCellRect()
    return world:_isWorldViewportReady(visibleRect) and world:_prewarmWorldViewport(visibleRect, false)
end

---@return boolean
function WorldGameMapRendering:_prepareCameraFrame()
    if self._camera == nil then
        return true
    end
    self._camera:syncFollowTarget()
    local desiredPosition = self._camera:getViewPosition()
    if prepareCameraPosition(self, desiredPosition) then
        self._worldLastReadyCameraPosition = desiredPosition ~= nil and copy(desiredPosition) or nil
        return true
    end
    if self._worldStreamingCameraPosition ~= nil and prepareCameraPosition(self, self._worldStreamingCameraPosition) then
        self._worldLastReadyCameraPosition = copy(self._worldStreamingCameraPosition)
        return true
    end
    if self._worldLastReadyCameraPosition ~= nil then
        if prepareCameraPosition(self, self._worldLastReadyCameraPosition) then
            return true
        end
    end
    return false
end

---@param world        Global.WorldGameMap.WorldGameMap
---@param region       Source.SceneComponents.WorldRegionData
---@param builder      Global.WorldGameMap.RegionBuildState
---@param requiredRect Global.WorldGeometry.CellRect
local function prepareRegionRect(world, region, builder, requiredRect)
    local started = perfCounter()
    while not builder.isRectReady(requiredRect) do
        world:_pumpRegionBackgroundActors(region, builder, math.huge)
        local prepareStarted = perfCounter()
        builder.prepareRect(requiredRect, math.huge)
        local prepareMilliseconds = (perfCounter() - prepareStarted) * 1000.0
        if prepareMilliseconds > world._worldPublishSlowStageMilliseconds then
            world._worldPublishSlowStage = "prepareVisibleTileChunk"
            world._worldPublishSlowStageMilliseconds = prepareMilliseconds
        end
        world:_pumpRegionBackgroundActors(region, builder, math.huge)
        region.geometryRevision = builder.geometryRevision
        if region.lightingRevision ~= builder.lightingRevision then
            region.lightingRevision = builder.lightingRevision
            world:_refreshWorldLights()
        end
    end
    if builder.completed and builder.actorPublishQueue == nil and not bool(builder.readyActorRoots) then
        region.backgroundBuilder = nil
    end
    local elapsedMilliseconds = (perfCounter() - started) * 1000.0
    world._worldPublishMilliseconds = world._worldPublishMilliseconds + elapsedMilliseconds
end

---@param position sf.Vector2i
function WorldGameMapRendering:prepareViewportAt(position)
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
        if region.demand == "Active" then
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
            prepareRegionRect(self, region, region.backgroundBuilder, activeRect)
        end
    end
    self._worldActivationDeferred = false
    for _, region in ipairs(initialActiveRegions) do
        if region.state == "Active" then
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
            region.payload ~= nil and region.state == "Active",
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

function WorldGameMapRendering:drawMapContent(target, states, _applyPlayerCover)
    states = states or sf.RenderStates.new()
    local visibleRect = self:_getVisibleCellRect()
    for _, layerName in ipairs(self._worldLayerOrder) do
        for _, region in ipairs(self._worldRegions) do
            if region.payload ~= nil and WorldGeometry.RectIntersects(region, visibleRect) then
                local layer = region.payload.tilemap:getLayer(layerName)
                if layer ~= nil and layer.visible then
                    local regionStates = sf.RenderStates.new(states.blendMode)
                    regionStates.transform = copy(states.transform)
                    regionStates.texture = states.texture
                    regionStates.shader = layer.shader
                    regionStates.transform:translate(
                        sf.Vector2f.new(region.x * Engine.CellSize, region.y * Engine.CellSize)
                    )
                    target:draw(layer, regionStates)
                end
            end
        end
        for _, actor in ipairs(self._actors[layerName] or {}) do
            if self:_isWorldActorLayerVisible(actor, layerName) and self._actorPixelShatterByActor[actor] == nil then
                self:_drawActor(target, states, actor, 255)
            end
        end
        self:_drawActorPixelShatterEffects(target, layerName)
    end
end

return WorldGameMapRendering
