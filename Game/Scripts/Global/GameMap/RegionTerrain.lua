local TerrainOperations = require("Global.GameMap.TerrainOperations")

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
    local changedPositions, layer, layerData, autoTileTextures, autoTileFrameCounts = TerrainOperations.SetTiles(
        self._tilemap, self._autoTileResolver, layerName, positions, tileID
    )
    if not bool(changedPositions) then
        return {}
    end
    ---@cast layer Engine.TileLayer
    ---@cast layerData Engine.TileLayerData
    ---@cast autoTileTextures sf.Texture[]
    ---@cast autoTileFrameCounts integer[]
    self:_replaceTerrainLayer(layerName, layer, layerData, autoTileTextures, autoTileFrameCounts)
    self:markPassabilityDirty()
    return changedPositions
end

function RegionTerrain:applyTerrainDestructions(terrainDestructions)
    for layerName, changes in pairs(terrainDestructions) do
        for _, change in pairs(changes) do
            self:setTerrainTile(layerName, change.position, change.tileID)
        end
    end
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
