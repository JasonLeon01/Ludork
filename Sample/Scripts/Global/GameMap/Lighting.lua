local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local Pool = require("Global.Pool")

local Light = GlobalCore.Light
local System = GlobalCore.System

local MAX_SHADER_LIGHTS = 16
local DYNAMIC_TRANSMISSION_PADDING = 2
local UNOBSTRUCTED_LIGHT_INTENSITY_UNIFORMS = {}
for index = 0, MAX_SHADER_LIGHTS - 1 do
    UNOBSTRUCTED_LIGHT_INTENSITY_UNIFORMS[index + 1] = "lightIntensity[" .. index .. "]"
end

---@param size  sf.Vector2u
---@param scale number
---@return sf.Vector2u
local function getLightingTargetSize(size, scale)
    local targetSize = sf.Vector2u.new(math.max(1, math.floor(size.x * scale)), math.max(1, math.floor(size.y * scale)))
    ---@cast targetSize sf.Vector2u
    return targetSize
end

---@class (partial) GameMap
local GameMapLighting = {}

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
    for index, listed in ipairs(self._lights) do
        if listed == light then
            table.remove(self._lights, index)
            return
        end
    end
    error("Light not found in map", 2)
end

---@param light GlobalCore.Light
function GameMapLighting:_requireLight(light)
    for _, listed in ipairs(self._lights) do
        if listed == light then
            return
        end
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

---@param activeLights table[]
function GameMapLighting:_renderLighting(activeLights)
    if self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    self:_rebuildStaticTransmission()
    local visibleActors = self:_renderSurfaceMask()
    self:_ensureDirectLight()
    ---@cast self._directLight sf.RenderTexture
    ---@cast self._camera GlobalCore.Camera
    self._useStaticDirectLight = bool(activeLights)
        and not self:_hasRelevantLightBlockingActors(activeLights, visibleActors)
    if self._useStaticDirectLight then
        self:_renderCachedLighting(activeLights)
        return
    end
    self._directLight:setView(self._camera:getView())
    self._directLight:clear(sf.Color.Black)
    if bool(activeLights) then
        self:_renderDynamicLighting(activeLights, visibleActors)
    end
    self._directLight:display()
end

---@param activeLights  table[]
---@param visibleActors table<Engine.Actor, boolean>
---@return boolean
function GameMapLighting:_hasRelevantLightBlockingActors(activeLights, visibleActors)
    for actor in pairs(visibleActors) do
        if not actor:isDestroyed() and actor:getLightBlock() > 0.0 then
            for _, entry in ipairs(activeLights) do
                if actor ~= entry.owner and self:_actorIntersectsLight(actor, entry.light) then
                    return true
                end
            end
        end
    end
    return false
end

---@param activeLights  table[]
---@param visibleActors table<Engine.Actor, boolean>
function GameMapLighting:_renderDynamicLighting(activeLights, visibleActors)
    ---@cast self._directLight sf.RenderTexture
    local unobstructedLights = {}
    local staticLights = {}
    self:_ensureDynamicTransmission(activeLights)
    local commonUniformsSet = false
    for _, entry in ipairs(activeLights) do
        local hasStaticTransmissionLoss = self:_lightHasStaticTransmissionLoss(entry.light)
        local dynamicOrigin, dynamicSize, hasDynamicTransmissionLoss = self:_renderDynamicTransmission(
            entry.light, entry.owner, visibleActors
        )
        if hasDynamicTransmissionLoss then
            if not commonUniformsSet then
                self:_setLightPassCommonUniforms()
                commonUniformsSet = true
            end
            self:_renderLight(entry, dynamicOrigin, dynamicSize, hasStaticTransmissionLoss, true, self._directLight)
        elseif hasStaticTransmissionLoss then
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

