local MapPath = require("Source.MapPath")

---@type GameInstanceImplState
local GameInstanceWorldPersistence = {}

function GameInstanceWorldPersistence:getAddedActors(mapPath)
    return self._cachedAddedActors[MapPath.Normalise(mapPath)] or {}
end

function GameInstanceWorldPersistence:recordAddedActorPosition(mapPath, actor, actorPosition)
    mapPath = MapPath.Normalise(mapPath)
    local actorTag = actor:getMapTag()
    if not bool(actorTag) then
        return
    end
    for _, record in ipairs(self._cachedAddedActors[mapPath] or {}) do
        if record.tag == actorTag then
            self:recordActorPosition(mapPath, actor, actorPosition)
            return
        end
    end
end

function GameInstanceWorldPersistence:recordActorPosition(mapPath, actor, actorPosition)
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

function GameInstanceWorldPersistence:getActorPositions(mapPath)
    return self._cachedActorPositions[MapPath.Normalise(mapPath)] or {}
end

function GameInstanceWorldPersistence:recordWorldMovedActor(
    worldPath, actor, definitionRegion, currentRegion, layerName, actorPosition
)
    assert(bool(layerName), "Moved world Actor layer must be a non-empty string")
    worldPath = MapPath.Normalise(worldPath)
    definitionRegion = MapPath.Normalise(definitionRegion)
    currentRegion = MapPath.Normalise(currentRegion)
    assert(bool(definitionRegion), "Moved world Actor definition region must be a non-empty map path")
    local actorTag = actor:getMapTag()
    if not bool(actorTag) then
        return
    end
    if currentRegion == definitionRegion then
        self:recordActorPosition(worldPath, actor, actorPosition)
        self:removeWorldMovedActor(worldPath, actorTag)
        return
    end
    if actorPosition == nil then
        actorPosition = actor:getMapPosition()
    end
    local records = self._cachedWorldMovedActors[worldPath] or {}
    self._cachedWorldMovedActors[worldPath] = records
    for _, record in ipairs(records) do
        if record.tag == actorTag then
            assert(
                record.definitionRegion == definitionRegion, "Moved world Actor definition region changed: " .. actorTag
            )
            if record.currentRegion == currentRegion and record.layer == layerName
                and record.position.x == actorPosition.x and record.position.y == actorPosition.y then
                return
            end
            record.currentRegion = currentRegion
            record.layer = layerName
            record.position = copy(actorPosition)
            return
        end
    end
    local actorRecord = self:_buildWorldMovedActorRecord(
        actor, definitionRegion, currentRegion, layerName, actorPosition
    )
    assert(actorRecord ~= nil, "Authored world Actor cannot be reconstructed from its class: " .. actorTag)
    records[#records + 1] = actorRecord
end

function GameInstanceWorldPersistence:removeWorldMovedActor(worldPath, actorTag)
    worldPath = MapPath.Normalise(worldPath)
    local records = self._cachedWorldMovedActors[worldPath]
    ---@cast records Source.GameInstance.WorldMovedActorRecord[] | nil
    if records == nil then
        return
    end
    for index, record in ipairs(records) do
        if record.tag == actorTag then
            table.remove(records, index)
            if not bool(records) then
                self._cachedWorldMovedActors[worldPath] = nil
            end
            return
        end
    end
end

function GameInstanceWorldPersistence:getWorldMovedActors(worldPath)
    return self._cachedWorldMovedActors[MapPath.Normalise(worldPath)] or {}
end

function GameInstanceWorldPersistence:_validateWorldActorRecordTags()
    for worldPath, movedActors in pairs(self._cachedWorldMovedActors) do
        local addedTags = {}
        for _, record in ipairs(self._cachedAddedActors[worldPath] or {}) do
            addedTags[record.tag] = true
        end
        for _, record in ipairs(movedActors) do
            assert(
                not addedTags[record.tag],
                "Duplicate world Actor MapTag across addedActors and worldMovedActors in " .. worldPath
                    .. ": " .. record.tag
            )
        end
    end
end

function GameInstanceWorldPersistence:recordDestroyedActorTag(mapPath, actorTag)
    mapPath = MapPath.Normalise(mapPath)
    if not bool(actorTag) then
        return
    end
    local records = self._cachedDestroyedActors[mapPath] or {}
    self._cachedDestroyedActors[mapPath] = records
    if not table.contains(records, actorTag) then
        records[#records + 1] = actorTag
    end
    self:removeWorldMovedActor(mapPath, actorTag)
end

function GameInstanceWorldPersistence:recordDestroyedActor(mapPath, actor)
    self:recordDestroyedActorTag(mapPath, actor:getMapTag())
end

function GameInstanceWorldPersistence:getDestroyedActors(mapPath)
    return self._cachedDestroyedActors[MapPath.Normalise(mapPath)] or {}
end

return GameInstanceWorldPersistence
