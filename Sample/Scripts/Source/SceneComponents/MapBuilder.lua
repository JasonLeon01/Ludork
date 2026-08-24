local cjson = require("cjson")
local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local GameMap = require("Global.GameMap")
local PathPreviewComponent = require("Global.Components.PathPreviewComponent")
local PathRouteState = require("Global.Components.PathRouteState")
local Data = require("Source.Data")
local MapPath = require("Source.MapPath")
local System = require("Source.System")
local MapClickAutoPath = require("Source.SceneComponents.MapClickAutoPath")
local MovementDangerPreviewComponent = require("Source.SceneComponents.MovementDangerPreviewComponent")
local MovementDangerState = require("Source.SceneComponents.MovementDangerState")

local ManagerFunctions = GlobalFunctions.Manager

local MAP_DATA_ROOT = os.path.join(".", "Data", "Maps")
local MAP_DATA_EXTENSION = ".json"
local DAMAGE_TEXT_CONFIG = "Global/DamageText"
local DAMAGE_TEXT_SPEED_CURVE = "Global/DamageTextSpeed"

---@param data table
local function normaliseMapData(data)
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
    local ambientLight = data.ambientLight or { 255, 255, 255, 255 }
    data.ambientLight = sf.Color.new(table.unpack(ambientLight))
    local lights = {}
    for index, lightData in ipairs(data.lights or {}) do
        lights[index] = GlobalCore.Light.fromDict(lightData)
    end
    data.lights = lights
    local serializedActors = data.actors or {}
    local actors = {}
    ---@cast serializedActors table<string, Source.Data.SerializedActorData[]>
    ---@cast actors table<string, Source.Data.ActorData[]>
    data.actors = actors
    for layerName, actorDatas in pairs(serializedActors) do
        local normalisedActorDatas = {}
        actors[layerName] = normalisedActorDatas
        for index, actorData in ipairs(actorDatas) do
            local position = actorData.position or { 0, 0 }
            local x = position[1] or 0
            local y = position[2] or 0
            normalisedActorDatas[index] = { bp = actorData.bp, tag = actorData.tag, position = sf.Vector2u.new(x, y) }
        end
    end
end

---@type function
local getMapPathCandidates
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
    local candidates = getMapPathCandidates(mapPath, currentMap)
    for _, candidate in ipairs(candidates) do
        if Engine.jsonExists(SceneMapBuilder.GetMapDataPath(candidate)) then
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
    local data = cjson.decode(Engine.getJSONText(SceneMapBuilder.GetMapDataPath(resolvedPath)))
    ---@cast data table
    normaliseMapData(data)
    ---@cast data Source.SceneComponents.MapData
    return resolvedPath, data
end

function SceneMapBuilder.GetMapDataPath(mapPath)
    return os.path.join(MAP_DATA_ROOT, MapPath.Normalise(mapPath))
end

---@param mapPath    string
---@param currentMap string | nil
---@return string[]
function getMapPathCandidates(mapPath, currentMap)
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
        local _, currentExtension = os.path.splitext(MapPath.Normalise(currentMap or System.GetStartMap()))
        if currentExtension:lower() == MAP_DATA_EXTENSION then
            append(mapPath .. currentExtension:lower())
        end
        append(mapPath .. MAP_DATA_EXTENSION)
    end
    return candidates
end

function SceneMapBuilder.GenerateTilemap(data, layerOrder, width, height)
    local mapWidth = width
    local mapHeight = height
    local mapLayers = {}
    for _, layerKey in ipairs(layerOrder) do
        local layerData = data[layerKey]
        local name = layerData.layerName
        local layerTileset = Data.GetTileset(layerData.layerTileset)
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
                    if type(cell) == "string" and bool(cell) and Data.HasAutoTile(cell) then
                        if autoTileIndexByKey[cell] == nil then
                            autoTilePool[#autoTilePool + 1] = Data.GetAutoTile(cell)
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
    local tilemap = SceneMapBuilder.GenerateTilemap(data.layers, data.layerOrder, data.width, data.height)
    local result = GameMap.new(data.mapName, tilemap, camera, previewOnly)
    result:setAutoTileResolver(Data.GetAutoTile)
    if not previewOnly then
        local pathRouteState = PathRouteState.new()
        local movementDangerState = MovementDangerState.new(result)
        result:addComponent(movementDangerState)
        result:addComponent(MovementDangerPreviewComponent.new(result, movementDangerState))
        result:addComponent(MapClickAutoPath.new(result, pathRouteState, movementDangerState))
        result:addComponent(PathPreviewComponent.new(result, pathRouteState))
        result:setDamageTextConfig(Data.GetPlainTextConfig(DAMAGE_TEXT_CONFIG))
        result:setDamageTextSpeedCurve(Data.GetCurve(DAMAGE_TEXT_SPEED_CURVE))
        result:setAmbientLight(data.ambientLight)
        for _, light in ipairs(data.lights) do
            result:addLight(light)
        end
    end
    local classVarChanges = data.BPClassVarChanged
    local actors = data.actors
    result:beginActorBatch()
    for _, layerName in ipairs(data.layerOrder) do
        local actorDatas = actors[layerName] or {}
        for _, actorData in ipairs(actorDatas) do
            local actorChanges = nil
            if classVarChanges ~= nil then
                actorChanges = classVarChanges[tostring(actorData.tag or "")]
            end
            local actor = Data.GenActorFromData(actorData, layerName, actorChanges)
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
            local actor = Data.GenActorFromClassPath(actorRecord.bp, actorRecord.tag, actorRecord.classVarChanges)
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
    if self._floorMapPreviewGameMaps[mapPath] == nil then
        local resolvedPath, mapData = self:loadMapData(mapPath, currentMap)
        mapPath = resolvedPath
        local gameMap = self:generateGameMap(mapData, nil, false, true)
        gameMap:applyTerrainDestructions(inst:getTerrainDestructions(mapPath))
        self:applyAddedActors(gameMap, inst:getAddedActors(mapPath), false)
        gameMap:applyActorPositions(inst:getActorPositions(mapPath))
        gameMap:removeActorsByTags(inst:getDestroyedActors(mapPath))
        self._floorMapPreviewGameMaps[mapPath] = { gameMap = gameMap, mapData = mapData }
    end
    local scale = previewScale > 0.0 and previewScale or 1.0
    local targetSize = sf.Vector2u.new(previewSize, previewSize)
    ---@cast targetSize sf.Vector2u
    local target = sf.RenderTexture.new(targetSize)
    target:clear(sf.Color.Transparent)
    local viewSize = sf.Vector2f.new(previewSize / scale, previewSize / scale)
    local mapPixelSize = sf.Vector2f.new(
        self._floorMapPreviewGameMaps[mapPath].mapData.width * Engine.CellSize,
        self._floorMapPreviewGameMaps[mapPath].mapData.height * Engine.CellSize
    )
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
    self._floorMapPreviewGameMaps[mapPath].gameMap:drawMapContent(target, states)
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
    for _, layerName in ipairs(mapData.layerOrder) do
        local actorDatas = actors[layerName] or {}
        for _, actorData in ipairs(actorDatas) do
            local position = actorData.position
            if position.x == telepoint.x and position.y == telepoint.y
                and string.startsWith(tostring(actorData.bp or ""), teleporterPrefix) then
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
