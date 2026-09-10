local GlobalCore = require("GlobalCore")
local AutoTileRuntime = require("Global.GameMap.AutoTileRuntime")
local TerrainValue = require("Global.GameMap.TerrainValue")

local TextureManager = GlobalCore.TextureManager

local TerrainOperations = {}

---@param layerData           Engine.TileLayerData
---@param autoTileTextures    sf.Texture[]
---@param autoTileFrameCounts integer[]
local function ensureAutoTileRuntimeData(layerData, autoTileTextures, autoTileFrameCounts)
    while #autoTileTextures < #layerData.autoTilePool do
        local autoTile = layerData.autoTilePool[#autoTileTextures + 1]
        ---@cast autoTile Engine.AutoTile
        autoTileTextures[#autoTileTextures + 1] = TextureManager.load(autoTile.fileName)
    end
    while #autoTileFrameCounts < #autoTileTextures do
        local texture = autoTileTextures[#autoTileFrameCounts + 1]
        autoTileFrameCounts[#autoTileFrameCounts + 1] = AutoTileRuntime.GetFrameCount(texture)
    end
end

---@param layer    Engine.TileLayer
---@param position sf.Vector2i
---@return boolean
local function isPositionInLayer(layer, position)
    local size = layer:getGridSize()
    return position.x >= 0 and position.y >= 0 and position.x < size.x and position.y < size.y
end

---@param layer    Engine.TileLayer
---@param position sf.Vector2i
---@return string | nil
local function getAutoTileID(layer, position)
    local autoTiles = layer:getAutoTiles()
    local row = autoTiles and autoTiles[position.y + 1] or nil
    if row == nil then
        return nil
    end
    local autoTileIndex = row[position.x + 1]
    if autoTileIndex == nil then
        return nil
    elseif Class.isInstance(autoTileIndex, "string") then
        ---@cast autoTileIndex string
        return bool(autoTileIndex) and autoTileIndex or nil
    end
    ---@cast autoTileIndex integer
    local autoTileKey = layer:getAutoTileKey(autoTileIndex)
    if autoTileKey ~= nil then
        return autoTileKey
    end
    local autoTilePool = layer:getAutoTilePool()
    if autoTileIndex >= 0 and autoTileIndex < #autoTilePool then
        local autoTile = autoTilePool[autoTileIndex + 1]
        ---@cast autoTile Engine.AutoTile
        return autoTile.name
    end
    return nil
end

---@param layer    Engine.TileLayer
---@param position sf.Vector2i
---@return Global.GameMap.TerrainTileID
local function getTileID(layer, position)
    return getAutoTileID(layer, position) or layer:get(position)
end

---@param layerData Engine.TileLayerData
---@param size      sf.Vector2u
local function ensureAutoTileGrid(layerData, size)
    local autoTiles = layerData.autoTiles
    if not bool(autoTiles) then
        autoTiles = {}
    end
    while #autoTiles < size.y do
        autoTiles[#autoTiles + 1] = {}
    end
    for _, row in ipairs(autoTiles) do
        ---@cast row Global.GameMap.TerrainTileID[] & { n: integer | nil }
        local rowLength = row.n
        if rowLength == nil then
            row.n = size.x
        else
            row.n = math.max(rowLength, size.x)
        end
    end
    layerData.autoTiles = autoTiles
end

---@param layerData           Engine.TileLayerData
---@param autoTileTextures    sf.Texture[]
---@param autoTileFrameCounts integer[]
---@param autoTileResolver    fun(name: string): Engine.AutoTile | nil
---@param autoTileName        string
---@return integer
local function resolveAutoTileIndex(layerData, autoTileTextures, autoTileFrameCounts, autoTileResolver, autoTileName)
    local autoTileKeys = layerData.autoTileKeys or {}
    local autoTilePool = layerData.autoTilePool
    if not bool(autoTileKeys) then
        for _, entry in ipairs(autoTilePool) do
            autoTileKeys[#autoTileKeys + 1] = entry.name
        end
        layerData.autoTileKeys = autoTileKeys
    end
    local index = table.index(autoTileKeys, autoTileName)
    if index ~= nil then
        ensureAutoTileRuntimeData(layerData, autoTileTextures, autoTileFrameCounts)
        return index - 1
    end
    if autoTileResolver == nil then
        error("AutoTile resolver is not configured", 2)
    end
    local autoTile = autoTileResolver(autoTileName)
    if autoTile == nil then
        error("Autotile '" .. autoTileName .. "' not found", 2)
    end
    autoTilePool[#autoTilePool + 1] = autoTile
    autoTileKeys[#autoTileKeys + 1] = autoTileName
    layerData.autoTilePool = autoTilePool
    layerData.autoTileKeys = autoTileKeys
    ensureAutoTileRuntimeData(layerData, autoTileTextures, autoTileFrameCounts)
    return #autoTilePool - 1
end

---@param layer               Engine.TileLayer
---@param layerData           Engine.TileLayerData
---@param autoTileTextures    sf.Texture[]
---@param autoTileFrameCounts integer[]
---@param autoTileResolver    fun(name: string): Engine.AutoTile | nil
---@param position            sf.Vector2i
---@param tileID              Global.GameMap.TerrainTileID
local function writeTile(layer, layerData, autoTileTextures, autoTileFrameCounts, autoTileResolver, position, tileID)
    local x = position.x + 1
    local y = position.y + 1
    ensureAutoTileGrid(layerData, layer:getGridSize())
    local tiles = layerData.tiles
    local autoTiles = layerData.autoTiles
    local tilesRow = tiles[y]
    local autoTilesRow = autoTiles[y]
    ---@cast tilesRow(integer | nil)[]
    ---@cast autoTilesRow Global.GameMap.TerrainTileID[]
    if tileID == nil then
        tilesRow[x] = nil
        autoTilesRow[x] = nil
    elseif Class.isInstance(tileID, "string") then
        ---@cast tileID string
        tilesRow[x] = nil
        autoTilesRow[x] = resolveAutoTileIndex(
            layerData, autoTileTextures, autoTileFrameCounts, autoTileResolver, tileID
        )
    else
        ---@cast tileID integer
        if tileID < 0 or tileID >= #layerData.layerTileset.materials then
            error("Tile ID " .. tileID .. " is out of range for layer '" .. layer:getName() .. "'", 2)
        end
        tilesRow[x] = tileID
        autoTilesRow[x] = nil
    end
    layerData.tiles = tiles
    layerData.autoTiles = autoTiles
end

---@param tilemap   Engine.Tilemap
---@param layerName string
---@param position  sf.Vector2i
---@return Global.GameMap.TerrainTileID
function TerrainOperations.GetTile(tilemap, layerName, position)
    local layer = tilemap:getLayer(layerName)
    if layer == nil or not isPositionInLayer(layer, position) then
        return nil
    end
    return getTileID(layer, position)
end

---@param tilemap   Engine.Tilemap
---@param layerName string
---@param tileID    Global.GameMap.TerrainTileID
---@return sf.Vector2i[]
function TerrainOperations.GetTilePositions(tilemap, layerName, tileID)
    local layer = tilemap:getLayer(layerName)
    if layer == nil then
        return {}
    end
    local terrainTileID = TerrainValue.Normalise(tileID)
    local positions = {}
    local size = layer:getGridSize()
    for y = 0, size.y - 1 do
        for x = 0, size.x - 1 do
            local terrainPosition = sf.Vector2i.new(x, y)
            ---@cast terrainPosition sf.Vector2i
            if getTileID(layer, terrainPosition) == terrainTileID then
                positions[#positions + 1] = terrainPosition
            end
        end
    end
    return positions
end

---@param tilemap          Engine.Tilemap
---@param autoTileResolver fun(name: string): Engine.AutoTile | nil
---@param layerName        string
---@param positions        sf.Vector2i[]
---@param tileID           Global.GameMap.TerrainTileID
---@return sf.Vector2i[] changedPositions
---@return Engine.TileLayer | nil layer
---@return Engine.TileLayerData | nil layerData
---@return sf.Texture[] | nil autoTileTextures
---@return integer[] | nil autoTileFrameCounts
function TerrainOperations.SetTiles(tilemap, autoTileResolver, layerName, positions, tileID)
    local terrainTileID = TerrainValue.Normalise(tileID)
    if not bool(positions) then
        return {}
    end
    local layer = tilemap:getLayer(layerName)
    if not bool(layer) then
        return {}
    end
    ---@cast layer Engine.TileLayer
    local layerData = layer:getData()
    local autoTileTextures = layer:getAutoTileTextures()
    local autoTileFrameCounts = layer:getAutoTileFrameCounts()
    local changedPositions = {}
    for _, position in ipairs(positions) do
        if isPositionInLayer(layer, position) then
            writeTile(
                layer, layerData, autoTileTextures, autoTileFrameCounts, autoTileResolver, position, terrainTileID
            )
            changedPositions[#changedPositions + 1] = position
        end
    end
    if not bool(changedPositions) then
        return {}
    end
    return changedPositions, layer, layerData, autoTileTextures, autoTileFrameCounts
end

return TerrainOperations
