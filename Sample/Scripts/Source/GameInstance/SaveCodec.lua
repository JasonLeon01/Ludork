local cjson = require("cjson")
local Data = require("Source.Data")
local Records = require("Source.GameInstance.Records")
local MapPath = require("Source.MapPath")

local SaveCodec = {}
local SAVE_VERSION = 1

local function vectorArray(position)
    return { position.x, position.y }
end

local function savedVector2i(position)
    local x = position[1]
    local y = position[2]
    ---@cast x integer
    ---@cast y integer
    local result = sf.Vector2i.new(x, y)
    ---@cast result sf.Vector2i
    return result
end

local function savedVector2u(position)
    local x = position[1]
    local y = position[2]
    ---@cast x integer
    ---@cast y integer
    local result = sf.Vector2u.new(x, y)
    ---@cast result sf.Vector2u
    return result
end

local function normaliseAddedActors(addedActors)
    local result = {}
    for mapPath, records in pairs(addedActors) do
        local bucket = {}
        result[MapPath.Normalise(mapPath)] = bucket
        for _, record in ipairs(records) do
            local actorRecord = {
                bp = Data.ResolveClassPath(record.bp),
                layer = record.layer,
                position = savedVector2i(record.position),
                tag = record.tag
            }
            local changes = Records.NormaliseClassVarChanges(record.classVarChanges)
            if bool(changes) then
                actorRecord.classVarChanges = changes
            end
            Records.UpsertTaggedRecord(bucket, actorRecord)
        end
    end
    return result
end

local function serialiseAddedActors(addedActors)
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

