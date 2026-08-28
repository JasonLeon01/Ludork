---@meta Global.GameMap.RegionTerrain

---@class Global.GameMap.RegionTerrain
---@field _tilemap          Engine.Tilemap
---@field _autoTileResolver fun(name: string): Engine.AutoTile
---@field new               fun(tilemap: Engine.Tilemap, autoTileResolver: fun(name: string): Engine.AutoTile): Global.GameMap.RegionTerrain
local RegionTerrain = {}

---@param tilemap          Engine.Tilemap
---@param autoTileResolver fun(name: string): Engine.AutoTile
function RegionTerrain:init(tilemap, autoTileResolver) end

---@return Engine.Tilemap
function RegionTerrain:getTilemap() end

---@param layerName string
---@param position  sf.Vector2i
---@return Global.GameMap.TerrainTileID
function RegionTerrain:getTerrainTile(layerName, position) end

---@param layerName string
---@param tileID    Global.GameMap.TerrainTileID
---@return sf.Vector2i[]
function RegionTerrain:getTerrainTilePositions(layerName, tileID) end

---@param layerName string
---@param position  sf.Vector2i
---@param tileID    Global.GameMap.TerrainTileID
---@return boolean
function RegionTerrain:setTerrainTile(layerName, position, tileID) end

---@param layerName string
---@param positions sf.Vector2i[]
---@param tileID    Global.GameMap.TerrainTileID
---@return sf.Vector2i[]
function RegionTerrain:setTerrainTiles(layerName, positions, tileID) end

---@param terrainDestructions table<string, table<string, { position: sf.Vector2i, tileID: Global.GameMap.TerrainTileID }>>
function RegionTerrain:applyTerrainDestructions(terrainDestructions) end

return RegionTerrain
