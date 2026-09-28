---@meta Global.GameMap.TerrainEditing

---@alias Global.GameMap.TerrainEditing.ReplaceLayer fun(layer: Engine.TileLayer, layerData: Engine.TileLayerData, autoTileTextures: sf.Texture[], autoTileFrameCounts: integer[])

---@class Global.GameMap.TerrainEditing.Module
local TerrainEditing = {}

--- Replace changed terrain, invalidate passability, then notify the owner's subscriber.
---@param owner            Global.GameMap.TerrainChanges.Owner
---@param tilemap          Engine.Tilemap
---@param autoTileResolver fun(name: string): Engine.AutoTile
---@param layerName        string
---@param positions        sf.Vector2i[]
---@param tileID           Global.GameMap.TerrainTileID
---@param replaceLayer     Global.GameMap.TerrainEditing.ReplaceLayer
---@return sf.Vector2i[]
function TerrainEditing.SetTiles(owner, tilemap, autoTileResolver, layerName, positions, tileID, replaceLayer) end

--- Replay saved changes through the owner's single-cell terrain command.
---@param owner               Global.GameMap.TerrainChanges.Owner
---@param terrainDestructions table<string, table<string, { position: sf.Vector2i, tileID: Global.GameMap.TerrainTileID }>>
function TerrainEditing.ApplyDestructions(owner, terrainDestructions) end

return TerrainEditing
