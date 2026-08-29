local cjson = require("cjson")
local Engine = require("Engine")
require("GlobalCore")
local GlobalFunctions = require("GlobalFunctions")
local GameMap = require("Global.GameMap")
local AutoTileRuntime = require("Global.GameMap.AutoTileRuntime")
local WorldGeometry = require("Global.WorldGeometry")
local PathPreviewComponent = require("Global.Components.PathPreviewComponent")
local PathRouteState = require("Global.Components.PathRouteState")
local Data = require("Source.Data")
local MapPath = require("Source.MapPath")
local MapClickAutoPath = require("Source.SceneComponents.MapClickAutoPath")
local MovementDangerPreviewComponent = require("Source.SceneComponents.MovementDangerPreviewComponent")
local MovementDangerState = require("Source.SceneComponents.MovementDangerState")
local WorldDirectoryValidator = require("Source.SceneComponents.WorldDirectoryValidator")
local MapDataParser = require("Source.SceneComponents.MapBuilder.MapDataParser")
local MapBuilderWorldActors = require("Source.SceneComponents.MapBuilder.WorldActors")
local MapBuilderWorldRegion = require("Source.SceneComponents.MapBuilder.WorldRegion")
local MapBuilderWorldTiles = require("Source.SceneComponents.MapBuilder.WorldTiles")

local ManagerFunctions = GlobalFunctions.Manager

local DAMAGE_TEXT_CONFIG = "Global/DamageText"
local DAMAGE_TEXT_SPEED_CURVE = "Global/DamageTextSpeed"
---@class (partial) Source.SceneComponents.SceneMapBuilder
local SceneMapBuilder = {}

---@alias SceneMapBuilderImplState Source.SceneComponents.SceneMapBuilder

---@param data integer[] | nil
---@return sf.Color
function SceneMapBuilder.BuildAmbientLight(data)
    local values = data or { 255, 255, 255, 255 }
    ---@diagnostic disable-next-line: param-type-mismatch, the map schema supplies three or four colour channels
    return sf.Color.new(table.unpack(values))
end

function SceneMapBuilder:init()
    self._floorMapPreviewGameMaps = {}
end

function SceneMapBuilder:clearFloorMapPreviewCache()
    self._floorMapPreviewGameMaps = {}
end

---@diagnostic disable-next-line: unused
function SceneMapBuilder:resolveMapPath(mapPath, currentMap)
    local candidates = MapDataParser.GetPathCandidates(mapPath, currentMap)
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
    assert(extension:lower() == MapDataParser.EXTENSION, "Unsupported map data format: " .. resolvedPath)
    local isWorldManifest = MapDataParser.IsWorldManifest(resolvedPath)
    if isWorldManifest then
        WorldDirectoryValidator.Validate(MapDataParser.DATA_ROOT, resolvedPath)
    end
    local data = cjson.decode(Engine.getJSONText(SceneMapBuilder.GetMapDataPath(resolvedPath)))
    ---@cast data table
    if isWorldManifest then
        assert(data.type == "worldMap", "_world.json must declare type as worldMap: " .. resolvedPath)
        MapDataParser.NormaliseWorld(data, resolvedPath)
        ---@cast data Source.SceneComponents.WorldMapData
        return resolvedPath, data
    end
    assert(data.type ~= "worldMap", "worldMap data must use the fixed _world.json filename: " .. resolvedPath)
    ---@cast data Source.SceneComponents.SerializedMapData
    return resolvedPath, MapDataParser.NormaliseMap(data, SceneMapBuilder.BuildAmbientLight)
end

