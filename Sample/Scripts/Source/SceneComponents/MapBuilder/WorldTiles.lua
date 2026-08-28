local cjson = require("cjson")
local TerrainValue = require("Global.GameMap.TerrainValue")
local WorldGeometry = require("Global.WorldGeometry")
local WorldMapConstants = require("Global.WorldMapConstants")

local WORLD_REGION_BUILD_CHUNK_SIZE = WorldMapConstants.REGION_BUILD_CHUNK_SIZE
local WORLD_TILE_GRAPHICS_CHUNK_SIZE = WorldMapConstants.SPATIAL_CHUNK_SIZE

---@class (partial) Source.SceneComponents.SceneMapBuilder
local MapBuilderWorldTiles = {}

---@param region       Source.SceneComponents.WorldRegionData
---@param width        integer
---@param height       integer
---@param priorityRect Global.WorldGeometry.CellRect | nil
---@return Global.WorldGeometry.CellRect[], Global.WorldGeometry.CellRect[], integer
---@diagnostic disable-next-line: unused
function MapBuilderWorldTiles:_createWorldRegionChunks(region, width, height, priorityRect)
    ---@type Global.WorldGeometry.CellRect[]
    local rowMajor = {}
    for y = 0, height - 1, WORLD_REGION_BUILD_CHUNK_SIZE do
        for x = 0, width - 1, WORLD_REGION_BUILD_CHUNK_SIZE do
            rowMajor[#rowMajor + 1] = {
                x = x,
                y = y,
                width = math.min(WORLD_REGION_BUILD_CHUNK_SIZE, width - x),
                height = math.min(WORLD_REGION_BUILD_CHUNK_SIZE, height - y)
            }
        end
    end
    local priority = copy(rowMajor)
    local priorityCount = #priority
    if priorityRect ~= nil then
        local localLeft = math.max(0, priorityRect.x - region.x)
        local localTop = math.max(0, priorityRect.y - region.y)
        local localRight = math.min(width, priorityRect.x + priorityRect.width - region.x)
        local localBottom = math.min(height, priorityRect.y + priorityRect.height - region.y)
        local expandedLeft = math.max(
            0,
            math.floor(localLeft / WORLD_TILE_GRAPHICS_CHUNK_SIZE) * WORLD_TILE_GRAPHICS_CHUNK_SIZE - WORLD_REGION_BUILD_CHUNK_SIZE
        )
        local expandedTop = math.max(
            0,
            math.floor(localTop / WORLD_TILE_GRAPHICS_CHUNK_SIZE) * WORLD_TILE_GRAPHICS_CHUNK_SIZE - WORLD_REGION_BUILD_CHUNK_SIZE
        )
        local expandedRight = math.min(
            width,
            math.ceil(localRight / WORLD_TILE_GRAPHICS_CHUNK_SIZE) * WORLD_TILE_GRAPHICS_CHUNK_SIZE + WORLD_REGION_BUILD_CHUNK_SIZE
        )
        local expandedBottom = math.min(
            height,
            math.ceil(localBottom / WORLD_TILE_GRAPHICS_CHUNK_SIZE) * WORLD_TILE_GRAPHICS_CHUNK_SIZE + WORLD_REGION_BUILD_CHUNK_SIZE
        )
        local expandedPriority = {
            x = region.x + expandedLeft,
            y = region.y + expandedTop,
            width = math.max(0, expandedRight - expandedLeft),
            height = math.max(0, expandedBottom - expandedTop)
        }
        local centreX = priorityRect.x + priorityRect.width / 2
        local centreY = priorityRect.y + priorityRect.height / 2
        ---@type table<Global.WorldGeometry.CellRect, Global.WorldGeometry.CellRect>
        local worldChunks = {}
        for _, chunk in ipairs(rowMajor) do
            worldChunks[chunk] = {
                x = region.x + chunk.x,
                y = region.y + chunk.y,
                width = chunk.width,
                height = chunk.height
            }
        end
        table.sort(priority, function (left, right)
            local leftWorld = worldChunks[left]
            local rightWorld = worldChunks[right]
            local leftVisible = WorldGeometry.RectIntersects(leftWorld, expandedPriority)
            local rightVisible = WorldGeometry.RectIntersects(rightWorld, expandedPriority)
            if leftVisible ~= rightVisible then
                return leftVisible
            end
            local leftDx = leftWorld.x + left.width / 2 - centreX
            local leftDy = leftWorld.y + left.height / 2 - centreY
            local rightDx = rightWorld.x + right.width / 2 - centreX
            local rightDy = rightWorld.y + right.height / 2 - centreY
            local leftDistance = leftDx * leftDx + leftDy * leftDy
            local rightDistance = rightDx * rightDx + rightDy * rightDy
            if leftDistance ~= rightDistance then
                return leftDistance < rightDistance
            end
            if left.y ~= right.y then
                return left.y < right.y
            end
            return left.x < right.x
        end)
        priorityCount = 0
        for _, chunk in ipairs(priority) do
            if WorldGeometry.RectIntersects(worldChunks[chunk], expandedPriority) then
                priorityCount = priorityCount + 1
            else
                break
            end
        end
    end
    return rowMajor, priority, priorityCount
