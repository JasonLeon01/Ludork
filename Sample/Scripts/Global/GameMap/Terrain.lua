local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local AutoTileRuntime = require("Global.GameMap.AutoTileRuntime")
local TerrainValue = require("Global.GameMap.TerrainValue")

local GameMapBase = GlobalCore.GameMapBase
local ManagerFunctions = GlobalFunctions.Manager

---@class (partial) GameMap
local GameMapTerrain = {}

---@param layerData           Engine.TileLayerData
---@param autoTileTextures    sf.Texture[]
---@param autoTileFrameCounts integer[]
local function ensureAutoTileRuntimeData(layerData, autoTileTextures, autoTileFrameCounts)
    while #autoTileTextures < #layerData.autoTilePool do
        local autoTile = layerData.autoTilePool[#autoTileTextures + 1]
        ---@cast autoTile Engine.AutoTile
        autoTileTextures[#autoTileTextures + 1] = ManagerFunctions.loadAutotile(autoTile.fileName)
    end
    while #autoTileFrameCounts < #autoTileTextures do
        local texture = autoTileTextures[#autoTileFrameCounts + 1]
        autoTileFrameCounts[#autoTileFrameCounts + 1] = AutoTileRuntime.GetFrameCount(texture)
    end
end

function GameMapTerrain:getTerrainTile(layerName, position)
    local layer = self._tilemap:getLayer(layerName)
    if layer == nil then
        return nil
    end
    if not self:_isTerrainPositionInLayer(layer, position) then
        return nil
    end
    return self:_getTerrainTileID(layer, position)
end

function GameMapTerrain:getTerrainTilePositions(layerName, tileID)
    local layer = self._tilemap:getLayer(layerName)
    if layer == nil then
        return {}
    end
    local terrainTileID = self:_normaliseTerrainTileID(tileID)
    local positions = {}
    local size = layer:getGridSize()
    for y = 0, size.y - 1 do
        for x = 0, size.x - 1 do
            local terrainPosition = sf.Vector2i.new(x, y)
            ---@cast terrainPosition sf.Vector2i
            if self:_getTerrainTileID(layer, terrainPosition) == terrainTileID then
                positions[#positions + 1] = terrainPosition
            end
        end
    end
    return positions
end

function GameMapTerrain:setTerrainTile(layerName, position, tileID)
    return bool(self:setTerrainTiles(layerName, { position }, tileID))
end

function GameMapTerrain:setTerrainTiles(layerName, positions, tileID)
    local terrainTileID = self:_normaliseTerrainTileID(tileID)
    if not bool(positions) then
        return {}
    end
    local layer = self._tilemap:getLayer(layerName)
    if not bool(layer) then
        return {}
    end
    ---@cast layer Engine.TileLayer
    local layerData = layer:getData()
    local autoTileTextures = layer:getAutoTileTextures()
    local autoTileFrameCounts = layer:getAutoTileFrameCounts()
    local changedPositions = {}
    for _, position in ipairs(positions) do
        if self:_isTerrainPositionInLayer(layer, position) then
            self:_writeTerrainTile(layer, layerData, autoTileTextures, autoTileFrameCounts, position, terrainTileID)
            changedPositions[#changedPositions + 1] = position
        end
    end
    if not bool(changedPositions) then
        return {}
    end
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

---@param tileID Global.GameMap.TerrainTileID
---@return Global.GameMap.TerrainTileID
---@diagnostic disable-next-line: unused
function GameMapTerrain:_normaliseTerrainTileID(tileID)
    return TerrainValue.Normalise(tileID)
end

---@param layer    Engine.TileLayer
---@param position sf.Vector2i
---@return boolean
---@diagnostic disable-next-line: unused
function GameMapTerrain:_isTerrainPositionInLayer(layer, position)
    local size = layer:getGridSize()
    return position.x >= 0 and position.y >= 0 and position.x < size.x and position.y < size.y
end

---@param layer    Engine.TileLayer
---@param position sf.Vector2i
---@return Global.GameMap.TerrainTileID
function GameMapTerrain:_getTerrainTileID(layer, position)
    return self:_getTerrainAutoTileID(layer, position) or layer:get(position)
end

---@param layer               Engine.TileLayer
---@param layerData           Engine.TileLayerData
---@param autoTileTextures    sf.Texture[]
---@param autoTileFrameCounts integer[]
---@param position            sf.Vector2i
---@param tileID              Global.GameMap.TerrainTileID
function GameMapTerrain:_writeTerrainTile(layer, layerData, autoTileTextures, autoTileFrameCounts, position, tileID)
    local x = position.x + 1
    local y = position.y + 1
    self:_ensureTerrainAutoTileGrid(layerData, layer:getGridSize())
    local tiles = layerData.tiles
    local autoTiles = layerData.autoTiles
    local tilesRow = tiles[y]
    local autoTilesRow = autoTiles[y]
    ---@cast tilesRow(integer | nil)[]
    ---@cast autoTilesRow Global.GameMap.TerrainTileID[]
    if tileID == nil then
        tilesRow[x] = nil
        autoTilesRow[x] = nil
    elseif type(tileID) == "string" then
        tilesRow[x] = nil
        autoTilesRow[x] = self:_resolveAutoTileIndex(layerData, autoTileTextures, autoTileFrameCounts, tileID)
    else
        if tileID < 0 or tileID >= #layerData.layerTileset.materials then
            error("Tile ID " .. tileID .. " is out of range for layer '" .. layer:getName() .. "'", 2)
        end
        tilesRow[x] = tileID
        autoTilesRow[x] = nil
    end
    layerData.tiles = tiles
    layerData.autoTiles = autoTiles
end

---@param layer    Engine.TileLayer
---@param position sf.Vector2i
---@return string | nil
---@diagnostic disable-next-line: unused
function GameMapTerrain:_getTerrainAutoTileID(layer, position)
    local autoTiles = layer:getAutoTiles()
    local row = autoTiles and autoTiles[position.y + 1] or nil
    if row == nil then
        return nil
    end
    local autoTileIndex = row[position.x + 1]
    if autoTileIndex == nil then
        return nil
    elseif type(autoTileIndex) == "string" then
        return bool(autoTileIndex) and autoTileIndex or nil
    end
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

---@param layerData Engine.TileLayerData
---@param size      sf.Vector2u
---@diagnostic disable-next-line: unused
function GameMapTerrain:_ensureTerrainAutoTileGrid(layerData, size)
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
---@param autoTileName        string
---@return integer
function GameMapTerrain:_resolveAutoTileIndex(layerData, autoTileTextures, autoTileFrameCounts, autoTileName)
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
    if self._autoTileResolver == nil then
        error("AutoTile resolver is not configured", 2)
    end
    local autoTile = self._autoTileResolver(autoTileName)
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

return class(GameMapTerrain)
