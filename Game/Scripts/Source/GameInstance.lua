local GameVariables = require("Source.Configs.GameVariables")
local GameInstanceRecords = require("Source.Utils.GameInstance.Records")
local GameInstanceSaveCodec = require("Source.Utils.GameInstance.SaveCodec")
local GameInstanceWorldPersistence = require("Source.Utils.GameInstance.WorldPersistence")
local MapPath = require("Source.Utils.MapPath")
---@class (partial) Source.GameInstance.GameInstance
local GameInstance = {}

function GameInstance:init(skipDefaultPlayer)
    self._playerKeys = {}
    self._players = {}
    self._currentRegion = ""
    ---@type table<string, Source.GameInstance.RecordValue>
    self._variables = deepcopy(GameVariables)
    self._cachedMaps = {}
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
    local Player = require("Source.MapActors.Player")
    local GameSystem = require("Source.System")

    self._currentRegion = GameSystem.GetStartRegion()
    local firstPlayer = Player.InitPlayer(GameSystem.GetStartPlayerClassPath())
    firstPlayer:setMapPosition(GameSystem.GetStartPos())
    GameInstanceRecords.AppendPlayer(self._players, self._playerKeys, firstPlayer)
    self._cachedMaps[GameInstanceRecords.RequirePlayerKey(firstPlayer)] = MapPath.Normalise(GameSystem.GetStartMap())
end

function GameInstance:asDict()
    return GameInstanceSaveCodec.Encode({
        playerKeys = self._playerKeys,
        players = self._players,
        currentRegion = self._currentRegion,
        variables = self._variables,
        currentMaps = self._cachedMaps,
        addedActors = self._cachedAddedActors,
        actorPositions = self._cachedActorPositions,
        worldMovedActors = self._cachedWorldMovedActors,
        destroyedActors = self._cachedDestroyedActors,
        terrainDestructions = self._cachedTerrainDestructions,
        obtainedItems = self._cachedNewItem,
        telepoints = self._cachedTelepoints,
        screenshot = self._screenshot
    })
end

function GameInstance.FromDict(data)
    local instance = GameInstance.new(true)
    instance:restoreFromData(data)
    return instance
end

function GameInstance:restoreFromData(data)
    local state = GameInstanceSaveCodec.Decode(data)
    GameInstanceWorldPersistence.ValidateWorldActorRecordTags(state.addedActors, state.worldMovedActors)
    self._playerKeys = state.playerKeys
    self._players = state.players
    self._currentRegion = state.currentRegion
    self._variables = state.variables
    self._cachedMaps = state.currentMaps
    self._cachedAddedActors = state.addedActors
    self._cachedActorPositions = state.actorPositions
    self._cachedWorldMovedActors = state.worldMovedActors
    self._cachedDestroyedActors = state.destroyedActors
    self._cachedTerrainDestructions = state.terrainDestructions
    self._cachedNewItem = state.obtainedItems
    self._cachedTelepoints = state.telepoints
    self._screenshot = state.screenshot
end

function GameInstance:getCurrentMapPath()
    return self._cachedMaps[self._playerKeys[1]]
end

function GameInstance:getVisitedMapPaths()
    return table.orderedStringKeys(self._cachedTelepoints)
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

function GameInstance:setPlayerByClass(playerClass)
    for index, playerKey in ipairs(self._playerKeys) do
        local player = self:getPlayerByIndex(index - 1)
        if player:getClassPath() == playerClass then
            self:setPlayer(playerKey)
            return
        end
    end
    error("Player class is not in the party: " .. tostring(playerClass))
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

function GameInstance:addPlayerByClass(playerClass, mapPath, position)
    assert(Class.isInstance(mapPath, "string") and bool(mapPath), "Added player map path must be a non-empty string")
    assert(position ~= nil, "Added player position is required")
    local Player = require("Source.MapActors.Player")
    local player = Player.InitPlayer(playerClass)
    player:setMapPosition(position)
    GameInstanceRecords.AppendPlayer(self._players, self._playerKeys, player)
    self._cachedMaps[GameInstanceRecords.RequirePlayerKey(player)] = MapPath.Normalise(mapPath)
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
            self._cachedMaps[playerKey] = nil
            return
        end
    end
end

function GameInstance:applyMapInfo(mapPath, position)
    if bool(mapPath) then
        self._cachedMaps[self._playerKeys[1]] = MapPath.Normalise(mapPath)
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

function GameInstance:getTelepointsForMap(mapKey)
    local target = MapPath.WithoutExtension(mapKey)
    for _, mapPath in ipairs(table.orderedStringKeys(self._cachedTelepoints)) do
        if MapPath.WithoutExtension(mapPath) == target then
            return deepcopy(self._cachedTelepoints[mapPath])
        end
    end
    return {}
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
    return GameInstanceWorldPersistence.GetAddedActors(self, mapPath)
end

function GameInstance:recordAddedActorPosition(mapPath, actor, actorPosition)
    return GameInstanceWorldPersistence.RecordAddedActorPosition(self, mapPath, actor, actorPosition)
end

function GameInstance:recordActorPosition(mapPath, actor, actorPosition)
    return GameInstanceWorldPersistence.RecordActorPosition(self, mapPath, actor, actorPosition)
end

function GameInstance:getActorPositions(mapPath)
    return GameInstanceWorldPersistence.GetActorPositions(self, mapPath)
end

function GameInstance:recordWorldMovedActor(worldPath, actor, definitionRegion, currentRegion, layerName, actorPosition)
    return GameInstanceWorldPersistence.RecordWorldMovedActor(
        self, worldPath, actor, definitionRegion, currentRegion, layerName, actorPosition
    )
end

function GameInstance:removeWorldMovedActor(worldPath, actorTag)
    return GameInstanceWorldPersistence.RemoveWorldMovedActor(self, worldPath, actorTag)
end

function GameInstance:getWorldMovedActors(worldPath)
    return GameInstanceWorldPersistence.GetWorldMovedActors(self, worldPath)
end

function GameInstance:_validateWorldActorRecordTags()
    return GameInstanceWorldPersistence.ValidateWorldActorRecordTags(
        self._cachedAddedActors, self._cachedWorldMovedActors
    )
end

function GameInstance:recordDestroyedActorTag(mapPath, actorTag)
    return GameInstanceWorldPersistence.RecordDestroyedActorTag(self, mapPath, actorTag)
end

function GameInstance:recordDestroyedActor(mapPath, actor)
    return GameInstanceWorldPersistence.RecordDestroyedActor(self, mapPath, actor)
end

function GameInstance:getDestroyedActors(mapPath)
    return GameInstanceWorldPersistence.GetDestroyedActors(self, mapPath)
end

return class(GameInstance)
