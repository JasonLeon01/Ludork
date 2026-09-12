local TerrainChanges = {}

---@type table<GameMap | Global.GameMap.RegionTerrain, Global.GameMap.TerrainChanges.Listener>
local listeners = setmetatable({}, { __mode = "k" })

function TerrainChanges.Subscribe(owner, listener)
    listeners[owner] = listener
end

function TerrainChanges.Unsubscribe(owner)
    listeners[owner] = nil
end

function TerrainChanges.Publish(owner, layerName, positions, tileId, previousLayer)
    local listener = listeners[owner]
    if listener ~= nil then
        listener(layerName, positions, tileId, previousLayer, assert(owner:getTilemap():getLayer(layerName)))
    end
end

return TerrainChanges
