---@type GameMapImplState
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
    local index = table.index(self._lights, light)
    if index == nil then
        error("Light not found in map", 2)
    end
    table.remove(self._lights, index)
end

---@param light GlobalCore.Light
function GameMapLighting:_requireLight(light)
    if not table.contains(self._lights, light) then
        error("Light not found in map", 3)
    end
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

function GameMapLighting:_lightingShadersAvailable()
    return self._renderer ~= nil and not self._previewOnly and sf.Shader.isAvailable()
end

function GameMapLighting:_getActiveLights()
    return self._lights
end

---@param mapLights GlobalCore.Light[]
function GameMapLighting:_renderLighting(mapLights)
    if self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    assert(self._renderer ~= nil, "GameMap renderer is unavailable")
    self._renderer:renderLighting(mapLights, self._ambientLight, self._materialRevision)
end

function GameMapLighting:refreshShader()
    if self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    if self._renderer ~= nil then
        self._renderer:refreshMaterialShader(self._ambientLight)
    end
end

function GameMapLighting:_getMaterialShader()
    if self._renderer == nil then
        return nil
    end
    return self._renderer:getMaterialShader()
end

return GameMapLighting
