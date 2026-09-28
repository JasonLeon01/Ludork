local TerrainOperations = require("Global.GameMap.TerrainOperations")
local TerrainEditing = require("Global.GameMap.TerrainEditing")

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

function RegionTerrain:getTerrainTile(layerName, position)
    return TerrainOperations.GetTile(self._tilemap, layerName, position)
end

function RegionTerrain:getTerrainTilePositions(layerName, tileID)
    return TerrainOperations.GetTilePositions(self._tilemap, layerName, tileID)
end

function RegionTerrain:setTerrainTile(layerName, position, tileID)
    return bool(self:setTerrainTiles(layerName, { position }, tileID))
end

function RegionTerrain:setTerrainTiles(layerName, positions, tileID)
    ---@type Global.GameMap.TerrainEditing.ReplaceLayer
    local replaceLayer = function (layer, layerData, autoTileTextures, autoTileFrameCounts)
        self:_replaceTerrainLayer(layerName, layer, layerData, autoTileTextures, autoTileFrameCounts)
    end
    return TerrainEditing.SetTiles(
        self, self._tilemap, self._autoTileResolver, layerName, positions, tileID, replaceLayer
    )
end

function RegionTerrain:applyTerrainDestructions(terrainDestructions)
    TerrainEditing.ApplyDestructions(self, terrainDestructions)
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

return class(RegionTerrain)