function SceneMapBuilder:resolveMapDestination(mapPath, currentMap, position)
    local resolvedPath = self:resolveMapPath(mapPath, currentMap)
    local worldManifestPath = nil
    if MapDataParser.IsWorldManifest(resolvedPath) then
        worldManifestPath = resolvedPath
    else
        local parent = os.path.dirname(resolvedPath)
        if bool(parent) then
            local candidate = MapPath.Normalise(os.path.join(parent, MapDataParser.WORLD_MANIFEST_FILE))
            if Engine.jsonExists(SceneMapBuilder.GetMapDataPath(candidate)) then
                worldManifestPath = candidate
            end
        end
    end
    if worldManifestPath == nil then
        return resolvedPath, position, false, nil
    end
    local manifestFile, manifest = self:loadMapData(worldManifestPath, currentMap)
    ---@cast manifest Source.SceneComponents.WorldMapData
    if resolvedPath == manifestFile then
        if position ~= nil then
            local worldBounds = { x = 0, y = 0, width = manifest.width, height = manifest.height }
            assert(
                WorldGeometry.RectContainsPosition(worldBounds, position),
                "World destination is outside world bounds: " .. manifestFile
            )
        end
        return manifestFile, position, false, nil
    end
    for _, region in ipairs(manifest.regions) do
        if region.path == resolvedPath then
            if position == nil then
                return manifestFile, nil, true, region
            end
            local childBounds = { x = 0, y = 0, width = region.width, height = region.height }
            assert(
                WorldGeometry.RectContainsPosition(childBounds, position),
                "World child destination is outside the child map: " .. resolvedPath
            )
            local worldPosition = sf.Vector2i.new(position.x + region.x, position.y + region.y)
            ---@cast worldPosition sf.Vector2i
            return manifestFile, worldPosition, true, region
        end
    end
    error("World child map is not placed in its manifest: " .. resolvedPath, 2)
end

function SceneMapBuilder.GetMapDataPath(mapPath)
    return MapDataParser.GetDataPath(mapPath)
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
            local rawTileRow = assert(layerTiles[y], "Map tile grid row is missing")
            for x = 1, mapWidth do
                local tile = rawTileRow[x]
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
            autoTileFrameCounts[index] = AutoTileRuntime.GetFrameCount(texture)
        end
        mapLayers[#mapLayers + 1] = Engine.TileLayer.new(
            tileLayerData, ManagerFunctions.loadTileset(tileLayerData.layerTileset.fileName), autoTileTextures,
            autoTileFrameCounts
        )
    end
    return Engine.Tilemap.new(mapLayers)
end

