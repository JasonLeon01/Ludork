local Engine = require("Engine")
local GameMapLightingPass = require("Global.GameMap.LightingPass")

---@class (partial) GameMap
local GameMapLighting = {}

for name, method in pairs(GameMapLightingPass) do
    GameMapLighting[name] = method
end

function GameMapLighting:getLights()
    return self._lights
end

function GameMapLighting:setLights(lights)
    self._lights = lights
end

function GameMapLighting:addLight(light)
    self._lights[#self._lights + 1] = light
end

function GameMapLighting:removeLight(light)
    local index = table.index(self._lights, light)
    if index ~= nil then
        table.remove(self._lights, index)
        return
    end
    error("Light not found in map", 2)
end

---@param light GlobalCore.Light
function GameMapLighting:_requireLight(light)
    if table.contains(self._lights, light) then
        return
    end
    error("Light not found in map", 3)
end

function GameMapLighting:setLightPosition(light, position)
    self:_requireLight(light)
    light.position = position
end

function GameMapLighting:setLightColour(light, colour)
    self:_requireLight(light)
    light.colour = colour
end

function GameMapLighting:setLightRadius(light, radius)
    self:_requireLight(light)
    light.radius = radius
end

function GameMapLighting:setLightIntensity(light, intensity)
    self:_requireLight(light)
    light.intensity = intensity
end

function GameMapLighting:getAmbientLight()
    return self._ambientLight
end

function GameMapLighting:setAmbientLight(ambientLight)
    self._ambientLight = ambientLight
end

function GameMapLighting:getMaterialPropertyMap(functionName, invalidValue)
    local mapSize = self._tilemap:getSize()
    return self:getMaterialPropertyMapExt(mapSize.x, mapSize.y, functionName, invalidValue)
end

function GameMapLighting:getActorLayerLightBlockMap(layerName, size)
    if self._actors[layerName] == nil then
        return nil
    end
    local result = {}
    for y = 1, size.y do
        result[y] = {}
        for x = 1, size.x do
            result[y][x] = 0.0
        end
    end
    for _, actor in ipairs(self._actors[layerName]) do
        local position = actor:getMapPosition()
        result[position.y + 1][position.x + 1] = actor:getLightBlock()
    end
    return result
end

function GameMapLighting:refreshShader()
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

---@param activeLights Global.GameMap.ActiveLight[]
---@param analyses     GlobalCore.LightOcclusionResult[]
function GameMapLighting:_renderDynamicLighting(activeLights, analyses)
    ---@cast self._directLight sf.RenderTexture
    local unobstructedLights = {}
    local staticLights = {}
    self:_ensureDynamicTransmission(activeLights)
    local commonUniformsSet = false
    for index, entry in ipairs(activeLights) do
        local analysis = assert(analyses[index])
        local dynamicOrigin, dynamicSize, hasDynamicTransmissionLoss = self:_renderDynamicTransmission(analysis)
        if hasDynamicTransmissionLoss then
            if not commonUniformsSet then
                self:_setLightPassCommonUniforms()
                commonUniformsSet = true
            end
            self:_renderLight(
                entry, dynamicOrigin, dynamicSize, analysis.hasStaticTransmissionLoss, true, self._directLight
            )
        elseif analysis.hasStaticTransmissionLoss then
            staticLights[#staticLights + 1] = entry
        else
            unobstructedLights[#unobstructedLights + 1] = entry
        end
    end
    if bool(staticLights) then
        if not commonUniformsSet then
            self:_setLightPassCommonUniforms()
        end
        self:_renderStaticLights(staticLights, self._directLight)
    end
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
function GameMapLighting:_rebuildStaticTransmission(_activeLights)
    local transmissionSignature = self:_getStaticTransmissionSignature()
    if self._staticTransmissionRevision == self._materialRevision
        and self._staticTransmissionSignature == transmissionSignature then
        return
    end
    ---@cast self._staticTransmission sf.RenderTexture
    ---@cast self._transmissionTileRenderStates sf.RenderStates
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
    self._staticTransmission:display()
    local origin = sf.Vector2i.new(0, 0)
    ---@cast origin sf.Vector2i
    self._staticOccupancy = assert(self:rebuildStaticLightOccupancy(origin, size))
    self._staticTransmissionRevision = self._materialRevision
    self._staticTransmissionSignature = transmissionSignature
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
        ---@type (string | integer)[]
        local coverState = {
            "cover", self._coverPlayerX, self._coverPlayerY, self._coverPlayerLayerIndex, self._coverAlpha
        }
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
                    if not visibleActorSet[actor] then
                        visibleActorSet[actor] = true
                        visibleActors[#visibleActors + 1] = actor
                    end
                    self:_setActorMaskUniforms(actor)
                    self._surfaceMask:draw(actor, self._surfaceActorRenderStates)
                end
            end
        end
    end
    self._surfaceMask:display()
    return visibleActors
end

---@param cacheKey   string
---@param layer      Engine.TileLayer
---@param worldMask? Global.GameMap.WorldTileMaskConfig
function GameMapLighting:_setTileMaskUniforms(cacheKey, layer, worldMask)
    local lightBlockImage = layer:getLightBlockImage()
    local reflectionStrengthImage = layer:getReflectionStrengthImage()
    local ignoreLightingImage = layer:getIgnoreLightingImage()
    ---@cast lightBlockImage sf.Image
    ---@cast reflectionStrengthImage sf.Image
    ---@cast ignoreLightingImage sf.Image
    local lightBlockTexture
    local reflectionStrengthTexture
    local ignoreLightingTexture
    if self._layerMaskTextureCache[cacheKey] == nil or self._layerMaskTextureCache[cacheKey][1] ~= lightBlockImage
        or self._layerMaskTextureCache[cacheKey][2] ~= reflectionStrengthImage
        or self._layerMaskTextureCache[cacheKey][3] ~= ignoreLightingImage then
        lightBlockTexture = sf.Texture.new(lightBlockImage)
        reflectionStrengthTexture = sf.Texture.new(reflectionStrengthImage)
        ignoreLightingTexture = sf.Texture.new(ignoreLightingImage)
        self._layerMaskTextureCache[cacheKey] = {
            lightBlockImage, reflectionStrengthImage, ignoreLightingImage, lightBlockTexture, reflectionStrengthTexture,
            ignoreLightingTexture
        }
    else
        lightBlockTexture = self._layerMaskTextureCache[cacheKey][4]
        reflectionStrengthTexture = self._layerMaskTextureCache[cacheKey][5]
        ignoreLightingTexture = self._layerMaskTextureCache[cacheKey][6]
    end
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

return class(GameMapLighting)
