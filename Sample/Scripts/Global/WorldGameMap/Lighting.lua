local Engine = require("Engine")
local GlobalCore = require("GlobalCore")

---@diagnostic disable: need-check-nil, param-type-mismatch, duplicate-set-field

local ShaderManager = GlobalCore.ShaderManager

---@type WorldGameMapImplState
local GameMapLighting = {}

function GameMapLighting:_initialiseWorldRendering()
    local materialShader = nil
    local tilemapLightMaskShader = nil
    local lightMaskShader = nil
    local lightPassShader = nil
    local unobstructedLightPassShader = nil
    local actorHueShader = nil
    if sf.Shader.isAvailable() then
        tilemapLightMaskShader = ShaderManager.load(
            "/Game/Assets/Shaders/Global/TilemapLightMask.frag",
            sf.Shader.Type.Fragment
        )
        lightMaskShader = ShaderManager.load("/Game/Assets/Shaders/Global/LightMask.frag", sf.Shader.Type.Fragment)
        materialShader = ShaderManager.load("/Game/Assets/Shaders/Global/WorldMaterial.frag", sf.Shader.Type.Fragment)
        lightPassShader = ShaderManager.load("/Game/Assets/Shaders/Global/LightPass.frag", sf.Shader.Type.Fragment)
        unobstructedLightPassShader = ShaderManager.load(
            "/Game/Assets/Shaders/Global/UnoccludedLightPass.frag",
            sf.Shader.Type.Fragment
        )
        actorHueShader = ShaderManager.load("/Game/Assets/Shaders/Global/Hue.frag", sf.Shader.Type.Fragment)
    end
    self._tilemapLightMaskShader = tilemapLightMaskShader
    self._lightMaskShader = lightMaskShader
    self._lightPassShader = lightPassShader
    self._unobstructedLightPassShader = unobstructedLightPassShader
    self._materialShader = materialShader
    self._actorHueShader = actorHueShader
    self._lightBlockSize = sf.Vector2f.new(Engine.CellSize, Engine.CellSize)
    local tilemapSize = self:getSize()
    self._shaderMapSize = sf.Vector2f.new(tilemapSize.x, tilemapSize.y)
    self._playerCoverColour = sf.Color.new(255, 255, 255, self.DefaultCoverAlpha)
    self._staticTransmission = nil
    self._staticOccupancy = nil
    self._surfaceMask = nil
    self._dynamicTransmission = nil
    self._directLight = nil
    self._directLightCleared = false
    self._staticDirectLight = nil
    self._useStaticDirectLight = false
    self._lightPassQuad = nil
    self._unobstructedLightVertices = nil
    self._unobstructedLightVertex = nil
    self._unobstructedLightCache = nil
    self._cachedActiveLights = nil
    self._cachedLightMaterialRevision = -1
    self._cachedLightTransmissionSignature = nil
    self._staticTransmissionRevision = -1
    self._staticTransmissionSignature = nil
    self._staticTransmissionActorCache = nil
    self._staticTransmissionGeneration = 0
    self._surfaceMaskRevision = -1
    self._surfaceMaskSignature = nil
    self._surfaceMaskActorCache = nil
    self._renderedLightingLights = nil
    self._renderedLightingOwners = nil
    self._renderedLightingActors = nil
    self._renderedLightingStaticGeneration = -1
    self._renderedLightingView = nil
    self._renderedLightingTargetSize = nil
    self._staticLightCaches = {}
    self._staticTextureOrigin = sf.Vector2f.new(0.0, 0.0)
    self._staticTextureSize = sf.Vector2f.new(1.0, 1.0)
    self._staticOccupancyOrigin = sf.Vector2f.new(0.0, 0.0)
    self._staticOccupancySize = sf.Vector2f.new(1.0, 1.0)
    self._dynamicTransmissionPixelSize = 0
    self._zeroShaderOffset = sf.Vector2f.new(0.0, 0.0)
    self._identityShaderRotation = sf.Vector2f.new(0.0, 1.0)
    self._shaderViewSinCos = sf.Vector2f.new(0.0, 1.0)
    self._shaderColour = sf.Vector3f.new(0.0, 0.0, 0.0)
    self._actorShaderBuffer = nil
    self._actorHueBuffer = nil
    self._actorHueSourceSprite = nil
    self._layerMaskTextureCache = {}
    self._transparentTiles = {}
    self._coverLayerStates = nil
    self._coverPlayerX = nil
    self._coverPlayerY = nil
    self._coverPlayerLayerIndex = nil
    self._coverAlpha = nil
    self._coverMaterialRevision = nil
    self._surfaceTileRenderStates = nil
    self._surfaceActorRenderStates = nil
    self._transmissionTileRenderStates = nil
    self._transmissionActorRenderStates = nil
    self._lightPassRenderStates = nil
    self._unobstructedLightPassRenderStates = nil
    ---@cast self._camera GlobalCore.Camera
    local maskPixelSize = assert(self._camera:getRenderTexture()):getSize()
    self._staticTransmission = sf.RenderTexture.new(maskPixelSize)
    self._staticTransmission:setSmooth(false)
    self._surfaceMask = sf.RenderTexture.new(maskPixelSize)
    self._surfaceMask:setSmooth(false)
    self._surfaceTileRenderStates = sf.RenderStates.new(self._camera:getRenderStates().blendMode)
    self._surfaceTileRenderStates.shader = self._tilemapLightMaskShader
    self._surfaceActorRenderStates = sf.RenderStates.new(self._camera:getRenderStates().blendMode)
    self._surfaceActorRenderStates.shader = self._lightMaskShader
    self._transmissionTileRenderStates = sf.RenderStates.new(sf.BlendMultiply)
    self._transmissionTileRenderStates.shader = self._tilemapLightMaskShader
    self._transmissionActorRenderStates = sf.RenderStates.new(sf.BlendMultiply)
    self._transmissionActorRenderStates.shader = self._lightMaskShader
    self._lightPassRenderStates = sf.RenderStates.new(sf.BlendAdd)
    self._lightPassRenderStates.shader = self._lightPassShader
    self._lightPassQuad = sf.RectangleShape.new()
    self._lightPassQuad:setFillColor(sf.Color.White)
    self._unobstructedLightPassRenderStates = sf.RenderStates.new(sf.BlendAdd)
    self._unobstructedLightPassRenderStates.shader = self._unobstructedLightPassShader
    self._unobstructedLightVertices = sf.VertexArray.new(sf.PrimitiveType.Triangles)
    self._unobstructedLightVertex = sf.Vertex.new()
