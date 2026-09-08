local GameMapLighting = {}

---@param self GameMapImplState
function GameMapLighting.GetLights(self)
    return self._lights
end

---@param self GameMapImplState
function GameMapLighting.SetLights(self, lights)
    self._lights = lights
end

---@param self GameMapImplState
function GameMapLighting.AddLight(self, light)
    self._lights[#self._lights + 1] = light
end

---@param self GameMapImplState
function GameMapLighting.RemoveLight(self, light)
    local index = table.index(self._lights, light)
    if index == nil then
        error("Light not found in map", 2)
    end
    table.remove(self._lights, index)
end

---@param light GlobalCore.Light
---@param self  GameMapImplState
function GameMapLighting.RequireLight(self, light)
    if not table.contains(self._lights, light) then
        error("Light not found in map", 3)
    end
end

---@param self GameMapImplState
function GameMapLighting.SetLightPosition(self, light, position)
    self:_requireLight(light)
    light.position = position
end

---@param self GameMapImplState
function GameMapLighting.SetLightColour(self, light, colour)
    self:_requireLight(light)
    light.colour = colour
end

---@param self GameMapImplState
function GameMapLighting.SetLightRadius(self, light, radius)
    self:_requireLight(light)
    light.radius = radius
end

---@param self GameMapImplState
function GameMapLighting.SetLightIntensity(self, light, intensity)
    self:_requireLight(light)
    light.intensity = intensity
end

---@param self GameMapImplState
function GameMapLighting.GetAmbientLight(self)
    return self._ambientLight
end

---@param self GameMapImplState
function GameMapLighting.SetAmbientLight(self, ambientLight)
    self._ambientLight = ambientLight
end

---@param self GameMapImplState
function GameMapLighting.GetMaterialPropertyMap(self, functionName, invalidValue)
    local mapSize = self._tilemap:getSize()
    return self:getMaterialPropertyMapExt(mapSize.x, mapSize.y, functionName, invalidValue)
end

---@param self GameMapImplState
function GameMapLighting.GetActorLayerLightBlockMap(self, layerName, size)
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

---@param self GameMapImplState
function GameMapLighting.LightingShadersAvailable(self)
    return self._renderer ~= nil and not self._previewOnly and sf.Shader.isAvailable()
end

---@param self GameMapImplState
function GameMapLighting.GetActiveLights(self)
    return self._lights
end

---@param mapLights GlobalCore.Light[]
---@param self      GameMapImplState
function GameMapLighting.RenderLighting(self, mapLights)
    if self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    assert(self._renderer ~= nil, "GameMap renderer is unavailable")
    self._renderer:renderLighting(mapLights, self._ambientLight, self._materialRevision)
end

---@param self GameMapImplState
function GameMapLighting.RefreshShader(self)
    if self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
    if self._renderer ~= nil then
        self._renderer:refreshMaterialShader(self._ambientLight)
    end
end

---@param self GameMapImplState
function GameMapLighting.GetMaterialShader(self)
    if self._renderer == nil then
        return nil
    end
    return self._renderer:getMaterialShader()
end

return GameMapLighting
