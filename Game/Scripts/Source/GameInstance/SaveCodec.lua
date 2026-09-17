local cjson = require("cjson")
local Data = require("Source.Data")
local Records = require("Source.GameInstance.Records")
local MapPath = require("Source.MapPath")
local MapConstants = require("Source.Configs.MapConstants")

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

local function containsWorldManifest(maps)
    for _, mapPath in pairs(maps) do
        if os.path.basename(mapPath) == MapConstants.WORLD_MANIFEST_FILE then
            return true
        end
    end
    return false
end

local function serialisePlayerMaps(playerKeys, currentMaps)
    assert(Class.isInstance(currentMaps, "table"), "Cannot serialise a GameInstance before its current map is set")
    local maps = {}
    for _, playerKey in ipairs(playerKeys) do
        local mapPath = currentMaps[playerKey]
        assert(
            Class.isInstance(mapPath, "string") and bool(mapPath),
            "Cannot serialise a GameInstance before map is set for player: " .. playerKey
        )
        maps[playerKey] = MapPath.Normalise(mapPath)
    end
    for playerKey in pairs(currentMaps) do
        assert(maps[playerKey] ~= nil, "Map is stored for a player key missing from playerKeys: " .. playerKey)
    end
    return maps
end

local function normalisePlayerMaps(savedMaps, playerKeys)
    assert(Class.isInstance(savedMaps, "table"), "map must be an object keyed by player key")
    local maps = {}
    for _, playerKey in ipairs(playerKeys) do
        local mapPath = savedMaps[playerKey]
        assert(
            Class.isInstance(mapPath, "string") and bool(mapPath),
            "Map is missing for player key: " .. playerKey
        )
        maps[playerKey] = MapPath.Normalise(mapPath)
    end
    for playerKey in pairs(savedMaps) do
        assert(maps[playerKey] ~= nil, "Map is stored for a player key missing from playerKeys: " .. playerKey)
    end
    return maps
end

function SaveCodec.Encode(state)
    local players = {}
    for _, playerKey in ipairs(state.playerKeys) do
        assert(Class.hasOwnField(state.players, playerKey), "Player data is missing for key: " .. playerKey)
        assert(
            Records.RequirePlayerKey(state.players[playerKey]) == playerKey,
            "Player ID does not match player key: " .. playerKey
        )
        assert(players[playerKey] == nil, "Duplicate player key: " .. playerKey)
        players[playerKey] = state.players[playerKey]:asDict()
    end
    for playerKey in pairs(state.players) do
        assert(players[playerKey] ~= nil, "Player key is missing from playerKeys: " .. playerKey)
    end
    local maps = serialisePlayerMaps(state.playerKeys, state.currentMaps)
    local worldMovedActors = serialiseWorldMovedActors(state.worldMovedActors)
    local saveData = {
        version = SAVE_VERSION,
        region = state.currentRegion,
        playerKeys = copy(state.playerKeys),
        players = players,
        variables = deepcopy(state.variables),
        map = maps,
        obtainedItems = deepcopy(state.obtainedItems),
        addedActors = serialiseAddedActors(state.addedActors),
        actorPositions = serialiseActorPositions(state.actorPositions),
        destroyedActors = deepcopy(state.destroyedActors),
        destroyedTerrain = serialiseTerrainDestructions(state.terrainDestructions),
        telepoints = serialiseTelepoints(state.telepoints),
        screenshot = deepcopy(state.screenshot)
    }
    if bool(worldMovedActors) or containsWorldManifest(maps) then
        saveData.worldMovedActors = worldMovedActors
    end
    return saveData
end

function SaveCodec.Decode(data)
    assert(Class.isInstance(data, "table"), "Save data must be an object")
    assert(
        Class.isInstance(data.version, "number") and data.version == SAVE_VERSION,
        "Unsupported save version: expected " .. tostring(SAVE_VERSION) .. ", got " .. tostring(data.version)
    )
    local Player = require("Source.Player")

    local state = { players = {}, playerKeys = {} }
    state.currentRegion = data.region
    assert(#data.playerKeys > 0, "playerKeys must contain at least one player key")
    for _, playerKey in ipairs(data.playerKeys) do
        assert(Class.isInstance(playerKey, "string") and bool(playerKey), "Player key must be a non-empty string")
        ---@diagnostic disable-next-line: unnecessary-assert, save validation must reject a missing keyed player
        local playerData = assert(data.players[playerKey], "Player data is missing for key: " .. playerKey)
        local player = Player.FromDict(playerData)
        assert(Records.RequirePlayerKey(player) == playerKey, "Player ID does not match player key: " .. playerKey)
        Records.AppendPlayer(state.players, state.playerKeys, player)
    end
    for playerKey in pairs(data.players) do
        assert(state.players[playerKey] ~= nil, "Player key is missing from playerKeys: " .. playerKey)
    end
    state.variables = data.variables
    state.currentMaps = normalisePlayerMaps(data.map, state.playerKeys)
    state.addedActors = normaliseAddedActors(data.addedActors)
    state.actorPositions = normaliseActorPositions(data.actorPositions)
    if data.worldMovedActors == nil then
        assert(not containsWorldManifest(state.currentMaps), "worldMovedActors must be an object for a world save")
        state.worldMovedActors = {}
    else
        state.worldMovedActors = normaliseWorldMovedActors(data.worldMovedActors)
    end
    state.destroyedActors = normaliseDestroyedActors(data.destroyedActors)
    state.terrainDestructions = normaliseTerrainDestructions(data.destroyedTerrain)
    state.obtainedItems = normaliseObtainedItems(data.obtainedItems)
    state.telepoints = normaliseTelepoints(data.telepoints)
    state.screenshot = data.screenshot == cjson.null and nil or data.screenshot
    return state
end

return SaveCodec
