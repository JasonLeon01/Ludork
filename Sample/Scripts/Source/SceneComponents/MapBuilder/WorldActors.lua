local ActorTree = require("Global.ActorTree")
local WorldGameMap = require("Global.WorldGameMap")
local Data = require("Source.Data")
local WorldActorRecords = require("Source.SceneComponents.MapBuilder.WorldActorRecords")

---@diagnostic disable-next-line: cast-type-mismatch, inherited constructor type does not retain the derived init signature
---@cast WorldGameMap Class.ClassType<Global.WorldGameMap.WorldGameMap>

---@class (partial) Source.SceneComponents.SceneMapBuilder
local MapBuilderWorldActors = {}

---@param records      Source.GameInstance.WorldMovedActorRecord[]
---@param targetRegion Source.SceneComponents.WorldRegionData | nil
---@return Source.GameInstance.WorldMovedActorRecord[]
---@diagnostic disable-next-line: unused
function MapBuilderWorldActors:_selectWorldMovedActors(records, targetRegion)
    local targetPath = targetRegion ~= nil and targetRegion.path or ""
    local selected = {}
    for _, record in ipairs(records) do
        if record.currentRegion == targetPath then
            selected[#selected + 1] = record
        end
    end
    return selected
end

---@param actor           Engine.Actor
---@param destroyedActors table<string, boolean>
---@return boolean
function MapBuilderWorldActors:_pruneDestroyedActorTree(actor, destroyedActors)
    if destroyedActors[actor:getMapTag()] then
        return false
    end
    for _, child in ipairs(actor:getChildren()) do
        if not self:_pruneDestroyedActorTree(child, destroyedActors) then
            actor:removeChild(child)
        end
    end
    return true
end

---@param actorRecord          Source.GameInstance.AddedActorRecord | Source.GameInstance.WorldMovedActorRecord
---@param actorPositions       table<string, sf.Vector2i>
---@param destroyedActors      table<string, boolean>
---@param preserveRootPosition boolean
---@return Engine.Actor | nil
function MapBuilderWorldActors:_generatePersistedActor(
    actorRecord, actorPositions, destroyedActors, preserveRootPosition
)
    local actor = Data.GenActorFromClassPath(actorRecord.bp, actorRecord.tag, actorRecord.classVarChanges)
    if actor == nil or not self:_pruneDestroyedActorTree(actor, destroyedActors) then
        return nil
    end
    actor:setMapPosition(actorRecord.position)
    for _, listed in ipairs(ActorTree.Collect(actor)) do
        if listed ~= actor or not preserveRootPosition then
            local savedPosition = actorPositions[listed:getMapTag()]
            if savedPosition ~= nil then
                listed:setMapPosition(savedPosition)
            end
        end
    end
    return actor
end

---@param gameMap         Global.WorldGameMap.WorldGameMap
---@param movedActors     Source.GameInstance.WorldMovedActorRecord[]
---@param actorPositions  table<string, sf.Vector2i>
---@param destroyedActors table<string, boolean>
function MapBuilderWorldActors:_applyHoleWorldMovedActors(gameMap, movedActors, actorPositions, destroyedActors)
    local addedAny = false
    gameMap:beginActorBatch()
    for _, actorRecord in ipairs(movedActors) do
        assert(gameMap:getActorByTag(actorRecord.tag) == nil, "Duplicate restored world MapTag: " .. actorRecord.tag)
        local actor = assert(
            self:_generatePersistedActor(actorRecord, actorPositions, destroyedActors, true),
            "Failed to restore moved world Actor: " .. actorRecord.tag
        )
        gameMap:spawnPersistedWorldActor(actor, actorRecord.layer, actorRecord.definitionRegion, false)
        addedAny = true
    end
    gameMap:endActorBatch()
    if addedAny then
        gameMap:initialiseActorsAndComponents()
    end
end

function MapBuilderWorldActors:generateWorldGameMap(worldPath, worldData, inst, initialPosition)
    local holeAddedActors = WorldActorRecords.SelectAdded(worldData, inst, worldPath, nil)
    local initialMovedActors = WorldActorRecords.CollectMoved(worldData, inst, worldPath)
    local holeMovedActors = self:_selectWorldMovedActors(initialMovedActors, nil)
    ---@param region       Source.SceneComponents.WorldRegionData
    ---@param data         Source.SceneComponents.SerializedMapData
    ---@param priorityRect Global.WorldGeometry.CellRect | nil
    ---@return Global.WorldGameMap.RegionBuildState
    local function createRegionBuildState(region, data, priorityRect)
        local regionAddedActors = WorldActorRecords.SelectAdded(worldData, inst, worldPath, region)
        local movedActors = WorldActorRecords.CollectMoved(worldData, inst, worldPath)
        return self:createWorldRegionBuildState(
            worldData, region, data, inst, worldPath, regionAddedActors, movedActors, priorityRect
        )
    end
    local gameMap = WorldGameMap.new(
        worldData, createRegionBuildState, WorldActorRecords.CollectReservedTags(inst, worldPath)
    )
    gameMap:setDestroyedActorTagProvider(function ()
        return inst:getDestroyedActors(worldPath)
    end)
    gameMap:setAddedActorPositionPersistenceCallback(function (actor, position)
        inst:recordAddedActorPosition(worldPath, actor, position)
    end)
    gameMap:setMovedActorPersistenceCallback(function (actor, definitionRegion, currentRegion, layerName, position)
        inst:recordWorldMovedActor(worldPath, actor, definitionRegion, currentRegion, layerName, position)
    end)
    self:_configureInteractiveMap(gameMap)
    if initialPosition ~= nil then
        local initialRegion = gameMap:getRegionPosition(initialPosition)
        if initialRegion ~= nil then
            gameMap:ensureRegionLoadedAt(initialPosition)
        end
    end
    self:applyAddedActors(gameMap, holeAddedActors)
    local destroyedActors = {}
    for _, tag in ipairs(inst:getDestroyedActors(worldPath)) do
        destroyedActors[tag] = true
    end
    self:_applyHoleWorldMovedActors(gameMap, holeMovedActors, inst:getActorPositions(worldPath), destroyedActors)
    local actorPositions = {}
    local movedRootTags = {}
    for _, record in ipairs(initialMovedActors) do
        movedRootTags[record.tag] = true
    end
    for tag, position in pairs(inst:getActorPositions(worldPath)) do
        if not movedRootTags[tag] then
            actorPositions[tag] = position
        end
    end
    gameMap:applyActorPositions(actorPositions)
    gameMap:removeActorsByTags(inst:getDestroyedActors(worldPath))
    return gameMap
end

---@param actorsByTag table<string, Engine.Actor>
---@param root        Engine.Actor
---@diagnostic disable-next-line: unused
function MapBuilderWorldActors:_indexActorTreeByTag(actorsByTag, root)
    for _, actor in ipairs(ActorTree.Collect(root)) do
        local actorTag = actor:getMapTag()
        if actorTag ~= nil then
            assert(
                actorsByTag[actorTag] == nil or actorsByTag[actorTag] == actor,
                "Duplicate restored world MapTag: " .. actorTag
            )
            actorsByTag[actorTag] = actor
        end
    end
end

return class(MapBuilderWorldActors)
