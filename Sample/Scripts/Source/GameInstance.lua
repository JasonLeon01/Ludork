
local cjson = require("cjson")
local Data = require("Source.Data")
local Engine = require("Engine")
local GameVariables = require("Source.Configs.GameVariables")
local MapPath = require("Source.MapPath")

---@param position sf.Vector2i | sf.Vector2u
---@return integer[]
local function vectorArray(position)
    return { position.x, position.y }
end

---@param position sf.Vector2i | sf.Vector2u
---@return string
local function coordinateKey(position)
    return tostring(position.x) .. "," .. tostring(position.y)
end

---@param records Source.GameInstance.AddedActorRecord[]
---@param record  Source.GameInstance.AddedActorRecord
local function upsertTaggedRecord(records, record)
    local index = 1
    while index <= #records do
        local existing = records[index]
        ---@cast existing Source.GameInstance.AddedActorRecord
        if existing.tag == record.tag then
            table.remove(records, index)
        else
            index = index + 1
        end
    end
    records[#records + 1] = record
end

---@param points sf.Vector2u[]
---@param point  sf.Vector2u
local function appendUniquePosition(points, point)
    for _, existing in ipairs(points) do
        if existing.x == point.x and existing.y == point.y then
            return
        end
    end
    points[#points + 1] = point
end

---@param mapChanges table<string, table<string, Source.GameInstance.TerrainChangeRecord>>
---@param layerName  string
---@param position   sf.Vector2i
---@param tileID     Source.GameInstance.TerrainTileID
local function storeTerrainChange(mapChanges, layerName, position, tileID)
    local layerChanges = mapChanges[layerName]
    if layerChanges == nil then
        layerChanges = {}
        mapChanges[layerName] = layerChanges
    end
    layerChanges[coordinateKey(position)] = { position = copy(position), tileID = tileID }
end

local GameInstance = {}

function GameInstance:init(skipDefaultPlayer)
    self._players = {}
    self._currentRegion = "Mota"
    self._variables = deepcopy(GameVariables)
    self._cachedMap = nil
    self._cachedNewItem = {}
    self._cachedAddedActors = {}
    self._cachedActorPositions = {}
    self._cachedDestroyedActors = {}
    self._cachedTerrainDestructions = {}
    self._cachedTelepoints = {}
    self._screenshot = nil
    if skipDefaultPlayer then
        return
    end
    local Player = require("Source.Player")
    local GameSystem = require("Source.System")

    local firstPlayer = Player.InitPlayer("Data.Blueprints.Actors.BP_Actor_Braver")
    firstPlayer:setMapPosition(GameSystem.getStartPos())
    self._players[1] = firstPlayer
end

function GameInstance:asDict()
    local players = {}
    for _, player in ipairs(self._players) do
        players[#players + 1] = player:asDict()
    end
    local cachedMap = self._cachedMap
    ---@cast cachedMap string
    return {
        region = self._currentRegion,
        players = players,
        variables = self._variables,
        map = cachedMap,
        obtainedItems = self._cachedNewItem,
        addedActors = GameInstance._serialiseAddedActors(self._cachedAddedActors),
        actorPositions = GameInstance._serialiseActorPositions(self._cachedActorPositions),
        destroyedActors = self._cachedDestroyedActors,
        destroyedTerrain = GameInstance._serialiseTerrainDestructions(self._cachedTerrainDestructions),
        telepoints = GameInstance._serialiseTelepoints(self._cachedTelepoints),
        screenshot = self._screenshot
    }
end

function GameInstance.FromDict(data)
    local Player = require("Source.Player")

    local instance = GameInstance.new(true)
    instance._currentRegion = data.region
    for _, playerData in ipairs(data.players) do
        instance._players[#instance._players + 1] = Player.FromDict(playerData)
    end
    instance._variables = data.variables
    instance._cachedMap = data.map
    instance._cachedAddedActors = GameInstance._normaliseAddedActors(data.addedActors)
    instance._cachedActorPositions = GameInstance._normaliseActorPositions(data.actorPositions)
    instance._cachedDestroyedActors = GameInstance._normaliseDestroyedActors(data.destroyedActors)
    instance._cachedTerrainDestructions = GameInstance._normaliseTerrainDestructions(data.destroyedTerrain)
    instance._cachedNewItem = GameInstance._normaliseObtainedItems(data.obtainedItems)
    instance._cachedTelepoints = GameInstance._normaliseTelepoints(data.telepoints)
    instance._screenshot = data.screenshot == cjson.null and nil or data.screenshot
    return instance
end

function GameInstance:getCurrentRegion()
    return self._currentRegion
end

function GameInstance:setCurrentRegion(region)
    self._currentRegion = region
end

function GameInstance:setScreenshot(screenshot)
    self._screenshot = screenshot
end

function GameInstance:getScreenshot()
    return self._screenshot
end

function GameInstance:getVariables()
    return self._variables
end

function GameInstance:getVariable(name)
    return self._variables[name]
end

function GameInstance:setVariable(name, value)
    self._variables[name] = value
end

function GameInstance:getPlayer()
    return self._players[1]
end

function GameInstance:setPlayer(player)
    if bool(self._players) then
        self._players[1] = player
    else
        self._players[#self._players + 1] = player
    end
end

function GameInstance:getPlayers()
    return self._players
end

function GameInstance:getPlayerByIndex(index)
    local player = self._players[index + 1]
    ---@cast player Source.Player.Player
    return player
end

function GameInstance:getPlayerByTag(tag)
    for _, player in ipairs(self._players) do
        if player.tag == tag then
            return player
        end
    end
    return nil
end

function GameInstance:addPlayerByClass(playerClass)
    local Player = require("Source.Player")

    self._players[#self._players + 1] = Player.InitPlayer(playerClass)
end

function GameInstance:removePlayerByClass(playerClass)
    if #self._players <= 1 then
        return
    end
    for index, player in ipairs(self._players) do
        if player:getClassPath() == playerClass then
            table.remove(self._players, index)
            return
        end
    end
end

function GameInstance:applyMapInfo(mapPath, position)
    if bool(mapPath) then
        self._cachedMap = mapPath
    end
    if position ~= nil then
        self._players[1]:setMapPosition(position)
    end
end

function GameInstance:recordAddedActor(mapPath, actor, layerName)
    local actorRecord = GameInstance._buildAddedActorRecord(actor, layerName)
    if actorRecord == nil then
        return
    end
    mapPath = MapPath.Normalise(mapPath)
    local records = self._cachedAddedActors[mapPath] or {}
    self._cachedAddedActors[mapPath] = records
    upsertTaggedRecord(records, actorRecord)
end

function GameInstance:getAddedActors(mapPath)
    return self._cachedAddedActors[MapPath.Normalise(mapPath)] or {}
end

function GameInstance:recordActorPosition(mapPath, actor, actorPosition)
    mapPath = MapPath.Normalise(mapPath)
    local actorTag = actor:getMapTag()
    if not bool(actorTag) then
        return
    end
    if actorPosition == nil then
        actorPosition = actor:getMapPosition()
    end
    self._cachedActorPositions[mapPath] = self._cachedActorPositions[mapPath] or {}
    self._cachedActorPositions[mapPath][actorTag] = copy(actorPosition)
end

function GameInstance:getActorPositions(mapPath)
    return self._cachedActorPositions[MapPath.Normalise(mapPath)] or {}
end

function GameInstance:recordDestroyedActor(mapPath, actor)
    mapPath = MapPath.Normalise(mapPath)
    local actorTag = actor:getMapTag()
    if not bool(actorTag) then
        return
    end
    local records = self._cachedDestroyedActors[mapPath] or {}
    self._cachedDestroyedActors[mapPath] = records
    if not table.contains(records, actorTag) then
        records[#records + 1] = actorTag
    end
end

function GameInstance:getDestroyedActors(mapPath)
    return self._cachedDestroyedActors[MapPath.Normalise(mapPath)] or {}
end

function GameInstance:recordTerrainDestruction(mapPath, layerName, position, tileID)
    GameInstance._storeTerrainChange(
        self._cachedTerrainDestructions, MapPath.Normalise(mapPath), layerName, position, tileID
    )
end

function GameInstance:getTerrainDestructions(mapPath)
    mapPath = MapPath.Normalise(mapPath)
    return self._cachedTerrainDestructions[mapPath] or {}
end

function GameInstance:recordTelepoint(mapPath, telepoint)
    mapPath = MapPath.Normalise(mapPath)
    local points = self._cachedTelepoints[mapPath] or {}
    self._cachedTelepoints[mapPath] = points
    appendUniquePosition(points, copy(telepoint))
end

function GameInstance:getTelepoints(mapPath)
    return self._cachedTelepoints[MapPath.Normalise(mapPath)] or {}
end

---@param actor     Engine.Actor
---@param layerName string
---@return Source.GameInstance.AddedActorRecord | nil
function GameInstance._buildAddedActorRecord(actor, layerName)
    local actorTag = actor:getMapTag()
    if not bool(actorTag) then
        return nil
    end
    local actorPosition = actor:getMapPosition()
    local blueprintPath = GameInstance._resolveActorClassPath(actor)
    if not bool(blueprintPath) then
        return nil
    end
    local actorRecord = {
        bp = blueprintPath,
        layer = layerName,
        position = copy(actorPosition),
        tag = actorTag
    }
    ---@cast actor Source.Data.GeneratedActor
    local classVarChanges = GameInstance._normaliseClassVarChanges(actor._classVarChanges)
    if bool(classVarChanges) then
        actorRecord.classVarChanges = classVarChanges
    end
    return actorRecord
end

---@param actor Engine.Actor
---@return string
function GameInstance._resolveActorClassPath(actor)
    local actorClass = Class.type(actor)
    if rawget(actorClass, "_GENERATED_CLASS") then
        return Data.resolveClassPath(rawget(actorClass, "__blueprintClassPath") or "")
    end
    if rawget(actorClass, "__ludorkClass") ~= true then
        return ""
    end
    local moduleName = Engine.getClassModulePath(actorClass)
    if not bool(moduleName) then
        return ""
    end
    local className = moduleName:match("([^%.]+)$")
    if className == nil then
        return ""
    end
    return moduleName .. "." .. className
end

---@param addedActors table<string, Source.GameInstance.SavedAddedActorRecord[]>
---@return table<string, Source.GameInstance.AddedActorRecord[]>
function GameInstance._normaliseAddedActors(addedActors)
    local result = {}
    for mapPath, records in pairs(addedActors) do
        local bucket = {}
        result[MapPath.Normalise(mapPath)] = bucket
        for _, record in ipairs(records) do
            local actorRecord = {
                bp = Data.resolveClassPath(record.bp),
                layer = record.layer,
                position = GameInstance._savedVector2i(record.position),
                tag = record.tag
            }
            local changes = GameInstance._normaliseClassVarChanges(record.classVarChanges)
            if bool(changes) then
                actorRecord.classVarChanges = changes
            end
            upsertTaggedRecord(bucket, actorRecord)
        end
    end
    return result
end

---@param addedActors table<string, Source.GameInstance.AddedActorRecord[]>
---@return table<string, Source.GameInstance.SavedAddedActorRecord[]>
function GameInstance._serialiseAddedActors(addedActors)
    local result = {}
    for mapPath, records in pairs(addedActors) do
        local serialisedRecords = {}
        for _, record in ipairs(records) do
            local serialisedRecord = {
                bp = record.bp,
                layer = record.layer,
                position = vectorArray(record.position),
                tag = record.tag
            }
            if bool(record.classVarChanges) then
                serialisedRecord.classVarChanges = record.classVarChanges
            end
            serialisedRecords[#serialisedRecords + 1] = serialisedRecord
        end
        if bool(serialisedRecords) then
            result[mapPath] = serialisedRecords
        end
    end
    return result
end

---@param changes table | nil
---@return table<string, Source.GameInstance.RecordValue>
function GameInstance._normaliseClassVarChanges(changes)
    local result = {}
    if changes == nil then
        return result
    end
    for key, value in pairs(changes) do
        result[key] = GameInstance._normaliseRecordValue(value)
    end
    return result
end

---@param value Source.Data.ClassVarValue
---@return Source.GameInstance.RecordValue
function GameInstance._normaliseRecordValue(value)
    if type(value) == "table" then
        local result = {}
        for key, item in pairs(value) do
            result[key] = GameInstance._normaliseRecordValue(item)
        end
        return result
    end
    if type(value) ~= "userdata" then
        return value
    end
    if Class.isInstance(value, sf.IntRect) or Class.isInstance(value, sf.FloatRect) then
        return { GameInstance._normaliseRecordValue(value.position), GameInstance._normaliseRecordValue(value.size) }
    end
    if Class.isInstance(value, sf.Vector2i) or Class.isInstance(value, sf.Vector2u)
        or Class.isInstance(value, sf.Vector2f) or Class.isInstance(value, sf.Vector2b) then
        return { value.x, value.y }
    end
    if Class.isInstance(value, sf.Vector3i) or Class.isInstance(value, sf.Vector3u)
        or Class.isInstance(value, sf.Vector3f) or Class.isInstance(value, sf.Vector3b) then
        return { value.x, value.y, value.z }
    end
    if Class.isInstance(value, sf.Color) then
        return { value.r, value.g, value.b, value.a }
    end
    return tostring(value)
end

---@param actorPositions table<string, table<string, integer[]>>
---@return table<string, table<string, sf.Vector2i>>
function GameInstance._normaliseActorPositions(actorPositions)
    local result = {}
    for mapPath, records in pairs(actorPositions) do
        local bucket = {}
        result[MapPath.Normalise(mapPath)] = bucket
        for actorTag, position in pairs(records) do
            bucket[actorTag] = GameInstance._savedVector2i(position)
        end
    end
    return result
end

---@param actorPositions table<string, table<string, sf.Vector2i>>
---@return table<string, table<string, integer[]>>
function GameInstance._serialiseActorPositions(actorPositions)
    local result = {}
    for mapPath, records in pairs(actorPositions) do
        local serialisedRecords = {}
        for actorTag, position in pairs(records) do
            serialisedRecords[actorTag] = vectorArray(position)
        end
        if bool(serialisedRecords) then
            result[mapPath] = serialisedRecords
        end
    end
    return result
end

---@param destroyedActors table<string, string[]>
---@return table<string, string[]>
function GameInstance._normaliseDestroyedActors(destroyedActors)
    local result = {}
    for mapPath, actorTags in pairs(destroyedActors) do
        local bucket = {}
        result[MapPath.Normalise(mapPath)] = bucket
        for _, actorTag in ipairs(actorTags) do
            if not table.contains(bucket, actorTag) then
                bucket[#bucket + 1] = actorTag
            end
        end
    end
    return result
end

---@param telepoints table<string, integer[][]>
---@return table<string, sf.Vector2u[]>
function GameInstance._normaliseTelepoints(telepoints)
    local result = {}
    for mapPath, points in pairs(telepoints) do
        local bucket = {}
        result[MapPath.Normalise(mapPath)] = bucket
        for _, point in ipairs(points) do
            appendUniquePosition(bucket, GameInstance._savedVector2u(point))
        end
    end
    return result
end

---@param telepoints table<string, sf.Vector2u[]>
---@return table<string, integer[][]>
function GameInstance._serialiseTelepoints(telepoints)
    local result = {}
    for mapPath, points in pairs(telepoints) do
        local serialisedPoints = {}
        for _, point in ipairs(points) do
            serialisedPoints[#serialisedPoints + 1] = vectorArray(point)
        end
        if bool(serialisedPoints) then
            result[mapPath] = serialisedPoints
        end
    end
    return result
end

---@param terrainDestructions table<string, table<string, table<string, Source.GameInstance.TerrainChangeRecord>>>
---@return table<string, table<string, Source.GameInstance.SavedTerrainChangeRecord[]>>
function GameInstance._serialiseTerrainDestructions(terrainDestructions)
    local result = {}
    for mapPath, layerChanges in pairs(terrainDestructions) do
        local serialisedLayers = {}
        for layerName, changes in pairs(layerChanges) do
            local orderedChanges = {}
            for _, change in pairs(changes) do
                orderedChanges[#orderedChanges + 1] = change
            end
            table.sort(orderedChanges, function (left, right)
                if left.position.x == right.position.x then
                    return left.position.y < right.position.y
                end
                return left.position.x < right.position.x
            end)
            local serialisedChanges = {}
            for _, change in ipairs(orderedChanges) do
                serialisedChanges[#serialisedChanges + 1] = {
                    position = vectorArray(change.position),
                    tileID = change.tileID == nil and cjson.null or change.tileID
                }
            end
            if bool(serialisedChanges) then
                serialisedLayers[layerName] = serialisedChanges
            end
        end
        if bool(serialisedLayers) then
            result[mapPath] = serialisedLayers
        end
    end
    return result
end

---@param terrainDestructions table<string, table<string, Source.GameInstance.SavedTerrainChangeRecord[]>>
---@return table<string, table<string, table<string, Source.GameInstance.TerrainChangeRecord>>>
function GameInstance._normaliseTerrainDestructions(terrainDestructions)
    local result = {}
    for mapPath, layerChanges in pairs(terrainDestructions) do
        local mapChanges = {}
        result[MapPath.Normalise(mapPath)] = mapChanges
        for layerName, changes in pairs(layerChanges) do
            for _, change in ipairs(changes) do
                ---@type Source.GameInstance.TerrainTileID
                local tileID
                if change.tileID ~= cjson.null then
                    local savedTileID = change.tileID
                    ---@cast savedTileID -lightuserdata
                    tileID = savedTileID
                end
                storeTerrainChange(mapChanges, layerName, GameInstance._savedVector2i(change.position), tileID)
            end
        end
    end
    return result
end

---@param position integer[]
---@return sf.Vector2i
function GameInstance._savedVector2i(position)
    local x = position[1]
    local y = position[2]
    ---@cast x integer
    ---@cast y integer
    local result = sf.Vector2i.new(x, y)
    ---@cast result sf.Vector2i
    return result
end

---@param position integer[]
---@return sf.Vector2u
function GameInstance._savedVector2u(position)
    local x = position[1]
    local y = position[2]
    ---@cast x integer
    ---@cast y integer
    local result = sf.Vector2u.new(x, y)
    ---@cast result sf.Vector2u
    return result
end

---@param terrainDestructions table<string, table<string, table<string, Source.GameInstance.TerrainChangeRecord>>>
---@param mapPath             string
---@param layerName           string
---@param position            sf.Vector2i
---@param tileID              Source.GameInstance.TerrainTileID
function GameInstance._storeTerrainChange(terrainDestructions, mapPath, layerName, position, tileID)
    local mapChanges = terrainDestructions[mapPath]
    if mapChanges == nil then
        mapChanges = {}
        terrainDestructions[mapPath] = mapChanges
    end
    storeTerrainChange(mapChanges, layerName, position, tileID)
end

---@param obtainedItems table<string, boolean>
---@return table<string, boolean>
function GameInstance._normaliseObtainedItems(obtainedItems)
    local result = {}
    for itemID, obtained in pairs(obtainedItems) do
        if obtained then
            result[itemID] = true
        end
    end
    return result
end

function GameInstance:getCachedNewItem(itemID)
    return self._cachedNewItem[itemID] == true
end

function GameInstance:setCachedNewItem(itemID)
    self._cachedNewItem[itemID] = true
end

return class(GameInstance)