---@param activeLights table[]
function GameMapLighting:_renderCachedLighting(activeLights)
    self:_ensureStaticDirectLight()
    ---@cast self._staticDirectLight sf.RenderTexture
    local cacheValid = self._cachedLightMaterialRevision == self._materialRevision and self._cachedLightTransmissionSignature
            == self._staticTransmissionSignature and self:_lightsMatchCache(activeLights, self._cachedActiveLights)
    if cacheValid then
        return
    end
    local unobstructedLights = {}
    local staticLights = {}
    for _, entry in ipairs(activeLights) do
        if self:_lightHasStaticTransmissionLoss(entry.light) then
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

function GameMapLighting:_rebuildStaticTransmission()
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
    ---@type number[][]
    local occupancy = {}
    for y = 1, size.y do
        local row = {}
        for x = 1, size.x do
            row[x] = 0.0
        end
        occupancy[y] = row
    end
    for _, layerName in ipairs(self._layerNames) do
        local layer = self._tilemap:getLayer(layerName)
        ---@cast layer Engine.TileLayer
        if layer.visible then
            local lightBlockMap = layer:getLightBlockMap()
            for y, row in ipairs(lightBlockMap) do
                local occupancyRow = occupancy[y]
                ---@cast occupancyRow number[]
                for x, lightBlock in ipairs(row) do
                    if lightBlock > 0.0 then
                        occupancyRow[x] = 1.0
                    end
                end
            end
            self:_setTileMaskUniforms(layerName, layer)
            local drawable = layer
            ---@cast drawable sf.Drawable
            self._staticTransmission:draw(drawable, self._transmissionTileRenderStates)
        end
    end
    self._staticTransmission:display()
    self:_rebuildStaticOccupancy(size, occupancy)
    self._staticTransmissionRevision = self._materialRevision
    self._staticTransmissionSignature = transmissionSignature
end

---@param size      sf.Vector2u
---@param occupancy number[][]
function GameMapLighting:_rebuildStaticOccupancy(size, occupancy)
    local prefix = {}
    local firstPrefixRow = {}
    prefix[1] = firstPrefixRow
    ---@cast prefix number[][]
    ---@cast firstPrefixRow number[]
    for x = 1, size.x + 1 do
        firstPrefixRow[x] = 0
    end
    local textureRows = {}
    for y = 1, size.y do
        local prefixRow = { 0 } ---@type number[]
        local previousPrefixRow = prefix[y]
        local occupancyRow = occupancy[y]
        ---@cast previousPrefixRow number[]
        ---@cast occupancyRow number[]
        for x = 1, size.x do
            local occupancyValue = occupancyRow[x]
            local previousPrefixValue = previousPrefixRow[x + 1]
            local prefixValue = prefixRow[x]
            local previousRowPrefixValue = previousPrefixRow[x]
            ---@cast occupancyValue number
            ---@cast previousPrefixValue number
            ---@cast prefixValue number
            ---@cast previousRowPrefixValue number
            prefixRow[x + 1] = occupancyValue + previousPrefixValue + prefixValue - previousRowPrefixValue
        end
        prefix[y + 1] = prefixRow
        textureRows[size.y - y + 1] = occupancyRow
    end
    self._staticOccupancyPrefix = prefix
    local lastPrefixRow = prefix[size.y + 1]
    ---@cast lastPrefixRow number[]
    local lastPrefixValue = lastPrefixRow[size.x + 1]
    ---@cast lastPrefixValue number
    self._staticHasTransmissionLoss = lastPrefixValue > 0
    self._staticOccupancy = self:generateDataFromMap(size, textureRows, false)
end

