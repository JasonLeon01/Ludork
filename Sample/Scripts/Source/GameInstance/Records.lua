local Engine = require("Engine")
local WorldGeometry = require("Global.WorldGeometry")
local Data = require("Source.Data")

local Records = {}

local function normaliseRecordValue(value)
    if Class.isInstance(value, "table") then
        local result = {}
        for key, item in pairs(value) do
            result[key] = normaliseRecordValue(item)
        end
        return result
    end
    if not Class.isInstance(value, "userdata") then
        return value
    end
    if Class.isInstance(value, sf.IntRect) or Class.isInstance(value, sf.FloatRect) then
        return { normaliseRecordValue(value.position), normaliseRecordValue(value.size) }
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

function Records.NormaliseClassVarChanges(changes)
    local result = {}
    if changes == nil then
        return result
    end
    for key, value in pairs(changes) do
        result[key] = normaliseRecordValue(value)
    end
    return result
end

local function resolveActorClassPath(actor)
    local actorClass = Class.type(actor)
    if bool(rawget(actorClass, "_GENERATED_CLASS")) then
        return Data.ResolveClassPath(rawget(actorClass, "__blueprintClassPath") or "")
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

local function storeLayerTerrainChange(mapChanges, layerName, position, tileID)
    local layerChanges = mapChanges[layerName]
    if layerChanges == nil then
        layerChanges = {}
        mapChanges[layerName] = layerChanges
    end
    layerChanges[WorldGeometry.GridKey(position.x, position.y)] = { position = copy(position), tileID = tileID }
end

function Records.RequirePlayerKey(player)
    local playerKey = player.ID
    assert(bool(playerKey), "Player ID must be a non-empty string")
    return playerKey
end

function Records.AppendPlayer(players, playerKeys, player)
    local playerKey = Records.RequirePlayerKey(player)
    assert(players[playerKey] == nil, "Duplicate player key: " .. playerKey)
    playerKeys[#playerKeys + 1] = playerKey
    players[playerKey] = player
end

function Records.UpsertTaggedRecord(records, record)
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

function Records.AppendUniquePosition(points, point)
    if not table.contains(points, point) then
        points[#points + 1] = point
    end
end

function Records.BuildAddedActorRecord(actor, layerName)
    local actorTag = actor:getMapTag()
    if not bool(actorTag) then
        return nil
    end
    local actorPosition = actor:getMapPosition()
    local blueprintPath = resolveActorClassPath(actor)
    if not bool(blueprintPath) then
        return nil
    end
    local actorRecord = { bp = blueprintPath, layer = layerName, position = copy(actorPosition), tag = actorTag }
    ---@cast actor Source.Data.GeneratedActor
    local classVarChanges = Records.NormaliseClassVarChanges(actor._classVarChanges)
    if bool(classVarChanges) then
        actorRecord.classVarChanges = classVarChanges
    end
    return actorRecord
end

function Records.BuildWorldMovedActorRecord(actor, definitionRegion, currentRegion, layerName, actorPosition)
    local actorRecord = Records.BuildAddedActorRecord(actor, layerName)
    if actorRecord == nil then
        return nil
    end
    local movedRecord = {
        bp = actorRecord.bp,
        layer = actorRecord.layer,
        position = copy(actorPosition),
        tag = actorRecord.tag,
        definitionRegion = definitionRegion,
        currentRegion = currentRegion
    }
    if actorRecord.classVarChanges ~= nil then
        movedRecord.classVarChanges = actorRecord.classVarChanges
    end
    return movedRecord
end

function Records.StoreTerrainChange(terrainDestructions, mapPath, layerName, position, tileID)
    local mapChanges = terrainDestructions[mapPath]
    if mapChanges == nil then
        mapChanges = {}
        terrainDestructions[mapPath] = mapChanges
    end
    storeLayerTerrainChange(mapChanges, layerName, position, tileID)
end

return Records