end

function GameMapLighting:_getMaterialShader()
    return self._materialShader
end

---@param cached table
---@param actor  Engine.Actor
---@return boolean
local function actorLightingStateMatches(cached, actor)
    local position = actor:getPosition()
    local translation = actor:getTranslation()
    local scale = actor:getScale()
    local origin = actor:getOrigin()
    local textureRect = actor:getTextureRect()
    local colour = actor:getColor()
    local texture = actor:getSpriteTexture()
    local textureHandle = texture ~= nil and texture:getNativeHandle() or 0
    return cached.actor == actor and cached.textureHandle == textureHandle and cached.positionX == position.x
        and cached.positionY == position.y and cached.translationX == translation.x
        and cached.translationY == translation.y and cached.rotation == actor:getRotation():asDegrees()
        and cached.scaleX == scale.x and cached.scaleY == scale.y and cached.originX == origin.x
        and cached.originY == origin.y and cached.textureX == textureRect.position.x
        and cached.textureY == textureRect.position.y and cached.textureWidth == textureRect.size.x
        and cached.textureHeight == textureRect.size.y and cached.colourR == colour.r and cached.colourG == colour.g
        and cached.colourB == colour.b and cached.colourA == colour.a and cached.lightBlock == actor:getLightBlock()
        and cached.mirror == actor:getMirror() and cached.reflectionStrength == actor:getReflectionStrength()
        and cached.ignoreLighting == actor:getIgnoreLighting()
end