---@return tuple<any>
function GameMapLighting:_getStaticTransmissionSignature()
    local states = {}
    for _, layerName in ipairs(self._layerNames) do
        local layer = self._tilemap:getLayer(layerName)
        ---@cast layer Engine.TileLayer
        states[#states + 1] = bool(layer.visible)
    end
    if self._coverPlayerX == nil or self._coverPlayerY == nil
        or self._coverPlayerLayerIndex == nil or self._coverAlpha == nil then
        states[#states + 1] = tuple { "none" }
    else
        states[#states + 1] = tuple {
            "cover", self._coverPlayerX, self._coverPlayerY, self._coverPlayerLayerIndex, self._coverAlpha
        }
    end
    return tuple(states)
end

---@return table<Engine.Actor, boolean>
function GameMapLighting:_renderSurfaceMask()
    local visibleActors = {}
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
                    visibleActors[actor] = true
                    self:_setActorMaskUniforms(actor)
                    self._surfaceMask:draw(actor, self._surfaceActorRenderStates)
                end
            end
        end
    end
    self._surfaceMask:display()
    return visibleActors
end

---@param layerName string
---@param layer     Engine.TileLayer
function GameMapLighting:_setTileMaskUniforms(layerName, layer)
    local lightBlockImage = layer:getLightBlockImage()
    local reflectionStrengthImage = layer:getReflectionStrengthImage()
    local ignoreLightingImage = layer:getIgnoreLightingImage()
    ---@cast lightBlockImage sf.Image
    ---@cast reflectionStrengthImage sf.Image
    ---@cast ignoreLightingImage sf.Image
    local lightBlockTexture
    local reflectionStrengthTexture
    local ignoreLightingTexture
    if self._layerMaskTextureCache[layerName] == nil or self._layerMaskTextureCache[layerName][1] ~= lightBlockImage
        or self._layerMaskTextureCache[layerName][2] ~= reflectionStrengthImage
        or self._layerMaskTextureCache[layerName][3] ~= ignoreLightingImage then
        lightBlockTexture = sf.Texture.new(lightBlockImage)
        reflectionStrengthTexture = sf.Texture.new(reflectionStrengthImage)
        ignoreLightingTexture = sf.Texture.new(ignoreLightingImage)
        self._layerMaskTextureCache[layerName] = {
            lightBlockImage, reflectionStrengthImage, ignoreLightingImage, lightBlockTexture, reflectionStrengthTexture,
            ignoreLightingTexture
        }
    else
        lightBlockTexture = self._layerMaskTextureCache[layerName][4]
        reflectionStrengthTexture = self._layerMaskTextureCache[layerName][5]
        ignoreLightingTexture = self._layerMaskTextureCache[layerName][6]
    end
    ---@cast lightBlockTexture sf.Texture
    ---@cast reflectionStrengthTexture sf.Texture
    ---@cast ignoreLightingTexture sf.Texture
    self._tilemapLightMaskShader:setUniform("lightBlockTex", lightBlockTexture)
    self._tilemapLightMaskShader:setUniform("reflectionStrengthTex", reflectionStrengthTexture)
    self._tilemapLightMaskShader:setUniform("ignoreLightingTex", ignoreLightingTexture)
    self._tilemapLightMaskShader:setUniform("lightBlockSize", self._lightBlockSize)
    self._tilemapLightMaskShader:setUniform("mapSize", self._shaderMapSize)
end

---@param actor Engine.Actor
function GameMapLighting:_setActorMaskUniforms(actor)
    self._lightMaskShader:setUniform("lightBlock", actor:getLightBlock())
    self._lightMaskShader:setUniform("reflectionStrength", actor:getMirror() and actor:getReflectionStrength() or 0.0)
    self._lightMaskShader:setUniform("ignoreLighting", actor:getIgnoreLighting() and 1.0 or 0.0)
end

---@param activeLights table[]
function GameMapLighting:_ensureDynamicTransmission(activeLights)
    local requiredSize = 1
    for _, entry in ipairs(activeLights) do
        local light = entry.light
        local width = math.ceil(light.position.x + light.radius) - math.floor(light.position.x - light.radius)
            + DYNAMIC_TRANSMISSION_PADDING * 2
        local height = math.ceil(light.position.y + light.radius) - math.floor(light.position.y - light.radius)
            + DYNAMIC_TRANSMISSION_PADDING * 2
        requiredSize = math.max(requiredSize, width, height)
    end
    if self._dynamicTransmission ~= nil and self._dynamicTransmissionPixelSize >= requiredSize then
        return
    end
    local size = sf.Vector2u.new(requiredSize, requiredSize)
    ---@cast size sf.Vector2u
    self._dynamicTransmission = sf.RenderTexture.new(size)
    self._dynamicTransmission:setSmooth(false)
    self._dynamicTransmissionPixelSize = requiredSize
end

function GameMapLighting:_ensureDirectLight()
    ---@cast self._camera GlobalCore.Camera
    local renderTexture = self._camera:getRenderTexture()
    ---@cast renderTexture sf.RenderTexture
    local lightingRenderScale = System.getLightingRenderScale()
    local requiredSize = getLightingTargetSize(renderTexture:getSize(), lightingRenderScale)
    if self._directLight ~= nil and self._directLight:getSize() == requiredSize then
        self._directLight:setSmooth(lightingRenderScale < 1.0)
        return
    end
    self._directLight = sf.RenderTexture.new(requiredSize)
    self._directLight:setSmooth(lightingRenderScale < 1.0)
end

function GameMapLighting:_ensureStaticDirectLight()
    local tilemapSize = self._tilemap:getSize()
    local mapPixelSize = sf.Vector2u.new(tilemapSize.x * Engine.CellSize, tilemapSize.y * Engine.CellSize)
    ---@cast mapPixelSize sf.Vector2u
    local lightingRenderScale = System.getLightingRenderScale()
    local requiredSize = getLightingTargetSize(mapPixelSize, lightingRenderScale)
    if self._staticDirectLight ~= nil and self._staticDirectLight:getSize() == requiredSize then
        self._staticDirectLight:setSmooth(lightingRenderScale < 1.0)
        return
    end
    self._staticDirectLight = sf.RenderTexture.new(requiredSize)
    self._staticDirectLight:setSmooth(lightingRenderScale < 1.0)
    self._cachedLightMaterialRevision = -1
end

function GameMapLighting:_setLightPassCommonUniforms()
    ---@cast self._camera GlobalCore.Camera
    ---@cast self._directLight sf.RenderTexture
    local screenSize = self._camera:getViewSize()
    local targetSize = self._directLight:getSize()
    ---@cast screenSize sf.Vector2f
    self:_setLightPassTextureUniforms()
    self._lightPassShader:setUniform(
        "targetPixelScale", sf.Vector2f.new(targetSize.x / screenSize.x, targetSize.y / screenSize.y)
    )
    self:_setViewShaderUniforms(self._lightPassShader, screenSize, self._zeroShaderOffset, true)
end

function GameMapLighting:_setLightPassWorldUniforms()
    ---@cast self._staticDirectLight sf.RenderTexture
    local tilemapSize = self._tilemap:getSize()
    local screenSize = sf.Vector2f.new(tilemapSize.x * Engine.CellSize, tilemapSize.y * Engine.CellSize)
    local targetSize = self._staticDirectLight:getSize()
    self:_setLightPassTextureUniforms()
    self._lightPassShader:setUniform(
        "targetPixelScale", sf.Vector2f.new(targetSize.x / screenSize.x, targetSize.y / screenSize.y)
    )
    self._lightPassShader:setUniform("screenSize", screenSize)
    self._lightPassShader:setUniform("mapViewOffset", self._zeroShaderOffset)
    self._lightPassShader:setUniform("viewPos", self._zeroShaderOffset)
    self._lightPassShader:setUniform("viewRot", 0.0)
    self._lightPassShader:setUniform("gridSize", self._shaderMapSize)
    self._lightPassShader:setUniform("cellSize", Engine.CellSize)
end

function GameMapLighting:_setLightPassTextureUniforms()
    ---@cast self._staticTransmission sf.RenderTexture
    self._lightPassShader:setUniform("staticTransmission", self._staticTransmission:getTexture())
    self._lightPassShader:setUniform("staticOccupancy", self._staticOccupancy)
end

---@param shader                  sf.Shader
---@param screenSize              sf.Vector2f
---@param mapViewOffset           sf.Vector2f
---@param usesFragmentCoordinates boolean
function GameMapLighting:_setViewShaderUniforms(shader, screenSize, mapViewOffset, usesFragmentCoordinates)
    if usesFragmentCoordinates then
        shader:setUniform("mapViewOffset", mapViewOffset)
    end
    shader:setUniform("screenSize", screenSize)
    ---@cast self._camera GlobalCore.Camera
    local viewPosition = self._camera:getViewPosition()
    ---@cast viewPosition sf.Vector2f
    shader:setUniform("viewPos", viewPosition)
    shader:setUniform("viewRot", self._camera:getViewRotation():asDegrees())
    shader:setUniform("gridSize", self._shaderMapSize)
    shader:setUniform("cellSize", Engine.CellSize)
end

---@param entry         table
---@param dynamicOrigin sf.Vector2f
---@param dynamicSize   sf.Vector2f
---@param traceStatic   boolean
---@param traceDynamic  boolean
---@param target        sf.RenderTexture
function GameMapLighting:_renderLight(entry, dynamicOrigin, dynamicSize, traceStatic, traceDynamic, target)
    local light = entry.light
    if traceDynamic then
        ---@cast self._dynamicTransmission sf.RenderTexture
        self._lightPassShader:setUniform("dynamicTransmission", self._dynamicTransmission:getTexture())
    end
    self._lightPassShader:setUniform("dynamicMaskOrigin", dynamicOrigin)
    self._lightPassShader:setUniform("dynamicMaskSize", dynamicSize)
    self._lightPassShader:setUniform("traceStatic", traceStatic and 1.0 or 0.0)
    self._lightPassShader:setUniform("traceDynamic", traceDynamic and 1.0 or 0.0)
    self._lightPassShader:setUniform("lightPos", light.position)
    self._lightPassShader:setUniform("lightColor", self:_toShaderColour(light.colour, false))
    self._lightPassShader:setUniform("lightRadius", light.radius)
    self._lightPassShader:setUniform("lightIntensity", light.intensity)
    local diameter = light.radius * 2.0
    ---@cast self._lightPassQuad sf.RectangleShape
    ---@cast self._lightPassRenderStates sf.RenderStates
    self._lightPassQuad:setSize(sf.Vector2f.new(diameter, diameter))
    self._lightPassQuad:setPosition(sf.Vector2f.new(light.position.x - light.radius, light.position.y - light.radius))
    target:draw(self._lightPassQuad, self._lightPassRenderStates)
end

---@param light GlobalCore.Light
---@return boolean
function GameMapLighting:_lightHasStaticTransmissionLoss(light)
    if not self._staticHasTransmissionLoss then
        return false
    end
    local size = self._tilemap:getSize()
    local cellSize = Engine.CellSize
    local minimumX = math.max(0, math.floor((light.position.x - light.radius) / cellSize) - 1)
    local minimumY = math.max(0, math.floor((light.position.y - light.radius) / cellSize) - 1)
    local maximumX = Engine.ToInteger(
        math.min(size.x - 1, math.floor((light.position.x + light.radius) / cellSize) + 1)
    )
    local maximumY = Engine.ToInteger(
        math.min(size.y - 1, math.floor((light.position.y + light.radius) / cellSize) + 1)
    )
    if minimumX > maximumX or minimumY > maximumY then
        return false
    end
    ---@cast self._staticOccupancyPrefix number[][]
    ---@diagnostic disable: need-check-nil
    local total = self._staticOccupancyPrefix[maximumY + 2][maximumX + 2] - self._staticOccupancyPrefix[minimumY + 1][maximumX
            + 2] - self._staticOccupancyPrefix[maximumY + 2][minimumX + 1]
        + self._staticOccupancyPrefix[minimumY + 1][minimumX + 1]
    ---@diagnostic enable: need-check-nil
    return total > 0
end

---@param entries table[]
---@param target  sf.RenderTexture
function GameMapLighting:_renderStaticLights(entries, target)
    for _, entry in ipairs(entries) do
        self:_renderLight(entry, self._zeroShaderOffset, self._zeroShaderOffset, true, false, target)
    end
end

---@param entries table[]
---@param target  sf.RenderTexture
function GameMapLighting:_renderUnobstructedLights(entries, target)
    if not bool(entries) then
        self._unobstructedLightCache = nil
        return
    end
    if not self:_lightsMatchCache(entries, self._unobstructedLightCache) then
        self:_cacheUnobstructedLights(entries)
    end
    ---@cast self._unobstructedLightVertices sf.VertexArray
    ---@cast self._unobstructedLightPassRenderStates sf.RenderStates
    target:draw(self._unobstructedLightVertices, self._unobstructedLightPassRenderStates)
end

---@param entries table[]
---@param cache   table[] | nil
---@return boolean
---@diagnostic disable-next-line: unused
function GameMapLighting:_lightsMatchCache(entries, cache)
    if cache == nil or #cache ~= #entries then
        return false
    end
    for index, entry in ipairs(entries) do
        local light = entry.light
        local cached = cache[index]
        ---@cast cached number[]
        if cached[1] ~= light.position.x or cached[2] ~= light.position.y or cached[3] ~= light.colour.r
            or cached[4] ~= light.colour.g or cached[5] ~= light.colour.b or cached[6] ~= light.radius
            or cached[7] ~= light.intensity then
            return false
        end
    end
    return true
end

---@param entries table[]
function GameMapLighting:_cacheUnobstructedLights(entries)
    local cache = {}
    ---@cast self._unobstructedLightVertices sf.VertexArray
    ---@cast self._unobstructedLightVertex sf.Vertex
    self._unobstructedLightVertices:clear()
    for index, entry in ipairs(entries) do
        local light = entry.light
        cache[index] = self:_cacheLightValues(light)
        self._unobstructedLightPassShader:setUniform(UNOBSTRUCTED_LIGHT_INTENSITY_UNIFORMS[index], light.intensity)
        self:_appendLightBatch(self._unobstructedLightVertices, self._unobstructedLightVertex, light, index - 1)
    end
    self._unobstructedLightCache = cache
end

---@param light GlobalCore.Light
---@return table
---@diagnostic disable-next-line: unused
function GameMapLighting:_cacheLightValues(light)
    return {
        light.position.x, light.position.y, light.colour.r, light.colour.g, light.colour.b, light.radius,
        light.intensity
    }
end

---@param entries table[]
---@return table[]
function GameMapLighting:_cacheLightList(entries)
    local cache = {}
    for index, entry in ipairs(entries) do
        cache[index] = self:_cacheLightValues(entry.light)
    end
    return cache
end

---@param vertices sf.VertexArray
---@param vertex   sf.Vertex
---@param light    GlobalCore.Light
---@param index    integer
function GameMapLighting:_appendLightBatch(vertices, vertex, light, index)
    local radius = light.radius
    local left = light.position.x - radius
    local right = light.position.x + radius
    local top = light.position.y - radius
    local bottom = light.position.y + radius
    local colour = sf.Color.new(light.colour.r, light.colour.g, light.colour.b, index)
    self:_appendLightBatchVertex(vertices, vertex, left, top, -1.0, -1.0, colour)
    self:_appendLightBatchVertex(vertices, vertex, right, top, 1.0, -1.0, colour)
    self:_appendLightBatchVertex(vertices, vertex, right, bottom, 1.0, 1.0, colour)
    self:_appendLightBatchVertex(vertices, vertex, left, top, -1.0, -1.0, colour)
    self:_appendLightBatchVertex(vertices, vertex, right, bottom, 1.0, 1.0, colour)
    self:_appendLightBatchVertex(vertices, vertex, left, bottom, -1.0, 1.0, colour)
end

---@param vertices sf.VertexArray
---@param vertex   sf.Vertex
---@param x        number
---@param y        number
---@param textureX number
---@param textureY number
---@param colour   sf.Color
---@diagnostic disable-next-line: unused
function GameMapLighting:_appendLightBatchVertex(vertices, vertex, x, y, textureX, textureY, colour)
    vertex.position = sf.Vector2f.new(x, y)
    vertex.texCoords = sf.Vector2f.new(textureX, textureY)
    vertex.color = colour
    vertices:append(vertex)
end

---@param light         GlobalCore.Light
---@param owner         Engine.Actor | nil
---@param visibleActors table<Engine.Actor, boolean>
---@return sf.Vector2f, sf.Vector2f, boolean
function GameMapLighting:_renderDynamicTransmission(light, owner, visibleActors)
    local cellRadius = math.ceil((light.radius + Engine.CellSize) / Engine.CellSize)
    local mapX = math.floor(light.position.x / Engine.CellSize)
    local mapY = math.floor(light.position.y / Engine.CellSize)
    local actors
    if owner == nil then
        actors = self:getActorsInRange(mapX, mapY, cellRadius)
    else
        actors = self:getActorsInRangeExcluding(mapX, mapY, cellRadius, owner)
    end
    ---@cast actors Engine.Actor[]
    local occluders = {}
    for _, actor in ipairs(actors) do
        if visibleActors[actor] == true and not actor:isDestroyed()
            and actor:getLightBlock() > 0.0 and self:_actorIntersectsLight(actor, light) then
            occluders[#occluders + 1] = actor
        end
    end
    if not bool(occluders) then
        return self._zeroShaderOffset, self._zeroShaderOffset, false
    end

    local minimumX = nil
    local minimumY = nil
    local maximumX = nil
    local maximumY = nil
    for _, actor in ipairs(occluders) do
        local bounds = actor:getGlobalBounds()
        if minimumX == nil or bounds.position.x < minimumX then
            minimumX = bounds.position.x
        end
        if minimumY == nil or bounds.position.y < minimumY then
            minimumY = bounds.position.y
        end
        local right = bounds.position.x + bounds.size.x
        if maximumX == nil or right > maximumX then
            maximumX = right
        end
        local bottom = bounds.position.y + bounds.size.y
        if maximumY == nil or bottom > maximumY then
            maximumY = bottom
        end
    end
    ---@cast minimumX number
    ---@cast minimumY number
    ---@cast maximumX number
    ---@cast maximumY number
    minimumX = math.max(
        math.floor(light.position.x - light.radius) - DYNAMIC_TRANSMISSION_PADDING,
        math.floor(minimumX) - DYNAMIC_TRANSMISSION_PADDING
    )
    minimumY = math.max(
        math.floor(light.position.y - light.radius) - DYNAMIC_TRANSMISSION_PADDING,
        math.floor(minimumY) - DYNAMIC_TRANSMISSION_PADDING
    )
    maximumX = math.min(
        math.ceil(light.position.x + light.radius) + DYNAMIC_TRANSMISSION_PADDING,
        math.ceil(maximumX) + DYNAMIC_TRANSMISSION_PADDING
    )
    maximumY = math.min(
        math.ceil(light.position.y + light.radius) + DYNAMIC_TRANSMISSION_PADDING,
        math.ceil(maximumY) + DYNAMIC_TRANSMISSION_PADDING
    )
    local origin = sf.Vector2f.new(minimumX, minimumY)
    local size = sf.Vector2f.new(math.max(1.0, maximumX - minimumX), math.max(1.0, maximumY - minimumY))
    local centre = sf.Vector2f.new(origin.x + size.x * 0.5, origin.y + size.y * 0.5)
    ---@cast self._dynamicTransmission sf.RenderTexture
    ---@cast self._transmissionActorRenderStates sf.RenderStates
    self._dynamicTransmission:setView(sf.View.new(centre, size))
    self._dynamicTransmission:clear(sf.Color.White)
    self._lightMaskShader:setUniform("transmissionMode", 1.0)
    for _, actor in ipairs(occluders) do
        self:_setActorMaskUniforms(actor)
        self._dynamicTransmission:draw(actor, self._transmissionActorRenderStates)
    end
    self._dynamicTransmission:display()
    return origin, size, true
end

---@param actor Engine.Actor
---@param light GlobalCore.Light
---@return boolean
---@diagnostic disable-next-line: unused
function GameMapLighting:_actorIntersectsLight(actor, light)
    local bounds = actor:getGlobalBounds()
    return bounds.position.x <= light.position.x + light.radius and bounds.position.x + bounds.size.x
            >= light.position.x - light.radius and bounds.position.y <= light.position.y + light.radius
        and bounds.position.y + bounds.size.y >= light.position.y - light.radius
end

---@return table[]
function GameMapLighting:_getActiveLights()
    local lights = {}
    local viewport = self._camera ~= nil and self._camera:getViewport() or nil
    for _, light in ipairs(self._lights) do
        if light.radius > 0.0 and self:_isLightVisible(light.position, light.radius, viewport) then
            lights[#lights + 1] = { light = light }
            if #lights >= MAX_SHADER_LIGHTS then
                return lights
            end
        end
    end
    ---@type sf.Vector2f | nil
    local position = nil
    for _, actor in ipairs(self:getAllActors()) do
        local lightComp = actor.lightComp
        if lightComp ~= nil and not actor:isDestroyed() then
            local radius = lightComp.lightRadius
            if radius > 0.0 then
                if position == nil then
                    position = Pool.Get("sf.Vector2f", sf.Vector2f, {
                        x = 0.0,
                        y = 0.0
                    })
                end
                ---@cast position sf.Vector2f
                self:_getActorLightPosition(actor, lightComp, position)
                if self:_isLightVisible(position, radius, viewport) then
                    lights[#lights + 1] = {
                        light = Light.new(position, lightComp.lightColour, radius, 1.0),
                        owner = actor
                    }
                    if #lights >= MAX_SHADER_LIGHTS then
                        break
                    end
                end
            end
        end
    end
    if position ~= nil then
        Pool.Put("sf.Vector2f", position)
    end
    return lights
end

---@param actor     Engine.Actor
---@param lightComp Engine.LightComponent
---@param result    sf.Vector2f | nil
---@return sf.Vector2f
---@diagnostic disable-next-line: unused
function GameMapLighting:_getActorLightPosition(actor, lightComp, result)
    local bounds = actor:getLocalBounds()
    local offset = lightComp.lightOffset
    local localPosition = sf.Vector2f.new(
        bounds.position.x + bounds.size.x * 0.5 + offset.x, bounds.position.y + bounds.size.y * 0.5 + offset.y
    )
    local worldPosition = actor:getTransform():transformPoint(localPosition)
    result = result or sf.Vector2f.new()
    result.x = worldPosition.x
    result.y = worldPosition.y
    return result
end

---@param position sf.Vector2f
---@param radius   number
---@param viewport sf.FloatRect | nil
---@return boolean
function GameMapLighting:_isLightVisible(position, radius, viewport)
    if viewport == nil then
        return true
    end
    ---@cast self._camera GlobalCore.Camera
    local radians = self._camera:getViewRotation():asDegrees() * 0.017453292519943295
    local cosine = math.abs(math.cos(radians))
    local sine = math.abs(math.sin(radians))
    local halfWidth = viewport.size.x * 0.5
    local halfHeight = viewport.size.y * 0.5
    local extentX = cosine * halfWidth + sine * halfHeight
    local extentY = sine * halfWidth + cosine * halfHeight
    local centreX = viewport.position.x + halfWidth
    local centreY = viewport.position.y + halfHeight
    return position.x + radius >= centreX - extentX and position.x - radius <= centreX + extentX
        and position.y + radius >= centreY - extentY and position.y - radius <= centreY + extentY
end

---@param colour     sf.Color
---@param applyAlpha boolean
---@return sf.Vector3f
function GameMapLighting:_toShaderColour(colour, applyAlpha)
    local alpha = applyAlpha and colour.a / 255.0 or 1.0
    self._shaderColour.x = colour.r / 255.0 * alpha
    self._shaderColour.y = colour.g / 255.0 * alpha
    self._shaderColour.z = colour.b / 255.0 * alpha
    return self._shaderColour
end

return class(GameMapLighting)
