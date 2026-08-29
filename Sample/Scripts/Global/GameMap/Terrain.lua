local GlobalCore = require("GlobalCore")
local TerrainOperations = require("Global.GameMap.TerrainOperations")

local GameMapBase = GlobalCore.GameMapBase

---@type GameMapImplState
local GameMapTerrain = {}

function GameMapTerrain:getTerrainTile(layerName, position)
    return TerrainOperations.GetTile(self._tilemap, layerName, position)
end

function GameMapTerrain:getTerrainTilePositions(layerName, tileID)
    return TerrainOperations.GetTilePositions(self._tilemap, layerName, tileID)
end

function GameMapTerrain:setTerrainTile(layerName, position, tileID)
    return bool(self:setTerrainTiles(layerName, { position }, tileID))
end

function GameMapTerrain:setTerrainTiles(layerName, positions, tileID)
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

function GameMapTerrain:applyTerrainDestructions(terrainDestructions)
    for layerName, changes in pairs(terrainDestructions) do
        for _, change in pairs(changes) do
            self:setTerrainTile(layerName, change.position, change.tileID)
        end
    end
end

function GameMapTerrain:markPassabilityDirty()
    self._materialDirty = true
    self._materialRevision = self._materialRevision + 1
    self:invalidatePassabilityCache()
end

function GameMapTerrain:updateActorOccupancy(actor)
    if self._tilePassableGrid == nil or self._materialDirty then
        self:_rebuildPassabilityCache()
        self._materialDirty = false
        return
    end
    actor:syncMapCache()
    GameMapBase.updateActorOccupancy(self, actor)
end

---@param _layerName          string
---@param layer               Engine.TileLayer
---@param layerData           Engine.TileLayerData
---@param autoTileTextures    sf.Texture[]
---@param autoTileFrameCounts integer[]
function GameMapTerrain:_replaceTerrainLayer(_layerName, layer, layerData, autoTileTextures, autoTileFrameCounts)
    self:_resetTransparentTiles()
    local newLayer = layer:rebuild(layerData, autoTileTextures, autoTileFrameCounts)
    self._tilemap:addLayer(newLayer)
    self._layersTopFirst = {}
    for index = #self._layerNames, 1, -1 do
        self._layersTopFirst[#self._layersTopFirst + 1] = self._tilemap:getLayer(self._layerNames[index])
    end
end

---@param functionName string
---@param invalidValue number | boolean
---@param smooth       boolean
---@return sf.Texture
function GameMapTerrain:_getMaterialPropertyTexture(functionName, invalidValue, smooth)
    ---@diagnostic disable-next-line: return-type-mismatch
    return self:generateDataFromMap(
        self._tilemap:getSize(), self:getMaterialPropertyMap(functionName, invalidValue), smooth == true
    )
end

function GameMapTerrain:_rebuildPassabilityCache()
    local size = self._tilemap:getSize()
    self:_syncActorsForMapCache()
    self._tilePassableGrid = self:rebuildPassabilityCache(size)
end

return GameMapTerrain
