---@meta Global.GameMap.TerrainChanges

---@alias Global.GameMap.TerrainChanges.Owner GameMap | Global.GameMap.RegionTerrain
---@alias Global.GameMap.TerrainChanges.Listener fun(layerName: string, positions: sf.Vector2i[], tileId: Global.GameMap.TerrainTileID, previousLayer: Engine.TileLayer, currentLayer: Engine.TileLayer)

---@class Global.GameMap.TerrainChanges.Module
local TerrainChanges = {}

--- Observe successful terrain writes without retaining the owner. Each owner has one current subscriber.
---@param owner    Global.GameMap.TerrainChanges.Owner
---@param listener Global.GameMap.TerrainChanges.Listener
function TerrainChanges.Subscribe(owner, listener) end

---@param owner Global.GameMap.TerrainChanges.Owner
function TerrainChanges.Unsubscribe(owner) end

--- Publish cells after replacing the runtime layer. Coordinates are local to the owner.
---@param owner         Global.GameMap.TerrainChanges.Owner
---@param layerName     string
---@param positions     sf.Vector2i[]
---@param tileId        Global.GameMap.TerrainTileID
---@param previousLayer Engine.TileLayer
function TerrainChanges.Publish(owner, layerName, positions, tileId, previousLayer) end

return TerrainChanges