end

---@param region       Source.SceneComponents.WorldRegionData
---@param width        integer
---@param height       integer
---@param priorityRect Global.WorldGeometry.CellRect | nil
---@return Global.WorldGeometry.CellRect[], integer
---@diagnostic disable-next-line: unused
function MapBuilderWorldTiles:_createWorldTileGraphicsChunks(region, width, height, priorityRect)
    ---@type Global.WorldGeometry.CellRect[]
    local chunks = {}
    ---@type Global.WorldGeometry.CellRect[]
    local priority = {}
    ---@type Global.WorldGeometry.CellRect[]
    local remaining = {}
    for y = 0, height - 1, WORLD_TILE_GRAPHICS_CHUNK_SIZE do
        for x = 0, width - 1, WORLD_TILE_GRAPHICS_CHUNK_SIZE do
            local chunk = {
                x = x,
                y = y,
                width = math.min(WORLD_TILE_GRAPHICS_CHUNK_SIZE, width - x),
                height = math.min(WORLD_TILE_GRAPHICS_CHUNK_SIZE, height - y)
            }
            local worldChunk = { x = region.x + x, y = region.y + y, width = chunk.width, height = chunk.height }
            local intersectsPriority = priorityRect == nil or WorldGeometry.RectIntersects(worldChunk, priorityRect)
            if intersectsPriority then
                priority[#priority + 1] = chunk
            else
                remaining[#remaining + 1] = chunk
            end
        end
    end
    for _, chunk in ipairs(priority) do
        chunks[#chunks + 1] = chunk
    end
    for _, chunk in ipairs(remaining) do
        chunks[#chunks + 1] = chunk
    end
    return chunks, #priority
end

---@param data                Source.SceneComponents.SerializedMapData
---@param terrainDestructions table<string, table<string, Source.GameInstance.TerrainChangeRecord>>
---@param yieldStep           fun()
---@return table<string, table<integer, table<integer, Source.SceneComponents.WorldTerrainOverride>>>
---@async
---@diagnostic disable-next-line: unused
function MapBuilderWorldTiles:_createWorldTerrainOverrides(data, terrainDestructions, yieldStep)
    local result = {}
    local mapBounds = { x = 0, y = 0, width = data.width, height = data.height }
    for layerName, changes in pairs(terrainDestructions) do
        if data.layers[layerName] ~= nil then
            local rows = {}
            local pending = 0
            result[layerName] = rows
            for _, change in pairs(changes) do
                local position = change.position
                if WorldGeometry.RectContainsPosition(mapBounds, position) then
                    local row = rows[position.y + 1] or {}
                    rows[position.y + 1] = row
                    row[position.x + 1] = { tileID = TerrainValue.Normalise(change.tileID) }
                end
                pending = pending + 1
                if pending >= WORLD_REGION_BUILD_CHUNK_SIZE * WORLD_REGION_BUILD_CHUNK_SIZE then
                    pending = 0
                    yieldStep()
                end
            end
        end
    end
    return result
end

---@param actorData Source.Data.SerializedActorData
---@return Source.Data.ActorData
---@diagnostic disable-next-line: unused
function MapBuilderWorldTiles:_normaliseActorData(actorData)
    local position = actorData.position or { 0, 0 }
    local x = position[1] or 0
    local y = position[2] or 0
    local actorPosition = sf.Vector2u.new(x, y)
    ---@cast actorPosition sf.Vector2u
    return { bp = actorData.bp, tag = actorData.tag, position = actorPosition }
end

---@param data Source.SceneComponents.SerializedMapData
---@diagnostic disable-next-line: unused
function MapBuilderWorldTiles:_validateIncrementalMapData(data)
    assert(type(data.layerOrder) == "table", "Map layerOrder must be an array")
    assert(type(data.layers) == "table", "Map layers must be an object")
    local seenLayers = {}
    for _, layerName in ipairs(data.layerOrder) do
        assert(data.layers[layerName] ~= nil, "Map layerOrder references a missing layer: " .. layerName)
        assert(not seenLayers[layerName], "Map layerOrder contains a duplicate layer: " .. layerName)
        seenLayers[layerName] = true
    end
    for layerName in pairs(data.layers) do
        assert(seenLayers[layerName], "Map layer is missing from layerOrder: " .. layerName)
    end
    for layerName in pairs(data.actors or {}) do
        assert(seenLayers[layerName], "Map actor group is missing from layerOrder: " .. layerName)
    end
end

---@param layerState Source.SceneComponents.WorldLayerBuildState
---@param chunk      Global.WorldGeometry.CellRect
---@diagnostic disable-next-line: unused
function MapBuilderWorldTiles:_writeWorldLayerDataChunk(layerState, chunk)
    local chunkKey = WorldGeometry.GridKey(chunk.x, chunk.y)
    if layerState.writtenDataChunks[chunkKey] then
        return
    end
    local tileBlock = layerState.tileBlock
    local autoTileBlock = layerState.autoTileBlock
    tileBlock.n = chunk.height
    autoTileBlock.n = chunk.height
    for blockY = 1, WORLD_REGION_BUILD_CHUNK_SIZE do
        local tileRow = tileBlock[blockY]
        local autoTileRow = autoTileBlock[blockY]
        local rowWidth = blockY <= chunk.height and chunk.width or 0
        tileRow.n = rowWidth
        autoTileRow.n = rowWidth
        for blockX = 1, WORLD_REGION_BUILD_CHUNK_SIZE do
            tileRow[blockX] = nil
            autoTileRow[blockX] = nil
        end
    end
    for blockY = 1, chunk.height do
        local y = chunk.y + blockY
        local tileRow = tileBlock[blockY]
        local autoTileRow = autoTileBlock[blockY]
        local rawTileRow = assert(layerState.layerData.tiles[y], "Map tile grid row is missing")
        local rawAutoTileRow = layerState.rawAutoTiles ~= nil and layerState.rawAutoTiles[y] or nil
        local terrainOverrideRow = layerState.layerTerrainOverrides[y]
        for blockX = 1, chunk.width do
            local x = chunk.x + blockX
            local terrainOverride = terrainOverrideRow ~= nil and terrainOverrideRow[x] or nil
            if terrainOverride ~= nil then
                local tileID = terrainOverride.tileID
                if type(tileID) == "string" then
                    autoTileRow[blockX] = layerState.autoTileIndexByKey[tileID]
                elseif tileID ~= nil then
                    if tileID < 0 or tileID >= #layerState.layerTileset.materials then
                        error("Tile ID " .. tileID
                                .. " is out of range for layer '" .. layerState.layerData.layerName
                                .. "'", 2)
                    end
                    tileRow[blockX] = tileID
                end
            else
                local tile = rawTileRow[x]
                if tile ~= nil and tile ~= cjson.null then
                    tileRow[blockX] = tile
                end
                local cell = rawAutoTileRow ~= nil and rawAutoTileRow[x] or nil
                local cellIndex = type(cell) == "number" and math.tointeger(cell) or nil
                if type(cell) == "string" and layerState.autoTileIndexByKey[cell] ~= nil then
                    autoTileRow[blockX] = layerState.autoTileIndexByKey[cell]
                elseif cellIndex ~= nil and cellIndex >= 0 and cellIndex < layerState.autoTilePoolSize then
                    autoTileRow[blockX] = cellIndex
                end
            end
        end
    end
    layerState.tileLayer:writeBlock(chunk.x, chunk.y, tileBlock, autoTileBlock)
    layerState.writtenDataChunks[chunkKey] = true
end

---@param layerState Source.SceneComponents.WorldLayerBuildState
---@param chunk      Global.WorldGeometry.CellRect
---@param deadline   number
---@return boolean
function MapBuilderWorldTiles:_prepareWorldLayerNativeChunk(layerState, chunk, deadline)
    local firstX = math.max(
        0,
        math.floor(chunk.x / WORLD_REGION_BUILD_CHUNK_SIZE) * WORLD_REGION_BUILD_CHUNK_SIZE - WORLD_REGION_BUILD_CHUNK_SIZE
    )
    local firstY = math.max(
        0,
        math.floor(chunk.y / WORLD_REGION_BUILD_CHUNK_SIZE) * WORLD_REGION_BUILD_CHUNK_SIZE - WORLD_REGION_BUILD_CHUNK_SIZE
    )
    local lastX = math.min(layerState.width - 1, chunk.x + chunk.width - 1 + WORLD_REGION_BUILD_CHUNK_SIZE)
    local lastY = math.min(layerState.height - 1, chunk.y + chunk.height - 1 + WORLD_REGION_BUILD_CHUNK_SIZE)
    for y = firstY, lastY, WORLD_REGION_BUILD_CHUNK_SIZE do
        for x = firstX, lastX, WORLD_REGION_BUILD_CHUNK_SIZE do
            local chunkKey = WorldGeometry.GridKey(x, y)
            local dataChunk = layerState.dataChunksByKey[chunkKey]
            if dataChunk ~= nil and not layerState.writtenDataChunks[chunkKey] then
                if perfCounter() >= deadline then
                    return false
                end
                self:_writeWorldLayerDataChunk(layerState, dataChunk)
            end
        end
    end
    return true
end

return class(MapBuilderWorldTiles)
