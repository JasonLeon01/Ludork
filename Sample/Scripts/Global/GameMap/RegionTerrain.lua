local GameMapTerrain = require("Global.GameMap.Terrain")

---@class Global.GameMap.RegionTerrain
local RegionTerrain = {}

---@param tilemap          Engine.Tilemap
---@param autoTileResolver fun(name: string): Engine.AutoTile
function RegionTerrain:init(tilemap, autoTileResolver)
    self._tilemap = tilemap
    self._autoTileResolver = autoTileResolver
end

function RegionTerrain:getTilemap()
    return self._tilemap
end

---@diagnostic disable-next-line: unused
function RegionTerrain:markPassabilityDirty()
end

---@param _layerName          string
---@param layer               Engine.TileLayer
---@param layerData           Engine.TileLayerData
---@param autoTileTextures    sf.Texture[]
---@param autoTileFrameCounts integer[]
function RegionTerrain:_replaceTerrainLayer(_layerName, layer, layerData, autoTileTextures, autoTileFrameCounts)
    self._tilemap:addLayer(layer:rebuild(layerData, autoTileTextures, autoTileFrameCounts))
end

return class(RegionTerrain, GameMapTerrain)