local function normaliseWorldMovedActors(worldMovedActors)
    assert(Class.isInstance(worldMovedActors, "table"), "worldMovedActors must be an object")
    local result = {}
    for worldPath, records in pairs(worldMovedActors) do
        local bucket = {}
        local tags = {}
        result[MapPath.Normalise(worldPath)] = bucket
        for _, record in ipairs(records) do
            assert(
                Class.isInstance(record.bp, "string") and bool(record.bp),
                "Moved world Actor bp must be a non-empty string"
            )
            assert(
                Class.isInstance(record.layer, "string") and bool(record.layer),
                "Moved world Actor layer must be a non-empty string"
            )
            assert(
                Class.isInstance(record.tag, "string") and bool(record.tag),
                "Moved world Actor tag must be a non-empty string"
            )
            assert(
                Class.isInstance(record.definitionRegion, "string"),
                "Moved world Actor definitionRegion must be a string"
            )
            assert(Class.isInstance(record.currentRegion, "string"), "Moved world Actor currentRegion must be a string")
            local definitionRegion = MapPath.Normalise(record.definitionRegion)
            local currentRegion = MapPath.Normalise(record.currentRegion)
            assert(not tags[record.tag], "Duplicate moved world Actor tag: " .. record.tag)
            assert(bool(definitionRegion), "Moved world Actor definitionRegion must be a non-empty map path")
            assert(
                currentRegion ~= definitionRegion,
                "Moved world Actor currentRegion must differ from definitionRegion: " .. record.tag
            )
            tags[record.tag] = true
            local actorRecord = {
                bp = Data.ResolveClassPath(record.bp),
                layer = record.layer,
                position = savedVector2i(record.position),
                tag = record.tag,
                definitionRegion = definitionRegion,
                currentRegion = currentRegion
            }
            local changes = Records.NormaliseClassVarChanges(record.classVarChanges)
            if bool(changes) then
                actorRecord.classVarChanges = changes
            end
            bucket[#bucket + 1] = actorRecord
        end
    end
    return result
end

local function serialiseWorldMovedActors(worldMovedActors)
    local result = {}
    for worldPath, records in pairs(worldMovedActors) do
        local serialisedRecords = {}
        for _, record in ipairs(records) do
            local serialisedRecord = {
                bp = record.bp,
                layer = record.layer,
                position = vectorArray(record.position),
                tag = record.tag,
                definitionRegion = record.definitionRegion,
                currentRegion = record.currentRegion
            }
            if bool(record.classVarChanges) then
                serialisedRecord.classVarChanges = record.classVarChanges
            end
            serialisedRecords[#serialisedRecords + 1] = serialisedRecord
        end
        if bool(serialisedRecords) then
            result[worldPath] = serialisedRecords
        end
    end
    return result
end

local function normaliseActorPositions(actorPositions)
    local result = {}
    for mapPath, records in pairs(actorPositions) do
        local bucket = {}
        result[MapPath.Normalise(mapPath)] = bucket
        for actorTag, position in pairs(records) do
            bucket[actorTag] = savedVector2i(position)
        end
    end
    return result
end

local function serialiseActorPositions(actorPositions)
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

local function normaliseDestroyedActors(destroyedActors)
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

local function normaliseTelepoints(telepoints)
    local result = {}
    for mapPath, points in pairs(telepoints) do
        local bucket = {}
        result[MapPath.Normalise(mapPath)] = bucket
        for _, point in ipairs(points) do
            Records.AppendUniqueTelepoint(bucket, savedVector2u(point.position), point.tag)
        end
    end
    return result
end

local function serialiseTelepoints(telepoints)
    local result = {}
    for mapPath, points in pairs(telepoints) do
        local serialisedPoints = {}
        for _, point in ipairs(points) do
            serialisedPoints[#serialisedPoints + 1] = { position = vectorArray(point.position), tag = point.tag }
        end
        if bool(serialisedPoints) then
            result[mapPath] = serialisedPoints
        end
    end
    return result
end

local function serialiseTerrainDestructions(terrainDestructions)
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

local function normaliseTerrainDestructions(terrainDestructions)
    local result = {}
    for mapPath, layerChanges in pairs(terrainDestructions) do
        local normalisedPath = MapPath.Normalise(mapPath)
        result[normalisedPath] = {}
        for layerName, changes in pairs(layerChanges) do
            for _, change in ipairs(changes) do
                ---@type Global.GameMap.TerrainTileID
                local tileID
                if change.tileID ~= cjson.null then
                    local savedTileID = change.tileID
                    ---@cast savedTileID - lightuserdata
                    tileID = savedTileID
                end
                Records.StoreTerrainChange(result, normalisedPath, layerName, savedVector2i(change.position), tileID)
            end
        end
    end
    return result
end

local function normaliseObtainedItems(obtainedItems)
    local result = {}
    for itemID, obtained in pairs(obtainedItems) do
        if obtained then
            result[itemID] = true
        end
    end
    return result
end

function SaveCodec.Encode(instance)
    local players = {}
    for _, playerKey in ipairs(instance._playerKeys) do
        assert(Class.hasOwnField(instance._players, playerKey), "Player data is missing for key: " .. playerKey)
        assert(
            Records.RequirePlayerKey(instance._players[playerKey]) == playerKey,
            "Player ID does not match player key: " .. playerKey
        )
        assert(players[playerKey] == nil, "Duplicate player key: " .. playerKey)
        players[playerKey] = instance._players[playerKey]:asDict()
    end
    for playerKey in pairs(instance._players) do
        assert(players[playerKey] ~= nil, "Player key is missing from playerKeys: " .. playerKey)
    end
    local cachedMap = assert(instance._cachedMap, "Cannot serialise a GameInstance before its current map is set")
    local worldMovedActors = serialiseWorldMovedActors(instance._cachedWorldMovedActors)
    local saveData = {
        version = SAVE_VERSION,
        region = instance._currentRegion,
        playerKeys = copy(instance._playerKeys),
        players = players,
        variables = instance._variables,
        map = cachedMap,
        obtainedItems = instance._cachedNewItem,
        addedActors = serialiseAddedActors(instance._cachedAddedActors),
        actorPositions = serialiseActorPositions(instance._cachedActorPositions),
        destroyedActors = instance._cachedDestroyedActors,
        destroyedTerrain = serialiseTerrainDestructions(instance._cachedTerrainDestructions),
        telepoints = serialiseTelepoints(instance._cachedTelepoints),
        screenshot = instance._screenshot
    }
    if bool(worldMovedActors) or os.path.basename(cachedMap) == "_world.json" then
        saveData.worldMovedActors = worldMovedActors
    end
    return saveData
end

function SaveCodec.DecodeInto(instance, data)
    assert(Class.isInstance(data, "table"), "Save data must be an object")
    assert(
        Class.isInstance(data.version, "number") and data.version == SAVE_VERSION,
        "Unsupported save version: expected " .. tostring(SAVE_VERSION) .. ", got " .. tostring(data.version)
    )
    local Player = require("Source.Player")

    instance._currentRegion = data.region
    assert(#data.playerKeys > 0, "playerKeys must contain at least one player key")
    for _, playerKey in ipairs(data.playerKeys) do
        assert(Class.isInstance(playerKey, "string") and bool(playerKey), "Player key must be a non-empty string")
        ---@diagnostic disable-next-line: unnecessary-assert, save validation must reject a missing keyed player
        local playerData = assert(data.players[playerKey], "Player data is missing for key: " .. playerKey)
        local player = Player.FromDict(playerData)
        assert(Records.RequirePlayerKey(player) == playerKey, "Player ID does not match player key: " .. playerKey)
        Records.AppendPlayer(instance._players, instance._playerKeys, player)
    end
    for playerKey in pairs(data.players) do
        assert(instance._players[playerKey] ~= nil, "Player key is missing from playerKeys: " .. playerKey)
    end
    instance._variables = data.variables
    instance._cachedMap = data.map
    instance._cachedAddedActors = normaliseAddedActors(data.addedActors)
    instance._cachedActorPositions = normaliseActorPositions(data.actorPositions)
    if data.worldMovedActors == nil then
        assert(
            os.path.basename(MapPath.Normalise(data.map)) ~= "_world.json",
            "worldMovedActors must be an object for a world save"
        )
        instance._cachedWorldMovedActors = {}
    else
        instance._cachedWorldMovedActors = normaliseWorldMovedActors(data.worldMovedActors)
    end
    instance:_validateWorldActorRecordTags()
    instance._cachedDestroyedActors = normaliseDestroyedActors(data.destroyedActors)
    instance._cachedTerrainDestructions = normaliseTerrainDestructions(data.destroyedTerrain)
    instance._cachedNewItem = normaliseObtainedItems(data.obtainedItems)
    instance._cachedTelepoints = normaliseTelepoints(data.telepoints)
    instance._screenshot = data.screenshot == cjson.null and nil or data.screenshot
end

return SaveCodec
