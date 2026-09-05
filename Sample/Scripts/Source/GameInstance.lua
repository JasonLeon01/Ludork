local GameVariables = require("Source.Configs.GameVariables")
local GameInstanceRecords = require("Source.GameInstance.Records")
local GameInstanceSaveCodec = require("Source.GameInstance.SaveCodec")
local GameInstanceWorldPersistence = require("Source.GameInstance.WorldPersistence")
local MapPath = require("Source.MapPath")
---@class (partial) Source.GameInstance.GameInstance
local GameInstance = {}

---@alias GameInstanceImplState Source.GameInstance.GameInstance

function GameInstance:init(skipDefaultPlayer)
    self._playerKeys = {}
    self._players = {}
    self._currentRegion = ""
    ---@type table<string, Source.GameInstance.RecordValue>
    self._variables = deepcopy(GameVariables)
    self._cachedMap = nil
    self._cachedNewItem = {}
    self._cachedAddedActors = {}
    self._cachedActorPositions = {}
    self._cachedWorldMovedActors = {}
    self._cachedDestroyedActors = {}
    self._cachedTerrainDestructions = {}
    self._cachedTelepoints = {}
    self._screenshot = nil
    if skipDefaultPlayer then
        return
    end
    local Player = require("Source.Player")
    local GameSystem = require("Source.System")

    self._currentRegion = GameSystem.GetStartRegion()
    local firstPlayer = Player.InitPlayer(GameSystem.GetStartPlayerClassPath())
    firstPlayer:setMapPosition(GameSystem.GetStartPos())
    GameInstanceRecords.AppendPlayer(self._players, self._playerKeys, firstPlayer)
end

function GameInstance:asDict()
    return GameInstanceSaveCodec.Encode(self)
end

function GameInstance.FromDict(data)
    local instance = GameInstance.new(true)
    GameInstanceSaveCodec.DecodeInto(instance, data)
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
    return self._players[self._playerKeys[1]]
end

function GameInstance:setPlayer(playerKey)
    assert(self._players[playerKey] ~= nil, "Player does not exist: " .. tostring(playerKey))
    local index = table.index(self._playerKeys, playerKey)
    if index ~= nil then
        if index > 1 then
            table.remove(self._playerKeys, index)
            table.insert(self._playerKeys, 1, playerKey)
        end
        return
    end
    error("Player key is missing from playerKeys: " .. playerKey)
end

function GameInstance:getPlayers()
    return self._players
end

function GameInstance:getPlayerKeys()
    return self._playerKeys
end

function GameInstance:getPlayerByIndex(index)
    ---@diagnostic disable-next-line: return-type-mismatch, need-check-nil
    return self._players[self._playerKeys[index + 1]]
end

function GameInstance:getPlayerByTag(tag)
    for index in ipairs(self._playerKeys) do
        local player = self:getPlayerByIndex(index - 1)
        if player.tag == tag then
            return player
        end
    end
    return nil
end

function GameInstance:addPlayerByClass(playerClass)
    local Player = require("Source.Player")

    GameInstanceRecords.AppendPlayer(self._players, self._playerKeys, Player.InitPlayer(playerClass))
end

function GameInstance:removePlayerByClass(playerClass)
    if #self._playerKeys <= 1 then
        return
    end
    for index, playerKey in ipairs(self._playerKeys) do
        local player = self:getPlayerByIndex(index - 1)
        if player:getClassPath() == playerClass then
            table.remove(self._playerKeys, index)
            self._players[playerKey] = nil
            return
        end
    end
end

function GameInstance:applyMapInfo(mapPath, position)
    if bool(mapPath) then
        self._cachedMap = mapPath
    end
    if position ~= nil then
        self:getPlayer():setMapPosition(position)
    end
end

