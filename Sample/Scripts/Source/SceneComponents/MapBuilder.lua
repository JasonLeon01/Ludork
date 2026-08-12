local cjson = require("cjson")
local Engine = require("Engine")
local GameMap = require("Global.GameMap")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local Data = require("Source.Data")
local MapPath = require("Source.MapPath")
local System = require("Source.System")
local MapClickAutoPath = require("Source.SceneComponents.MapClickAutoPath")
local MovementDangerPreviewComponent = require("Source.SceneComponents.MovementDangerPreviewComponent")
local MovementDangerState = require("Source.SceneComponents.MovementDangerState")
local PathPreviewComponent = require("Global.Components.PathPreviewComponent")
local PathRouteState = require("Global.Components.PathRouteState")

local ManagerFunctions = GlobalFunctions.Manager

local MAP_DATA_ROOT = os.path.join(".", "Data", "Maps")
local MAP_DATA_EXTENSION = ".json"
local DAMAGE_TEXT_SPEED_CURVE = "Global/DamageTextSpeed"
local objectKeyOrders = setmetatable({}, {
    __mode = "k"
})

---@param text  string
---@param index integer
---@return integer
local function skipWhitespace(text, index)
    while index <= #text and text:sub(index, index):match("%s") do
        index = index + 1
    end
    return index
end

---@param text  string
---@param index integer
---@return integer
local function scanJSONString(text, index)
    assert(text:sub(index, index) == '"', "Invalid JSON string")
    index = index + 1
    while index <= #text do
        local character = text:sub(index, index)
        if character == "\\" then
            index = index + 2
        elseif character == '"' then
            return index
        else
            index = index + 1
        end
    end
    error("Unterminated JSON string")
end