---@param actor Engine.Actor
---@return table
local function captureActorLightingState(actor)
    local position = actor:getPosition()
    local translation = actor:getTranslation()
    local scale = actor:getScale()
    local origin = actor:getOrigin()
    local textureRect = actor:getTextureRect()
    local colour = actor:getColor()
    local texture = actor:getSpriteTexture()
    return {
        actor = actor,
        textureHandle = texture ~= nil and texture:getNativeHandle() or 0,
        positionX = position.x,
        positionY = position.y,
        translationX = translation.x,
        translationY = translation.y,
        rotation = actor:getRotation():asDegrees(),
        scaleX = scale.x,
        scaleY = scale.y,
        originX = origin.x,
        originY = origin.y,
        textureX = textureRect.position.x,
        textureY = textureRect.position.y,
        textureWidth = textureRect.size.x,
        textureHeight = textureRect.size.y,
        colourR = colour.r,
        colourG = colour.g,
        colourB = colour.b,
        colourA = colour.a,
        lightBlock = actor:getLightBlock(),
        mirror = actor:getMirror(),
        reflectionStrength = actor:getReflectionStrength(),
        ignoreLighting = actor:getIgnoreLighting()
    }
end

function GameMapLighting:refreshShader()
    ---@diagnostic disable-next-line: unnecessary-if
    if self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    if self._camera == nil or self._materialShader == nil or self._surfaceMask == nil or self._directLight == nil then
        return
    end
    local screenSize = self._camera:getViewSize()
    ---@cast screenSize sf.Vector2f
    self._materialShader:setUniform("surfaceMask", self._surfaceMask:getTexture())
    self._materialShader:setUniform("directLight", self._directLight:getTexture())
    if self._staticDirectLight ~= nil then
        self._materialShader:setUniform("staticDirectLight", self._staticDirectLight:getTexture())
    end
    self._materialShader:setUniform("useStaticDirectLight", self._useStaticDirectLight and 1.0 or 0.0)
    self:_setViewShaderUniforms(self._materialShader, screenSize, self._zeroShaderOffset, false)
    self._materialShader:setUniform("ambientColor", self:_toShaderColour(self._ambientLight, true))
end

---@return boolean
function GameMapLighting:_lightingShadersAvailable()
    return self._camera ~= nil and self._materialShader ~= nil and self._tilemapLightMaskShader ~= nil
        and self._lightMaskShader ~= nil and self._lightPassShader ~= nil and self._unobstructedLightPassShader ~= nil
        and self._staticTransmission ~= nil and self._surfaceMask ~= nil
end