function SceneMapBuilder.GenerateActors(data)
    local classVarChanges = data.BPClassVarChanged
    local actors = {}
    for _, layerName in ipairs(data.layerOrder) do
        local layerActors = {}
        actors[layerName] = layerActors
        for _, actorData in ipairs(data.actors[layerName] or {}) do
            local actorChanges = nil
            if classVarChanges ~= nil then
                actorChanges = classVarChanges[tostring(actorData.tag or "")]
            end
            local actor = Data.GenActorFromData(actorData, layerName, actorChanges)
            if actor ~= nil then
                layerActors[#layerActors + 1] = actor
            end
        end
    end
    return actors
end

---@param gameMap GameMap
---@diagnostic disable-next-line: unused
function SceneMapBuilder:_configureInteractiveMap(gameMap)
    local pathRouteState = PathRouteState.new()
    local movementDangerState = MovementDangerState.new(gameMap)
    gameMap:addComponent(movementDangerState)
    gameMap:addComponent(MovementDangerPreviewComponent.new(gameMap, movementDangerState))
    gameMap:addComponent(MapClickAutoPath.new(gameMap, pathRouteState, movementDangerState))
    gameMap:addComponent(PathPreviewComponent.new(gameMap, pathRouteState))
    gameMap:setDamageTextConfig(Data.GetPlainTextConfig(DAMAGE_TEXT_CONFIG))
    gameMap:setDamageTextSpeedCurve(Data.GetCurve(DAMAGE_TEXT_SPEED_CURVE))
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
        self:_configureInteractiveMap(result)
        result:setAmbientLight(data.ambientLight)
        for _, light in ipairs(data.lights) do
            result:addLight(light)
        end
    end
    local actors = SceneMapBuilder.GenerateActors(data)
    result:beginActorBatch()
    for _, layerName in ipairs(data.layerOrder) do
        for _, actor in ipairs(actors[layerName]) do
            result:spawnActor(actor, layerName, false)
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
    ---@type table<string, Engine.Actor>
    local actorsByTag = {}
    for _, actor in ipairs(gameMap:getAllActors()) do
        local actorTag = actor:getMapTag()
        if actorTag ~= nil and actorsByTag[actorTag] == nil then
            actorsByTag[actorTag] = actor
        end
    end
    local addedAny = false
    gameMap:beginActorBatch()
    for _, actorRecord in ipairs(addedActors) do
        if actorsByTag[actorRecord.tag] == nil then
            local actor = Data.GenActorFromClassPath(actorRecord.bp, actorRecord.tag, actorRecord.classVarChanges)
            if actor ~= nil then
                actor:setMapPosition(actorRecord.position)
                gameMap:spawnActor(actor, actorRecord.layer, false)
                self:_indexActorTreeByTag(actorsByTag, actor)
                addedAny = true
            end
        end
    end
    gameMap:endActorBatch()
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
        assert(mapData.type ~= "worldMap", "Floor map preview does not support world manifests: " .. mapPath)
        ---@cast mapData Source.SceneComponents.MapData
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

function SceneMapBuilder:_selectWorldMovedActors(records, targetRegion)
    return MapBuilderWorldActors._selectWorldMovedActors(self, records, targetRegion)
end

function SceneMapBuilder:_pruneDestroyedActorTree(actor, destroyedActors)
    return MapBuilderWorldActors._pruneDestroyedActorTree(self, actor, destroyedActors)
end

---@param actorRecord          Source.GameInstance.AddedActorRecord | Source.GameInstance.WorldMovedActorRecord
---@param actorPositions       table<string, sf.Vector2i>
---@param destroyedActors      table<string, boolean>
---@param preserveRootPosition boolean
---@return Engine.Actor | nil
function SceneMapBuilder:_generatePersistedActor(actorRecord, actorPositions, destroyedActors, preserveRootPosition)
    return MapBuilderWorldActors._generatePersistedActor(
        self, actorRecord, actorPositions, destroyedActors, preserveRootPosition
    )
end

function SceneMapBuilder:_applyHoleWorldMovedActors(gameMap, movedActors, actorPositions, destroyedActors)
    return MapBuilderWorldActors._applyHoleWorldMovedActors(self, gameMap, movedActors, actorPositions, destroyedActors)
end

function SceneMapBuilder:generateWorldGameMap(worldPath, worldData, inst, initialPosition)
    return MapBuilderWorldActors.generateWorldGameMap(self, worldPath, worldData, inst, initialPosition)
end

function SceneMapBuilder:_indexActorTreeByTag(actorsByTag, root)
    return MapBuilderWorldActors._indexActorTreeByTag(self, actorsByTag, root)
end

function SceneMapBuilder:createWorldRegionBuildState(
    worldData, region, data, inst, worldPath, addedActors, movedActors, priorityRect
)
    return MapBuilderWorldRegion.createWorldRegionBuildState(
        self, worldData, region, data, inst, worldPath, addedActors, movedActors, priorityRect
    )
end

function SceneMapBuilder:_createWorldRegionChunks(region, width, height, priorityRect)
    return MapBuilderWorldTiles._createWorldRegionChunks(self, region, width, height, priorityRect)
end

function SceneMapBuilder:_createWorldTileGraphicsChunks(region, width, height, priorityRect)
    return MapBuilderWorldTiles._createWorldTileGraphicsChunks(self, region, width, height, priorityRect)
end

function SceneMapBuilder:_createWorldTerrainOverrides(data, terrainDestructions, yieldStep)
    return MapBuilderWorldTiles._createWorldTerrainOverrides(self, data, terrainDestructions, yieldStep)
end

function SceneMapBuilder:_normaliseActorData(actorData)
    return MapBuilderWorldTiles._normaliseActorData(self, actorData)
end

function SceneMapBuilder:_validateIncrementalMapData(data)
    return MapBuilderWorldTiles._validateIncrementalMapData(self, data)
end

function SceneMapBuilder:_writeWorldLayerDataChunk(layerState, chunk)
    return MapBuilderWorldTiles._writeWorldLayerDataChunk(self, layerState, chunk)
end

function SceneMapBuilder:_prepareWorldLayerNativeChunk(layerState, chunk, deadline)
    return MapBuilderWorldTiles._prepareWorldLayerNativeChunk(self, layerState, chunk, deadline)
end

return class(SceneMapBuilder)
