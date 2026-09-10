---@meta Global.GameMap.TerrainValue

---@class Global.GameMap.TerrainValue.Module
---@field Normalise fun(tileID: Global.GameMap.TerrainTileID): Global.GameMap.TerrainTileID
local TerrainValue = {}

---@brief Validate and canonicalise a terrain tile ID.
---@param tileID Global.GameMap.TerrainTileID
---@return Global.GameMap.TerrainTileID
function TerrainValue.Normalise(tileID) end

return TerrainValue