function GameInstance:recordAddedActor(mapPath, actor, layerName)
    local actorRecord = GameInstanceRecords.BuildAddedActorRecord(actor, layerName)
    if actorRecord == nil then
        return
    end
    mapPath = MapPath.Normalise(mapPath)
    local records = self._cachedAddedActors[mapPath] or {}
    self._cachedAddedActors[mapPath] = records
    GameInstanceRecords.UpsertTaggedRecord(records, actorRecord)
end

function GameInstance:recordTerrainDestruction(mapPath, layerName, position, tileID)
    GameInstanceRecords.StoreTerrainChange(
        self._cachedTerrainDestructions, MapPath.Normalise(mapPath), layerName, position, tileID
    )
end

function GameInstance:getTerrainDestructions(mapPath)
    mapPath = MapPath.Normalise(mapPath)
    return self._cachedTerrainDestructions[mapPath] or {}
end

function GameInstance:recordTelepoint(mapPath, position, tag)
    mapPath = MapPath.Normalise(mapPath)
    local points = self._cachedTelepoints[mapPath] or {}
    self._cachedTelepoints[mapPath] = points
    GameInstanceRecords.AppendUniqueTelepoint(points, position, tag)
end

function GameInstance:getTelepoints(mapPath)
    return self._cachedTelepoints[MapPath.Normalise(mapPath)] or {}
end

---@param actor            Engine.Actor
---@param definitionRegion string
---@param currentRegion    string
---@param layerName        string
---@param actorPosition    sf.Vector2i
---@return Source.GameInstance.WorldMovedActorRecord | nil
---@diagnostic disable-next-line: unused
function GameInstance:_buildWorldMovedActorRecord(actor, definitionRegion, currentRegion, layerName, actorPosition)
    return GameInstanceRecords.BuildWorldMovedActorRecord(
        actor, definitionRegion, currentRegion, layerName, actorPosition
    )
end

function GameInstance:getCachedNewItem(itemID)
    return self._cachedNewItem[itemID] == true
end

function GameInstance:setCachedNewItem(itemID)
    self._cachedNewItem[itemID] = true
end

function GameInstance:getAddedActors(mapPath)
    return GameInstanceWorldPersistence.getAddedActors(self, mapPath)
end

function GameInstance:recordAddedActorPosition(mapPath, actor, actorPosition)
    return GameInstanceWorldPersistence.recordAddedActorPosition(self, mapPath, actor, actorPosition)
end

function GameInstance:recordActorPosition(mapPath, actor, actorPosition)
    return GameInstanceWorldPersistence.recordActorPosition(self, mapPath, actor, actorPosition)
end

function GameInstance:getActorPositions(mapPath)
    return GameInstanceWorldPersistence.getActorPositions(self, mapPath)
end

function GameInstance:recordWorldMovedActor(worldPath, actor, definitionRegion, currentRegion, layerName, actorPosition)
    return GameInstanceWorldPersistence.recordWorldMovedActor(
        self, worldPath, actor, definitionRegion, currentRegion, layerName, actorPosition
    )
end

function GameInstance:removeWorldMovedActor(worldPath, actorTag)
    return GameInstanceWorldPersistence.removeWorldMovedActor(self, worldPath, actorTag)
end

function GameInstance:getWorldMovedActors(worldPath)
    return GameInstanceWorldPersistence.getWorldMovedActors(self, worldPath)
end

function GameInstance:_validateWorldActorRecordTags()
    return GameInstanceWorldPersistence._validateWorldActorRecordTags(self)
end

function GameInstance:recordDestroyedActorTag(mapPath, actorTag)
    return GameInstanceWorldPersistence.recordDestroyedActorTag(self, mapPath, actorTag)
end

function GameInstance:recordDestroyedActor(mapPath, actor)
    return GameInstanceWorldPersistence.recordDestroyedActor(self, mapPath, actor)
end

function GameInstance:getDestroyedActors(mapPath)
    return GameInstanceWorldPersistence.getDestroyedActors(self, mapPath)
end

return class(GameInstance)