---@param visibleActors Engine.Actor[]
---@return Engine.Actor[], Engine.Actor[]
function GameMapLighting:_partitionLightBlockingActors(visibleActors)
    if self:isWorldMap() then
        return visibleActors, {}
    end
    local dynamicActors = {}
    local staticActors = {}
    for _, actor in ipairs(visibleActors) do
        if actor:getLightBlock() > 0.0 then
            if actor.lightComp ~= nil or actor:getAnimatable() or actor:isMoving() then
                dynamicActors[#dynamicActors + 1] = actor
            else
                staticActors[#staticActors + 1] = actor
            end
        end
    end
    return dynamicActors, staticActors
end

---@param actors Engine.Actor[]
---@return boolean
function GameMapLighting:_staticTransmissionActorsMatch(actors)
    if self._staticTransmissionActorCache == nil or #self._staticTransmissionActorCache ~= #actors then
        return false
    end
    for index, actor in ipairs(actors) do
        local cached = assert(self._staticTransmissionActorCache[index])
        local position = actor:getPosition()
        local translation = actor:getTranslation()
        local scale = actor:getScale()
        local origin = actor:getOrigin()
        local textureRect = actor:getTextureRect()
        local colour = actor:getColor()
        local texture = actor:getSpriteTexture()
        local textureHandle = texture ~= nil and texture:getNativeHandle() or 0
        if cached.actor ~= actor or cached.textureHandle ~= textureHandle or cached.positionX ~= position.x
            or cached.positionY ~= position.y or cached.translationX ~= translation.x
            or cached.translationY ~= translation.y or cached.rotation ~= actor:getRotation():asDegrees()
            or cached.scaleX ~= scale.x or cached.scaleY ~= scale.y or cached.originX ~= origin.x
            or cached.originY ~= origin.y or cached.textureX ~= textureRect.position.x
            or cached.textureY ~= textureRect.position.y or cached.textureWidth ~= textureRect.size.x
            or cached.textureHeight ~= textureRect.size.y or cached.colourA ~= colour.a
            or cached.lightBlock ~= actor:getLightBlock() then
            return false
        end
    end
    return true
end

---@param actors Engine.Actor[]
function GameMapLighting:_cacheStaticTransmissionActors(actors)
    local cache = {}
    for index, actor in ipairs(actors) do
        local position = actor:getPosition()
        local translation = actor:getTranslation()
        local scale = actor:getScale()
        local origin = actor:getOrigin()
        local textureRect = actor:getTextureRect()
        local colour = actor:getColor()
        local texture = actor:getSpriteTexture()
        cache[index] = {
            actor = actor,
            textureHandle = texture ~= nil and texture:getNativeHandle() or 0,
            positionX = position.x,
            positionY = position.y,
            translationX = translation.x,
            translationY = translation.y,
            rotation = actor:getRotation():asDegrees(),
            scaleX = scale.x,
            scaleY = scale.y,
            originX = origin.x,
            originY = origin.y,
            textureX = textureRect.position.x,
            textureY = textureRect.position.y,
            textureWidth = textureRect.size.x,
            textureHeight = textureRect.size.y,
            colourA = colour.a,
            lightBlock = actor:getLightBlock()
        }
    end
    self._staticTransmissionActorCache = cache
end

---@param actors Engine.Actor[]
---@return boolean
function GameMapLighting:_surfaceMaskActorsMatch(actors)
    if self._surfaceMaskActorCache == nil or #self._surfaceMaskActorCache ~= #actors then
        return false
    end
    for index, actor in ipairs(actors) do
        if not actorLightingStateMatches(assert(self._surfaceMaskActorCache[index]), actor) then
            return false
        end
    end
    return true
end

---@param actors Engine.Actor[]
function GameMapLighting:_cacheSurfaceMaskActors(actors)
    local cache = {}
    for index, actor in ipairs(actors) do
        cache[index] = captureActorLightingState(actor)
    end
    self._surfaceMaskActorCache = cache
end

---@param activeLights     Global.GameMap.ActiveLight[]
---@param dynamicOccluders Engine.Actor[]
---@return boolean
function GameMapLighting:_renderedLightingMatches(activeLights, dynamicOccluders)
    if self._directLight == nil or self._renderedLightingStaticGeneration ~= self._staticTransmissionGeneration
        or self._renderedLightingView == nil or self._renderedLightingTargetSize == nil
        or not self:_lightsMatchCache(activeLights, self._renderedLightingLights) or self._renderedLightingOwners == nil
        or #self._renderedLightingOwners ~= #activeLights or self._renderedLightingActors == nil
        or #self._renderedLightingActors ~= #dynamicOccluders then
        return false
    end
    for index, entry in ipairs(activeLights) do
        if self._renderedLightingOwners[index] ~= (entry.owner or false) then
            return false
        end
    end
    for index, actor in ipairs(dynamicOccluders) do
        if not actorLightingStateMatches(assert(self._renderedLightingActors[index]), actor) then
            return false
        end
    end
    ---@cast self._camera GlobalCore.Camera
    local viewPosition = assert(self._camera:getViewPosition())
    local viewSize = assert(self._camera:getViewSize())
    local targetSize = self._directLight:getSize()
    return self._renderedLightingView[1] == viewPosition.x and self._renderedLightingView[2] == viewPosition.y
        and self._renderedLightingView[3] == viewSize.x and self._renderedLightingView[4] == viewSize.y
        and self._renderedLightingView[5] == self._camera:getViewRotation():asDegrees()
        and self._renderedLightingTargetSize[1] == targetSize.x and self._renderedLightingTargetSize[2] == targetSize.y
end

---@param activeLights     Global.GameMap.ActiveLight[]
---@param dynamicOccluders Engine.Actor[]
function GameMapLighting:_cacheRenderedLighting(activeLights, dynamicOccluders)
    self._renderedLightingLights = self:_cacheLightList(activeLights)
    self._renderedLightingOwners = {}
    for index, entry in ipairs(activeLights) do
        self._renderedLightingOwners[index] = entry.owner or false
    end
    self._renderedLightingActors = {}
    for index, actor in ipairs(dynamicOccluders) do
        self._renderedLightingActors[index] = captureActorLightingState(actor)
    end
    ---@cast self._camera GlobalCore.Camera
    ---@cast self._directLight sf.RenderTexture
    local viewPosition = assert(self._camera:getViewPosition())
    local viewSize = assert(self._camera:getViewSize())
    local targetSize = self._directLight:getSize()
    self._renderedLightingView = {
        viewPosition.x, viewPosition.y, viewSize.x, viewSize.y, self._camera:getViewRotation():asDegrees()
    }
    self._renderedLightingTargetSize = { targetSize.x, targetSize.y }
    self._renderedLightingStaticGeneration = self._staticTransmissionGeneration
end

---@param activeLights Global.GameMap.ActiveLight[]
---@param analyses     GlobalCore.LightOcclusionResult[]
function GameMapLighting:_renderDynamicLighting(activeLights, analyses)
    ---@cast self._directLight sf.RenderTexture
    local unobstructedLights = {}
    local staticLightTextures = {}
    self:_ensureDynamicTransmission(activeLights)
    for index, entry in ipairs(activeLights) do
        if assert(analyses[index]).hasStaticTransmissionLoss then
            staticLightTextures[index] = self:_ensureStaticLightCache(index, entry)
        end
    end
    local commonUniformsSet = false
    for index, entry in ipairs(activeLights) do
        local analysis = assert(analyses[index])
        local dynamicOrigin, dynamicSize, hasDynamicTransmissionLoss = self:_renderDynamicTransmission(analysis)
        local staticLightTexture = staticLightTextures[index]
        if hasDynamicTransmissionLoss or staticLightTexture ~= nil then
            if not commonUniformsSet then
                self:_setLightPassCommonUniforms()
                commonUniformsSet = true
            end
            if staticLightTexture ~= nil then
                self._lightPassShader:setUniform("cachedStaticLight", staticLightTexture)
                self._lightPassShader:setUniform("useCachedStaticLight", 1.0)
            else
                self._lightPassShader:setUniform("useCachedStaticLight", 0.0)
            end
            self:_renderLight(entry, dynamicOrigin, dynamicSize, false, hasDynamicTransmissionLoss, self._directLight)
        else
            unobstructedLights[#unobstructedLights + 1] = entry
        end
    end
    self._lightPassShader:setUniform("useCachedStaticLight", 0.0)
    self:_renderUnobstructedLights(unobstructedLights, self._directLight)
end

---@param activeLights Global.GameMap.ActiveLight[]
---@param analyses     GlobalCore.LightOcclusionResult[]
function GameMapLighting:_renderCachedLighting(activeLights, analyses)
    self:_ensureStaticDirectLight()
    ---@cast self._staticDirectLight sf.RenderTexture
    local cacheValid = self._cachedLightMaterialRevision == self._materialRevision and self._cachedLightTransmissionSignature
            == self._staticTransmissionSignature and self:_lightsMatchCache(activeLights, self._cachedActiveLights)
    if cacheValid then
        return
    end
    local unobstructedLights = {}
    local staticLights = {}
    for index, entry in ipairs(activeLights) do
        if assert(analyses[index]).hasStaticTransmissionLoss then
            staticLights[#staticLights + 1] = entry
        else
            unobstructedLights[#unobstructedLights + 1] = entry
        end
    end
    local tilemapSize = self._tilemap:getSize()
    local worldSize = sf.Vector2f.new(tilemapSize.x * Engine.CellSize, tilemapSize.y * Engine.CellSize)
    local worldCentre = sf.Vector2f.new(worldSize.x * 0.5, worldSize.y * 0.5)
    self._staticDirectLight:setView(sf.View.new(worldCentre, worldSize))
    self._staticDirectLight:clear(sf.Color.Black)
    if bool(staticLights) then
        self:_setLightPassWorldUniforms()
        self:_renderStaticLights(staticLights, self._staticDirectLight)
    end
    self:_renderUnobstructedLights(unobstructedLights, self._staticDirectLight)
    self._staticDirectLight:display()
    self._cachedActiveLights = self:_cacheLightList(activeLights)
    self._cachedLightMaterialRevision = self._materialRevision
    self._cachedLightTransmissionSignature = self._staticTransmissionSignature
end

---@param _activeLights? Global.GameMap.ActiveLight[]
---@param staticActors   Engine.Actor[]
function GameMapLighting:_rebuildStaticTransmission(_activeLights, staticActors)
    local transmissionSignature = self:_getStaticTransmissionSignature()
    if self._staticTransmissionRevision == self._materialRevision and self._staticTransmissionSignature
            == transmissionSignature and self:_staticTransmissionActorsMatch(staticActors) then
        return
    end
    ---@cast self._staticTransmission sf.RenderTexture
    ---@cast self._transmissionTileRenderStates sf.RenderStates
    ---@cast self._transmissionActorRenderStates sf.RenderStates
    self._staticTransmission:clear(sf.Color.White)
    self._tilemapLightMaskShader:setUniform("transmissionMode", 1.0)
    local size = self._tilemap:getSize()
    for _, layerName in ipairs(self._layerNames) do
        local layer = self._tilemap:getLayer(layerName)
        ---@cast layer Engine.TileLayer
        if layer.visible then
            self:_setTileMaskUniforms(layerName, layer)
            local drawable = layer
            ---@cast drawable sf.Drawable
            self._staticTransmission:draw(drawable, self._transmissionTileRenderStates)
        end
    end
    self._lightMaskShader:setUniform("transmissionMode", 1.0)
    for _, actor in ipairs(staticActors) do
        self:_setActorMaskUniforms(actor)
        self._staticTransmission:draw(actor, self._transmissionActorRenderStates)
    end
    self._staticTransmission:display()
    local origin = sf.Vector2i.new(0, 0)
    ---@cast origin sf.Vector2i
    self._staticOccupancy = assert(self:rebuildStaticLightOccupancy(origin, size, staticActors))
    self:_cacheStaticTransmissionActors(staticActors)
    self._staticTransmissionRevision = self._materialRevision
    self._staticTransmissionSignature = transmissionSignature
    self._staticTransmissionGeneration = self._staticTransmissionGeneration + 1
    self._cachedLightMaterialRevision = -1
end

---@return Global.GameMap.StaticTransmissionSignature
function GameMapLighting:_getStaticTransmissionSignature()
    ---@type Global.GameMap.StaticTransmissionElement[]
    local states = {}
    for _, layerName in ipairs(self._layerNames) do
        local layer = self._tilemap:getLayer(layerName)
        ---@cast layer Engine.TileLayer
        states[#states + 1] = bool(layer.visible)
    end
    if self._coverPlayerX == nil or self._coverPlayerY == nil
        or self._coverPlayerLayerIndex == nil or self._coverAlpha == nil then
        states[#states + 1] = tuple("none")
    else
        local coverPlayerX = self._coverPlayerX
        local coverPlayerY = self._coverPlayerY
        local coverPlayerLayerIndex = self._coverPlayerLayerIndex
        local coverAlpha = self._coverAlpha
        ---@type (string | integer)[]
        local coverState = { "cover", coverPlayerX, coverPlayerY, coverPlayerLayerIndex, coverAlpha }
        local coverTuple = tuple(table.unpack(coverState))
        ---@diagnostic disable-next-line: cast-type-mismatch, table.unpack cannot express that this contiguous array has no nil slots
        ---@cast coverTuple tuple<string | integer>
        states[#states + 1] = coverTuple
    end
    local signature = tuple(table.unpack(states))
    ---@diagnostic disable-next-line: cast-type-mismatch, table.unpack cannot express that this contiguous array has no nil slots
    ---@cast signature Global.GameMap.StaticTransmissionSignature
    return signature
end

---@return Engine.Actor[]
function GameMapLighting:_renderSurfaceMask()
    local visibleActors = {}
    local visibleActorSet = {}
    for _, layerName in ipairs(self._layerNames) do
        local layer = self._tilemap:getLayer(layerName)
        ---@cast layer Engine.TileLayer
        if layer.visible then
            for _, actor in ipairs(self._actors[layerName] or {}) do
                if not actor:isDestroyed() and not visibleActorSet[actor] then
                    visibleActorSet[actor] = true
                    visibleActors[#visibleActors + 1] = actor
                end
            end
        end
    end
    local signature = self:_getStaticTransmissionSignature()
    if self._surfaceMaskRevision == self._materialRevision and self._surfaceMaskSignature == signature
        and self:_surfaceMaskActorsMatch(visibleActors) then
        return visibleActors
    end
    ---@cast self._surfaceMask sf.RenderTexture
    ---@cast self._surfaceTileRenderStates sf.RenderStates
    ---@cast self._surfaceActorRenderStates sf.RenderStates
    self._surfaceMask:clear(sf.Color.Transparent)
    self._tilemapLightMaskShader:setUniform("transmissionMode", 0.0)
    self._lightMaskShader:setUniform("transmissionMode", 0.0)
    for _, layerName in ipairs(self._layerNames) do
        local layer = self._tilemap:getLayer(layerName)
        ---@cast layer Engine.TileLayer
        if layer.visible then
            self:_setTileMaskUniforms(layerName, layer)
            local drawable = layer
            ---@cast drawable sf.Drawable
            self._surfaceMask:draw(drawable, self._surfaceTileRenderStates)
            for _, actor in ipairs(self._actors[layerName] or {}) do
                if not actor:isDestroyed() then
                    self:_setActorMaskUniforms(actor)
                    self._surfaceMask:draw(actor, self._surfaceActorRenderStates)
                end
            end
        end
    end
    self._surfaceMask:display()
    self:_cacheSurfaceMaskActors(visibleActors)
    self._surfaceMaskRevision = self._materialRevision
    self._surfaceMaskSignature = signature
    return visibleActors
end

---@param cacheKey        string
---@param layer           Engine.TileLayer
---@param worldMask?      Global.GameMap.WorldTileMaskConfig
---@param regionRevision? integer
function GameMapLighting:_setTileMaskUniforms(cacheKey, layer, worldMask, regionRevision)
    local effectiveRegionRevision = regionRevision or false
    local cached = self._layerMaskTextureCache[cacheKey]
    ---@cast cached Global.GameMap.LayerMaskTextureCacheEntry | nil
    if cached == nil or cached[1] ~= self._materialRevision or cached[2] ~= effectiveRegionRevision then
        local lightBlockImage = layer:getLightBlockImage()
        local reflectionStrengthImage = layer:getReflectionStrengthImage()
        local ignoreLightingImage = layer:getIgnoreLightingImage()
        ---@cast lightBlockImage sf.Image
        ---@cast reflectionStrengthImage sf.Image
        ---@cast ignoreLightingImage sf.Image
        cached = {
            self._materialRevision, effectiveRegionRevision, sf.Texture.new(lightBlockImage),
            sf.Texture.new(reflectionStrengthImage), sf.Texture.new(ignoreLightingImage)
        }
        self._layerMaskTextureCache[cacheKey] = cached
    end
    local lightBlockTexture = cached[3]
    local reflectionStrengthTexture = cached[4]
    local ignoreLightingTexture = cached[5]
    ---@cast lightBlockTexture sf.Texture
    ---@cast reflectionStrengthTexture sf.Texture
    ---@cast ignoreLightingTexture sf.Texture
    self._tilemapLightMaskShader:setUniform("lightBlockTex", lightBlockTexture)
    self._tilemapLightMaskShader:setUniform("reflectionStrengthTex", reflectionStrengthTexture)
    self._tilemapLightMaskShader:setUniform("ignoreLightingTex", ignoreLightingTexture)
    self._tilemapLightMaskShader:setUniform("lightBlockSize", self._lightBlockSize)
    if worldMask == nil then
        self._tilemapLightMaskShader:setUniform("worldMode", 0.0)
        self._tilemapLightMaskShader:setUniform("mapSize", self._shaderMapSize)
        return
    end
    self._tilemapLightMaskShader:setUniform("worldMode", 1.0)
    self._tilemapLightMaskShader:setUniform("mapSize", worldMask.regionSize)
    self._tilemapLightMaskShader:setUniform("maskTargetSize", worldMask.targetSize)
    self._tilemapLightMaskShader:setUniform("maskViewSize", worldMask.viewSize)
    self._tilemapLightMaskShader:setUniform("maskViewPosition", worldMask.viewPosition)
    self._tilemapLightMaskShader:setUniform("maskViewRotation", worldMask.viewRotation)
    self._tilemapLightMaskShader:setUniform("regionPosition", worldMask.regionPosition)
end

---@param actor Engine.Actor
function GameMapLighting:_setActorMaskUniforms(actor)
    self._lightMaskShader:setUniform("lightBlock", actor:getLightBlock())
    self._lightMaskShader:setUniform("reflectionStrength", actor:getMirror() and actor:getReflectionStrength() or 0.0)
    self._lightMaskShader:setUniform("ignoreLighting", actor:getIgnoreLighting() and 1.0 or 0.0)
end

return GameMapLighting
