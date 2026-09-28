local TerrainOperations = require("Global.GameMap.TerrainOperations")
local TerrainChanges = require("Global.GameMap.TerrainChanges")

local TerrainEditing = {}

function TerrainEditing.SetTiles(owner, tilemap, autoTileResolver, layerName, positions, tileID, replaceLayer)
    local changedPositions, layer, layerData, autoTileTextures, autoTileFrameCounts = TerrainOperations.SetTiles(
        tilemap, autoTileResolver, layerName, positions, tileID
    )
    if not bool(changedPositions) then
        return {}
    end
    ---@cast layer Engine.TileLayer
    ---@cast layerData Engine.TileLayerData
    ---@cast autoTileTextures sf.Texture[]
    ---@cast autoTileFrameCounts integer[]
    replaceLayer(layer, layerData, autoTileTextures, autoTileFrameCounts)
    owner:markPassabilityDirty()
    TerrainChanges.Publish(owner, layerName, changedPositions, tileID, layer)
    return changedPositions
end

function TerrainEditing.ApplyDestructions(owner, terrainDestructions)
    for layerName, changes in pairs(terrainDestructions) do
        for _, change in pairs(changes) do
            owner:setTerrainTile(layerName, change.position, change.tileID)
        end
    end
end

return TerrainEditing