---@param text  string
---@param index integer
---@return integer
local function scanJSONContainer(text, index)
    local opening = text:sub(index, index)
    local stack = { opening == "{" and "}" or "]" }
    index = index + 1
    while index <= #text do
        local character = text:sub(index, index)
        if character == '"' then
            index = scanJSONString(text, index) + 1
        elseif character == "{" then
            stack[#stack + 1] = "}"
            index = index + 1
        elseif character == "[" then
            stack[#stack + 1] = "]"
            index = index + 1
        elseif character == stack[#stack] then
            stack[#stack] = nil
            if not bool(stack) then
                return index
            end
            index = index + 1
        else
            index = index + 1
        end
    end
    error("Unterminated JSON container")
end

---@param text  string
---@param index integer
---@return integer
local function scanJSONValue(text, index)
    index = skipWhitespace(text, index)
    local character = text:sub(index, index)
    if character == '"' then
        return scanJSONString(text, index)
    end
    if character == "{" or character == "[" then
        return scanJSONContainer(text, index)
    end
    while index <= #text do
        character = text:sub(index, index)
        if character == "," or character == "}" or character == "]" then
            return index - 1
        end
        index = index + 1
    end
    return #text
end

---@param text      string
---@param fieldName string
---@param values    table<string, table>
---@return string[]
local function extractObjectKeyOrder(text, fieldName, values)
    local _, objectStart = text:find('"' .. fieldName .. '"%s*:%s*{')
    if objectStart == nil then
        return {}
    end
    local result = {}
    local seen = {}
    local index = objectStart + 1
    while index <= #text do
        index = skipWhitespace(text, index)
        local character = text:sub(index, index)
        if character == "}" then
            break
        end
        if character == "," then
            index = skipWhitespace(text, index + 1)
        end
        if text:sub(index, index) ~= '"' then
            return {}
        end
        local keyEnd = scanJSONString(text, index)
        local key = cjson.decode(text:sub(index, keyEnd))
        index = skipWhitespace(text, keyEnd + 1)
        if text:sub(index, index) ~= ":" then
            return {}
        end
        index = skipWhitespace(text, index + 1)
        local valueEnd = scanJSONValue(text, index)
        if values[key] ~= nil and not seen[key] then
            result[#result + 1] = key
            seen[key] = true
        end
        index = valueEnd + 1
    end
    return result
end

---@param values table<string, table>
---@return string[]
local function orderedKeys(values)
    local result = {}
    local seen = {}
    for _, key in ipairs(objectKeyOrders[values] or {}) do
        if values[key] ~= nil and not seen[key] then
            result[#result + 1] = key
            seen[key] = true
        end
    end
    local remaining = {}
    for key in pairs(values) do
        if not seen[key] then
            remaining[#remaining + 1] = key
        end
    end
    table.sort(remaining, function (left, right)
        return tostring(left) < tostring(right)
    end)
    for _, key in ipairs(remaining) do
        result[#result + 1] = key
    end
    return result
end

---@param data table
local function normaliseMapData(data)
    local ambientLight = data.ambientLight or { 255, 255, 255, 255 }
    data.ambientLight = sf.Color.new(table.unpack(ambientLight))
    local lights = {}
    for index, lightData in ipairs(data.lights or {}) do
        lights[index] = GlobalCore.Light.fromDict(lightData)
    end
    data.lights = lights
    ---@type table<string, Source.Data.SerializedActorData[]>
    local serializedActors = data.actors or {}
    ---@type table<string, Source.Data.ActorData[]>
    local actors = {}
    data.actors = actors
    for layerName, actorDatas in pairs(serializedActors) do
        local normalisedActorDatas = {}
        actors[layerName] = normalisedActorDatas
        for index, actorData in ipairs(actorDatas) do
            local position = actorData.position or { 0, 0 }
            local x = position[1] or 0
            local y = position[2] or 0
            normalisedActorDatas[index] = {
                bp = actorData.bp,
                tag = actorData.tag,
                position = sf.Vector2u.new(x, y)
            }
        end
    end
end

---@class Source.SceneComponents.SceneMapBuilder
local SceneMapBuilder = {}

function SceneMapBuilder:init()
    self._floorMapPreviewGameMaps = {}
end

function SceneMapBuilder:clearFloorMapPreviewCache()
    self._floorMapPreviewGameMaps = {}
end

---@diagnostic disable-next-line: unused
function SceneMapBuilder:resolveMapPath(mapPath, currentMap)
    local candidates = SceneMapBuilder._getMapPathCandidates(mapPath, currentMap)
    for _, candidate in ipairs(candidates) do
        if Engine.jsonExists(SceneMapBuilder.getMapDataPath(candidate)) then
            return candidate
        end
    end
    if bool(candidates) then
        return candidates[1]
    end
    return MapPath.Normalise(mapPath)
end

function SceneMapBuilder:loadMapData(mapPath, currentMap)
    local resolvedPath = self:resolveMapPath(mapPath, currentMap)
    local _, extension = os.path.splitext(resolvedPath)
    assert(extension:lower() == MAP_DATA_EXTENSION, "Unsupported map data format: " .. resolvedPath)
    local text = Engine.getJSONText(SceneMapBuilder.getMapDataPath(resolvedPath))
    local data = cjson.decode(text)
    ---@cast data table
    normaliseMapData(data)
    ---@cast data Source.SceneComponents.MapData
    objectKeyOrders[data.layers] = extractObjectKeyOrder(text, "layers", data.layers)
    objectKeyOrders[data.actors] = extractObjectKeyOrder(text, "actors", data.actors)
    return resolvedPath, data
end

function SceneMapBuilder.getMapDataPath(mapPath)
    return os.path.join(MAP_DATA_ROOT, MapPath.Normalise(mapPath))
end

---@param mapPath    string
---@param currentMap string | nil
---@return string[]
function SceneMapBuilder._getMapPathCandidates(mapPath, currentMap)
    mapPath = MapPath.Normalise(mapPath)
    if not bool(mapPath) then
        return {}
    end
    local candidates = {}
    local seen = {}
    ---@param candidate string
    local function append(candidate)
        if not seen[candidate] then
            seen[candidate] = true
            candidates[#candidates + 1] = candidate
        end
    end
    local stem, extension = os.path.splitext(mapPath)
    extension = extension:lower()
    if bool(extension) then
        append(mapPath)
        if extension == MAP_DATA_EXTENSION then
            append(stem .. MAP_DATA_EXTENSION)
        end
    else
        local _, currentExtension = os.path.splitext(MapPath.Normalise(currentMap or System.getStartMap()))
        if currentExtension:lower() == MAP_DATA_EXTENSION then
            append(mapPath .. currentExtension:lower())
        end
        append(mapPath .. MAP_DATA_EXTENSION)
    end
    return candidates
end

function SceneMapBuilder.generateTilemap(data, width, height)
    local mapWidth = width
    local mapHeight = height
    local mapLayers = {}
    for _, layerKey in ipairs(orderedKeys(data)) do
        local layerData = data[layerKey]
        local name = layerData.layerName
        local layerTileset = Data.getTileset(layerData.layerTileset)
        local layerTiles = layerData.tiles
        local tiles = {}
        for y = 1, mapHeight do
            local row = { n = mapWidth }
            for x = 1, mapWidth do
                local tile = layerTiles[y][x]
                if tile ~= cjson.null then
                    row[x] = tile
                end
            end
            tiles[y] = row
        end

        local rawAutoTiles = layerData.autoTiles
        local autoTilePool = {}
        local autoTileIndexByKey = {}
        local autoTileGrid = {}
        if rawAutoTiles ~= nil then
            for y = 1, mapHeight do
                local row = { n = mapWidth }
                local rawRow = rawAutoTiles[y]
                for x = 1, mapWidth do
                    local cell = rawRow ~= nil and rawRow[x] or nil
                    local cellIndex = type(cell) == "number" and math.tointeger(cell) or nil
                    if type(cell) == "string" and bool(cell) and Data.hasAutoTile(cell) then
                        if autoTileIndexByKey[cell] == nil then
                            autoTilePool[#autoTilePool + 1] = Data.getAutoTile(cell)
                            autoTileIndexByKey[cell] = #autoTilePool - 1
                        end
                        row[x] = autoTileIndexByKey[cell]
                    elseif cellIndex ~= nil and cellIndex >= 0 and cellIndex < #autoTilePool then
                        row[x] = cellIndex
                    end
                end
                autoTileGrid[y] = row
            end
        else
            for y = 1, mapHeight do
                local row = { n = mapWidth }
                autoTileGrid[y] = row
            end
        end

        local autoTileKeys = {}
        for key, index in pairs(autoTileIndexByKey) do
            autoTileKeys[index + 1] = key
        end
        local tileLayerData = Engine.TileLayerData.new(
            name, layerTileset, tiles, autoTileGrid, autoTilePool, autoTileKeys, tostring(layerData.shaderPath or "")
        )
        local autoTileTextures = {}
        local autoTileFrameCounts = {}
        for index, entry in ipairs(autoTilePool) do
            local texture = ManagerFunctions.loadAutotile(entry.fileName)
            autoTileTextures[index] = texture
            local size = texture:getSize()
            local frames = Engine.CellSize > 0 and math.floor(size.x / (3 * Engine.CellSize)) or 1
            autoTileFrameCounts[index] = math.max(1, frames)
        end
        mapLayers[#mapLayers + 1] = Engine.TileLayer.new(
            tileLayerData, ManagerFunctions.loadTileset(tileLayerData.layerTileset.fileName), autoTileTextures,
            autoTileFrameCounts
        )
    end
    return Engine.Tilemap.new(mapLayers)
end

---@diagnostic disable-next-line: unused
function SceneMapBuilder:generateGameMap(data, camera, emitCreateEvents, previewOnly)
    if emitCreateEvents == nil then
        emitCreateEvents = true
    end
    local tilemap = SceneMapBuilder.generateTilemap(data.layers, data.width, data.height)
    local result = GameMap.new(data.mapName, tilemap, camera, previewOnly)
    result:setAutoTileResolver(Data.getAutoTile)
    if not previewOnly then
        local pathRouteState = PathRouteState.new()
        local movementDangerState = MovementDangerState.new(result)
        result:addComponent(movementDangerState)
        result:addComponent(MovementDangerPreviewComponent.new(result, movementDangerState))
        result:addComponent(MapClickAutoPath.new(result, pathRouteState, movementDangerState))
        result:addComponent(PathPreviewComponent.new(result, pathRouteState))
        result:setDamageTextSpeedCurve(Data.getCurve(DAMAGE_TEXT_SPEED_CURVE))
        result:setAmbientLight(data.ambientLight)
        for _, light in ipairs(data.lights) do
            result:addLight(light)
        end
    end
    local classVarChanges = data.BPClassVarChanged
    local actors = data.actors
    result:beginActorBatch()
    for _, layerName in ipairs(orderedKeys(actors)) do
        local actorDatas = actors[layerName]
        for _, actorData in ipairs(actorDatas) do
            local actorChanges = nil
            if classVarChanges ~= nil then
                actorChanges = classVarChanges[tostring(actorData.tag or "")]
            end
            local actor = Data.genActorFromData(actorData, layerName, actorChanges)
            if actor ~= nil then
                result:spawnActor(actor, layerName, false)
            end
        end
    end
    result:endActorBatch()
    if emitCreateEvents then
        result:initialiseActorsAndComponents()
    end
    return result
end

---@diagnostic disable-next-line: unused
function SceneMapBuilder:applyAddedActors(gameMap, addedActors, emitCreateEvents)
    if not bool(addedActors) then
        return
    end
    if emitCreateEvents == nil then
        emitCreateEvents = true
    end
    local addedAny = false
    for _, actorRecord in ipairs(addedActors) do
        if gameMap:getActorByTag(actorRecord.tag) == nil then
            local actor = Data.genActorFromClassPath(actorRecord.bp, actorRecord.tag, actorRecord.classVarChanges)
            if actor ~= nil then
                actor:setMapPosition(actorRecord.position)
                gameMap:spawnActor(actor, actorRecord.layer, false)
                addedAny = true
            end
        end
    end
    if addedAny and emitCreateEvents then
        gameMap:initialiseActorsAndComponents()
    end
end

function SceneMapBuilder:buildFloorMapPreview(
    inst, currentMap, mapKey, telepoint, previewSize, previewScale, showTelepointMarker
)
    local mapPath = self:resolveMapPath(mapKey, currentMap)
    local cachedPreview = self._floorMapPreviewGameMaps[mapPath]
    if cachedPreview == nil then
        local resolvedPath, mapData = self:loadMapData(mapPath, currentMap)
        mapPath = resolvedPath
        local gameMap = self:generateGameMap(mapData, nil, false, true)
        gameMap:applyTerrainDestructions(inst:getTerrainDestructions(mapPath))
        self:applyAddedActors(gameMap, inst:getAddedActors(mapPath), false)
        gameMap:applyActorPositions(inst:getActorPositions(mapPath))
        gameMap:removeActorsByTags(inst:getDestroyedActors(mapPath))
        cachedPreview = { gameMap = gameMap, mapData = mapData }
        self._floorMapPreviewGameMaps[mapPath] = cachedPreview
    end
    local gameMap = cachedPreview.gameMap
    local mapData = cachedPreview.mapData
    local scale = previewScale > 0.0 and previewScale or 1.0
    local targetSize = sf.Vector2u.new(previewSize, previewSize)
    ---@cast targetSize sf.Vector2u
    local target = sf.RenderTexture.new(targetSize)
    target:clear(sf.Color.Transparent)
    local viewSize = sf.Vector2f.new(previewSize / scale, previewSize / scale)
    local mapPixelSize = sf.Vector2f.new(mapData.width * Engine.CellSize, mapData.height * Engine.CellSize)
    local centre = sf.Vector2f.new(
        mapPixelSize.x >= viewSize.x and viewSize.x / 2.0 or mapPixelSize.x / 2.0,
        mapPixelSize.y >= viewSize.y and viewSize.y / 2.0 or mapPixelSize.y / 2.0
    )
    local telepointCentre = sf.Vector2f.new(
        (telepoint.x + 0.5) * Engine.CellSize, (telepoint.y + 0.5) * Engine.CellSize
    )
    local halfView = viewSize / 2.0
    if telepointCentre.x < centre.x - halfView.x or telepointCentre.x > centre.x + halfView.x
        or telepointCentre.y < centre.y - halfView.y or telepointCentre.y > centre.y + halfView.y then
        centre = telepointCentre
    end
    target:setView(sf.View.new(centre, viewSize))
    local states = Engine.CanvasRenderStates()
    gameMap:drawMapContent(target, states)
    if showTelepointMarker then
        local marker = sf.RectangleShape.new(sf.Vector2f.new(Engine.CellSize, Engine.CellSize))
        marker:setPosition(sf.Vector2f.new(telepoint.x * Engine.CellSize, telepoint.y * Engine.CellSize))
        marker:setFillColor(sf.Color.new(0, 255, 0, 64))
        marker:setOutlineColor(sf.Color.new(0, 255, 0, 255))
        marker:setOutlineThickness(2.0)
        target:draw(marker, states)
    end
    target:display()
    local result = sf.Texture.new(target:getTexture():copyToImage())
    result:setSmooth(false)
    return result
end

function SceneMapBuilder:getFloorTelepointTag(currentMap, mapKey, telepoint)
    local _, mapData = self:loadMapData(mapKey, currentMap)
    local teleporterPrefix = "Data.Blueprints.Teleportations"
    local actors = mapData.actors or {}
    for _, layerName in ipairs(orderedKeys(actors)) do
        local actorDatas = actors[layerName]
        for _, actorData in ipairs(actorDatas) do
            local position = actorData.position
            if position.x == telepoint.x and position.y == telepoint.y
                and tostring(actorData.bp or ""):sub(1, #teleporterPrefix) == teleporterPrefix then
                return tostring(actorData.tag or "")
            end
        end
    end
    return nil
end

function SceneMapBuilder:resolveRegionMapPath(mapKey, currentMap)
    return self:resolveMapPath(mapKey, currentMap)
end

return class(SceneMapBuilder)
