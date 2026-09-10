local WorldGeometry = require("Global.WorldGeometry")

local WorldActorRecords = {}

local function findRegion(worldData, position)
    for _, region in ipairs(worldData.regions) do
        if WorldGeometry.RectContainsPosition(region, position) then
            return region
        end
    end
    return nil
end

local function resolveAddedRecord(record, position)
    local resolved = { bp = record.bp, layer = record.layer, position = copy(position), tag = record.tag }
    if record.classVarChanges ~= nil then
        resolved.classVarChanges = record.classVarChanges
    end
    return resolved
end

function WorldActorRecords.SelectAdded(worldData, inst, worldPath, targetRegion)
    local worldBounds = { x = 0, y = 0, width = worldData.width, height = worldData.height }
    local destroyedActors = {}
    for _, tag in ipairs(inst:getDestroyedActors(worldPath)) do
        destroyedActors[tag] = true
    end
    local actorPositions = inst:getActorPositions(worldPath)
    local selectedActors = {}
    for _, record in ipairs(inst:getAddedActors(worldPath)) do
        if not destroyedActors[record.tag] then
            local position = actorPositions[record.tag] or record.position
            assert(
                WorldGeometry.RectContainsPosition(worldBounds, position),
                "Saved world Actor position is outside world bounds: " .. record.tag
            )
            local matchedRegion = findRegion(worldData, position)
            local matchesTarget = targetRegion == nil and matchedRegion == nil
            if targetRegion ~= nil and matchedRegion ~= nil then
                matchesTarget = targetRegion.index == matchedRegion.index
            end
            if matchesTarget then
                selectedActors[#selectedActors + 1] = resolveAddedRecord(record, position)
            end
        end
    end
    return selectedActors
end

function WorldActorRecords.CollectMoved(worldData, inst, worldPath)
    local worldBounds = { x = 0, y = 0, width = worldData.width, height = worldData.height }
    local regionsByPath = {}
    local layers = {}
    local destroyedActors = {}
    for _, region in ipairs(worldData.regions) do
        regionsByPath[region.path] = region
    end
    for _, layerName in ipairs(worldData.layerOrder) do
        layers[layerName] = true
    end
    for _, tag in ipairs(inst:getDestroyedActors(worldPath)) do
        destroyedActors[tag] = true
    end
    local records = inst:getWorldMovedActors(worldPath)
    for _, record in ipairs(records) do
        assert(not destroyedActors[record.tag], "Moved world Actor is also destroyed: " .. record.tag)
        assert(
            regionsByPath[record.definitionRegion] ~= nil,
            "Moved world Actor definition region is not placed: " .. record.definitionRegion
        )
        assert(layers[record.layer], "Moved world Actor layer is not in world layerOrder: " .. record.layer)
        local position = record.position
        assert(
            WorldGeometry.RectContainsPosition(worldBounds, position),
            "Moved world Actor position is outside world bounds: " .. record.tag
        )
        local currentRegion = findRegion(worldData, position)
        local currentRegionPath = currentRegion ~= nil and currentRegion.path or ""
        assert(
            currentRegionPath == record.currentRegion,
            "Moved world Actor currentRegion does not match its position: " .. record.tag
        )
        assert(
            record.currentRegion ~= record.definitionRegion,
            "Moved world Actor record must be removed after returning to its definition region: " .. record.tag
        )
    end
    return records
end

function WorldActorRecords.CollectReservedTags(inst, worldPath)
    local result = {}
    local seen = {}
    local function append(tag)
        if bool(tag) and not seen[tag] then
            seen[tag] = true
            result[#result + 1] = tag
        end
    end
    for _, record in ipairs(inst:getAddedActors(worldPath)) do
        append(record.tag)
    end
    for _, record in ipairs(inst:getWorldMovedActors(worldPath)) do
        append(record.tag)
    end
    for tag in pairs(inst:getActorPositions(worldPath)) do
        append(tag)
    end
    for _, tag in ipairs(inst:getDestroyedActors(worldPath)) do
        append(tag)
    end
    return result
end

return WorldActorRecords
