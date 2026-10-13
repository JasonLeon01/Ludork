local GameMapLighting = {}

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
        if not actor:isDestroyed() and actor:isVisibleInHierarchy() and self:isActorVisibleOnMap(actor) then
            local position = actor:getMapPosition()
            result[position.y + 1][position.x + 1] = actor:getLightBlock()
        end
    end
    return result
end

---@param self GameMapImplState
function GameMapLighting.RefreshShader(self)
    if self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
end

return GameMapLighting
