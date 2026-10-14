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
function GameMapLighting.RefreshShader(self)
    if self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
    end
end

return GameMapLighting
