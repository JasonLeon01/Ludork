local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameMap = require("Global.GameMap")

local Actor = Engine.Actor
local WorldRegionState = GlobalCore.WorldRegionState

---@type WorldGameMapImplState
local WorldGameMapActors = {}

---@param actors Engine.Actor[]
---@param actor  Engine.Actor
local function appendActorOnce(actors, actor)
    if table.contains(actors, actor) then
        return
    end
    actors[#actors + 1] = actor
end

---@param world Global.WorldGameMap.WorldGameMap
local function keepPlayerAtLayerEnd(world)
    if world._player == nil then
        return
    end
    local layerName = world._worldActorLayers[world._player]
    if layerName == nil then
        return
    end
    local actors = world._actors[layerName]
    if actors == nil or actors[#actors] == world._player then
        return
    end
    local index = table.index(actors, world._player)
    if index ~= nil then
        table.remove(actors, index)
        actors[#actors + 1] = world._player
    end
end

---@param roots Engine.Actor[]
---@param root  Engine.Actor
local function removeRoot(roots, root)
    local index = table.index(roots, root)
    if index ~= nil then
        table.remove(roots, index)
    end
end

---@param world Global.WorldGameMap.WorldGameMap
---@param roots Engine.Actor[]
local function removeRootsFromLiveActors(world, roots)
    local targets = {}
    for _, root in ipairs(roots) do
        for _, actor in ipairs(root:collectTree()) do
            targets[actor] = true
        end
    end
    local changed = false
    for layerName, actors in pairs(world._actors) do
        local kept = {}
        for _, actor in ipairs(actors) do
            if targets[actor] then
                changed = true
            else
                kept[#kept + 1] = actor
            end
        end
        world._actors[layerName] = kept
    end
    if changed then
        world:updateActorList()
        world._materialDirty = true
    end
end

---@param world Global.WorldGameMap.WorldGameMap
---@param roots Engine.Actor[]
local function sleepRoots(world, roots)
    local sleeping = {}
    local sleepTime = perfCounter()
    for _, root in ipairs(roots) do
        if world._worldRootStates[root] == "Active" then
            sleeping[#sleeping + 1] = root
            world._worldRootStates[root] = "Dormant"
            world._worldRootSleepTimes[root] = sleepTime
        end
    end
    if not bool(sleeping) then
        return
    end
    removeRootsFromLiveActors(world, sleeping)
    for _, root in ipairs(sleeping) do
        for _, actor in ipairs(root:collectTree()) do
            if not actor:isDestroyed() then
                Actor.BlueprintEvent(actor, Actor, "onWorldSleep")
            end
        end
    end
end

---@param world Global.WorldGameMap.WorldGameMap
---@param roots Engine.Actor[]
local function activateRoots(world, roots)
    if not bool(roots) then
        return
    end
    local waking = {}
    world:beginActorBatch()
    for _, root in ipairs(roots) do
        local state = world._worldRootStates[root]
        if state ~= "Active" and not root:isDestroyed() then
            local layerName = world._worldActorLayers[root]
            world:_addActorTreeToLayer(root, layerName)
            if state == "Dormant" then
                waking[#waking + 1] = root
            end
            world._worldRootStates[root] = "Active"
        end
    end
    world:endActorBatch()
    world:initialiseActorsAndComponents()
    local wakeTime = perfCounter()
    for _, root in ipairs(waking) do
        local elapsedSeconds = math.max(0.0, wakeTime - (world._worldRootSleepTimes[root] or wakeTime))
        for _, actor in ipairs(root:collectTree()) do
            if not actor:isDestroyed() then
                Actor.BlueprintEvent(actor, Actor, "onWorldWake", { elapsedSeconds = elapsedSeconds })
            end
        end
        world._worldRootSleepTimes[root] = nil
    end
end

---@param _world  Global.WorldGameMap.WorldGameMap
---@param payload Global.WorldGameMap.RegionPayload
---@param root    Engine.Actor
local function removeRegionRootMetadata(_world, payload, root)
    payload.activeRoots[root] = nil
    payload.definitionRegions[root] = nil
    for _, actor in ipairs(root:collectTree()) do
        payload.actorSet[actor] = nil
        payload.actorRoots[actor] = nil
    end
end

---@param actor            Engine.Actor
---@param definitionRegion string | nil
---@return string
local function actorTagSource(actor, definitionRegion)
    if definitionRegion ~= nil then
        return "authored:" .. definitionRegion
    end
    return "persisted:" .. actor:getMapTag()
end

---@param world  Global.WorldGameMap.WorldGameMap
---@param tag    string
---@param source string
---@param actor  Engine.Actor
local function claimWorldTag(world, tag, source, actor)
    local resident = world._worldActorsByTag[tag]
    assert(resident == nil or resident == actor, "Duplicate world MapTag: " .. tag)
    local existingSource = world._worldReservedTagSources[tag]
    assert(
        existingSource == nil or existingSource == "reserved" or existingSource == source,
        "Duplicate world MapTag: " .. tag
    )
    world._worldReservedTagSources[tag] = source
    world._worldActorsByTag[tag] = actor
end

---@param actor      Engine.Actor
---@param suppressed table<string, boolean>
---@param objects    table<Engine.Actor, boolean>
---@return boolean
local function filterSuppressedActorTree(actor, suppressed, objects)
    if suppressed[actor:getMapTag()] or objects[actor] then
        for _, listed in ipairs(actor:collectTree()) do
            objects[listed] = true
            listed:markDestroyed(true)
        end
        return false
    end
    local children = copy(actor:getChildren())
    for _, child in ipairs(children) do
        if not filterSuppressedActorTree(child, suppressed, objects) then
            actor:removeChild(child)
        end
    end
    return true
end

function WorldGameMapActors:_initialiseWorldActorState(config, reservedTags)
    self._worldActorsByTag = {}
    self._worldActorLayers = {}
    self._worldActorDefinitionRegions = {}
    self._worldActorRoots = {}
    self._worldActorRegions = {}
    self._worldRootStates = {}
    self._worldRootSleepTimes = {}
    self._worldLooseRoots = {}
    self._worldPendingRehomes = {}
    self._worldObservedRootPositions = {}
    self._worldDestroyedRootsDirty = false
    self._worldActiveChunkBounds = nil
    self._worldActiveChunkGeneration = 0
    self._worldActiveChunkReconcilePending = false
    self._worldLooseActiveChunkGeneration = -1
    self._worldActivationDeferred = false
    self._worldSuppressedActorTags = {}
    self._worldSuppressedActorObjects = setmetatable({}, { __mode = "k" })
    self._worldDestroyedActorTagProvider = nil
    self._worldAddedActorPositionRecorder = nil
    self._worldReservedTagSources = {}
    self._worldRuntimeTagIndices = {}
    self._worldLayerOrder = copy(config.layerOrder)
    self._worldLayerNames = {}
    for _, layerName in ipairs(self._worldLayerOrder) do
        self._worldLayerNames[layerName] = true
    end
    for _, tag in ipairs(reservedTags or {}) do
        self._worldReservedTagSources[tag] = "reserved"
        self:_trackRuntimeTag(tag)
    end
end

---@param destroyedActorTagProvider fun(): string[]
function WorldGameMapActors:setDestroyedActorTagProvider(destroyedActorTagProvider)
    self._worldDestroyedActorTagProvider = destroyedActorTagProvider
    self:_refreshSuppressedActorTags()
end

---@param addedActorPositionRecorder fun(actor: Engine.Actor, position: sf.Vector2i)
function WorldGameMapActors:setAddedActorPositionPersistenceCallback(addedActorPositionRecorder)
    self._worldAddedActorPositionRecorder = addedActorPositionRecorder
end

---@return boolean
function WorldGameMapActors:_refreshSuppressedActorTags()
    if self._worldDestroyedActorTagProvider == nil then
        return false
    end
    local changed = false
    for _, tag in ipairs(self._worldDestroyedActorTagProvider()) do
        if bool(tag) and not self._worldSuppressedActorTags[tag] then
            self._worldSuppressedActorTags[tag] = true
            self._worldReservedTagSources[tag] = "suppressed"
            self:_trackRuntimeTag(tag)
            changed = true
        end
    end
    return changed
end

function WorldGameMapActors:_applySuppressedActorTags()
    for tag in pairs(self._worldSuppressedActorTags) do
        local actor = self._worldActorsByTag[tag]
        if actor ~= nil and self._worldRootStates[self._worldActorRoots[actor] or actor] ~= "NeverActive" then
            actor:destroy()
        end
    end
    for _, region in ipairs(self._worldRegions) do
        if region.payload ~= nil then
            self:_filterSuppressedRegionActors(region, region.payload)
        elseif region.publishState ~= nil and region.publishState.payload ~= nil then
            self:_filterSuppressedRegionActors(region, region.publishState.payload)
        end
    end
    local keptLooseRoots = {}
    for _, root in ipairs(self._worldLooseRoots) do
        if filterSuppressedActorTree(root, self._worldSuppressedActorTags, self._worldSuppressedActorObjects) then
            keptLooseRoots[#keptLooseRoots + 1] = root
        else
            self:_unindexWorldActorTree(root)
        end
    end
    self._worldLooseRoots = keptLooseRoots
    for layerName, actors in pairs(self._actors) do
        local kept = {}
        for _, listed in ipairs(actors) do
            if not self._worldSuppressedActorObjects[listed] and not self._worldSuppressedActorTags[listed:getMapTag()] then
                kept[#kept + 1] = listed
            end
        end
        self._actors[layerName] = kept
    end
    self:updateActorList()
    self:markPassabilityDirty()
end

---@param tag string
function WorldGameMapActors:suppressActorTag(tag)
    if not bool(tag) then
        return
    end
    self._worldSuppressedActorTags[tag] = true
    self._worldReservedTagSources[tag] = "suppressed"
    self:_trackRuntimeTag(tag)
    self:_applySuppressedActorTags()
end

---@param region  Source.SceneComponents.WorldRegionData
---@param payload Global.WorldGameMap.RegionPayload
function WorldGameMapActors:_filterSuppressedRegionActors(region, payload)
    payload.worldRegion = region
    payload.actorSet = payload.actorSet or {}
    payload.actorRoots = payload.actorRoots or {}
    payload.activeRoots = payload.activeRoots or {}
    self:_refreshSuppressedActorTags()
    local filteredTags = copy(self._worldSuppressedActorTags)
    for tag in pairs(self:_getPendingWorldActorTags(region)) do
        filteredTags[tag] = true
    end
    if not bool(filteredTags) then
        return
    end
    for layerName, roots in pairs(payload.actors) do
        local kept = {}
        for _, root in ipairs(roots) do
            if filterSuppressedActorTree(root, filteredTags, self._worldSuppressedActorObjects) then
                kept[#kept + 1] = root
            else
                removeRegionRootMetadata(self, payload, root)
                self:_unindexWorldActorTree(root)
            end
        end
        payload.actors[layerName] = kept
    end
end

---@param layerName string
function WorldGameMapActors:_ensureWorldLayer(layerName)
    if self._worldLayerNames[layerName] then
        return
    end
    self._worldLayerNames[layerName] = true
    self._worldLayerOrder[#self._worldLayerOrder + 1] = layerName
end

---@param position sf.Vector2i
---@return string
function WorldGameMapActors:_getRuntimeTagNamespace(position)
    local regionIndex = self:getSparseWorldRegionIndexAt(position)
    local region = regionIndex ~= nil and self._worldRegions[regionIndex] or nil
    if region ~= nil then
        local stem = os.path.splitext(os.path.basename(region.map))
        return stem
    end
    return self._worldConfig.worldName
end

---@param tag string
function WorldGameMapActors:_trackRuntimeTag(tag)
    local prefix, suffix = tag:match("^(.-%.runtime_default_)(%d+)$")
    if prefix == nil or suffix == nil then
        return
    end
    local index = math.tointeger(tonumber(suffix))
    if index ~= nil and tostring(index) == suffix then
        self._worldRuntimeTagIndices[prefix] = math.max(self._worldRuntimeTagIndices[prefix] or 0, index)
    end
end

---@param position sf.Vector2i
---@return string
function WorldGameMapActors:_allocateRuntimeTag(position)
    local prefix = self:_getRuntimeTagNamespace(position) .. ".runtime_default_"
    local index = self._worldRuntimeTagIndices[prefix] or 0
    local tag
    repeat
        index = index + 1
        tag = prefix .. index
    until self._worldReservedTagSources[tag] == nil and self._worldActorsByTag[tag] == nil
    self._worldRuntimeTagIndices[prefix] = index
    self._worldReservedTagSources[tag] = "persisted:" .. tag
    return tag
end

---@param payload Global.WorldGameMap.RegionPayload
---@param region  Source.SceneComponents.WorldRegionData
local function initialisePayloadActorState(payload, region)
    payload.worldRegion = region
    payload.actorSet = payload.actorSet or {}
    payload.actorRoots = payload.actorRoots or {}
    payload.activeRoots = payload.activeRoots or {}
end

---@param payload   Global.WorldGameMap.RegionPayload
---@param layerName string
---@param actor     Engine.Actor
---@param region    Source.SceneComponents.WorldRegionData | nil
---@param root      Engine.Actor | nil
---@return boolean
function WorldGameMapActors:_indexRegionActor(payload, layerName, actor, region, root)
    local targetRegion = region or payload.worldRegion
    assert(targetRegion ~= nil, "World region Actor indexing requires its region")
    root = root or actor
    initialisePayloadActorState(payload, targetRegion)
    if self._worldSuppressedActorObjects[actor] or self._worldSuppressedActorTags[actor:getMapTag()]
        or self:_isPendingWorldActorTag(targetRegion, actor:getMapTag()) then
        return false
    end
    if not bool(actor:getMapTag()) then
        actor:setMapTag(self:_allocateRuntimeTag(actor:getMapPosition()))
    end
    local tag = actor:getMapTag()
    self:_trackRuntimeTag(tag)
    local definitionRegion = payload.definitionRegions[root]
    claimWorldTag(self, tag, actorTagSource(actor, definitionRegion), actor)
    self:_ensureWorldLayer(layerName)
    self._worldActorLayers[actor] = layerName
    self._worldActorRoots[actor] = root
    self._worldActorRegions[root] = targetRegion
    payload.actorSet[actor] = true
    payload.actorRoots[actor] = root
    if actor == root and self._worldRootStates[root] == nil then
        local wakeTime = targetRegion.wakeTags ~= nil and targetRegion.wakeTags[tag] or nil
        self._worldRootStates[root] = wakeTime ~= nil and "Dormant" or "NeverActive"
        self._worldRootSleepTimes[root] = wakeTime
        self:_rememberWorldRootPosition(root)
    end
    return true
end

---@param region Source.SceneComponents.WorldRegionData
function WorldGameMapActors:_indexRegionActors(region)
    local payload = assert(region.payload)
    initialisePayloadActorState(payload, region)
    for layerName, roots in pairs(payload.actors) do
        for _, root in ipairs(roots) do
            local definitionRegion = payload.definitionRegions[root]
            if definitionRegion ~= nil then
                self._worldActorDefinitionRegions[root] = definitionRegion
            end
            for _, actor in ipairs(root:collectTree()) do
                self:_indexRegionActor(payload, layerName, actor, region, root)
            end
        end
    end
end

---@param actor Engine.Actor
---@param layer string
function WorldGameMapActors:_registerWorldActorTree(actor, layer)
    self:_ensureWorldLayer(layer)
    for _, listed in ipairs(actor:collectTree()) do
        if not bool(listed:getMapTag()) then
            listed:setMapTag(self:_allocateRuntimeTag(listed:getMapPosition()))
        end
        local tag = listed:getMapTag()
        self:_trackRuntimeTag(tag)
        claimWorldTag(self, tag, "persisted:" .. tag, listed)
        self._worldActorLayers[listed] = layer
        self._worldActorRoots[listed] = actor
    end
    self._worldRootStates[actor] = self._worldRootStates[actor] or "NeverActive"
    self:_rememberWorldRootPosition(actor)
end

---@param actor Engine.Actor
function WorldGameMapActors:_unindexWorldActorTree(actor)
    for _, listed in ipairs(actor:collectTree()) do
        local tag = listed:getMapTag()
        if bool(tag) and self._worldActorsByTag[tag] == listed then
            self._worldActorsByTag[tag] = nil
        end
        self._worldActorLayers[listed] = nil
        self._worldActorRoots[listed] = nil
    end
    self._worldActorDefinitionRegions[actor] = nil
    self._worldActorRegions[actor] = nil
    self._worldRootStates[actor] = nil
    self._worldRootSleepTimes[actor] = nil
    self._worldPendingRehomes[actor] = nil
    self._worldObservedRootPositions[actor] = nil
end

---@param region           Source.SceneComponents.WorldRegionData
---@param actor            Engine.Actor
---@param layer            string
---@param definitionRegion string | nil
function WorldGameMapActors:_attachRegionRoot(region, actor, layer, definitionRegion)
    local payload = assert(region.payload, "World region is not loaded: " .. region.path)
    initialisePayloadActorState(payload, region)
    local roots = payload.actors[layer] or {}
    payload.actors[layer] = roots
    appendActorOnce(roots, actor)
    if definitionRegion ~= nil then
        payload.definitionRegions[actor] = definitionRegion
    end
    self._worldActorRegions[actor] = region
    for _, listed in ipairs(actor:collectTree()) do
        payload.actorSet[listed] = true
        payload.actorRoots[listed] = actor
    end
end

---@param actor           Engine.Actor
---@param layer           string
---@param emitCreateEvent boolean | nil
function WorldGameMapActors:spawnActor(actor, layer, emitCreateEvent)
    self:_refreshSuppressedActorTags()
    if not filterSuppressedActorTree(actor, self._worldSuppressedActorTags, self._worldSuppressedActorObjects) then
        local parent = actor:getParent()
        if parent ~= nil then
            parent:removeChild(actor)
        end
        return
    end
    if actor ~= self._player and not bool(actor:getMapTag()) then
        actor:setMapTag(self:_allocateRuntimeTag(actor:getMapPosition()))
    end
    self:_registerWorldActorTree(actor, layer)
    if self._player ~= nil and actor == self._player then
        self._worldRootStates[actor] = "Active"
        return GameMap.spawnActor(self, actor, layer, emitCreateEvent)
    end
    local regionIndex = self:getSparseWorldRegionIndexAt(actor:getMapPosition())
    local region = regionIndex ~= nil and self._worldRegions[regionIndex] or nil
    if region == nil then
        appendActorOnce(self._worldLooseRoots, actor)
        self:_syncLooseRootActivation()
        return
    end
    assert(region.payload ~= nil, "Cannot spawn Actor into unloaded world region: " .. region.path)
    self:_attachRegionRoot(region, actor, layer, self._worldActorDefinitionRegions[actor])
    if self._worldStreamingState:getRegionState(region.index) == WorldRegionState.Active then
        self:_syncRegionActorActivation(region)
    end
end

---@param actor            Engine.Actor
---@param layer            string
---@param definitionRegion string
---@param emitCreateEvent  boolean | nil
function WorldGameMapActors:spawnPersistedWorldActor(actor, layer, definitionRegion, emitCreateEvent)
    assert(bool(definitionRegion), "Persisted world Actor definition region must be a non-empty map path")
    local regionIndex = self:getSparseWorldRegionIndexAt(actor:getMapPosition())
    local region = regionIndex ~= nil and self._worldRegions[regionIndex] or nil
    assert(region == nil or region.payload ~= nil, "Cannot restore Actor into unloaded world region")
    assert(self._worldActorDefinitionRegions[actor] == nil, "World Actor definition region is already registered")
    self._worldActorDefinitionRegions[actor] = definitionRegion
    return self:spawnActor(actor, layer, emitCreateEvent)
end

function WorldGameMapActors:getAllActors()
    local actors = {}
    local seen = {}
    for _, actorList in pairs(self._actors) do
        for _, actor in ipairs(actorList) do
            if not seen[actor] then
                seen[actor] = true
                actors[#actors + 1] = actor
            end
        end
    end
    return actors
end

function WorldGameMapActors:updateActorList()
    keepPlayerAtLayerEnd(self)
    self:_syncActorViews(self._actors)
end

---@param actor Engine.Actor
function WorldGameMapActors:destroyActor(actor)
    self._worldDestroyedRootsDirty = true
    GameMap.destroyActor(self, actor)
end

function WorldGameMapActors:getActorByTag(tag)
    if self._worldSuppressedActorTags[tag] then
        return nil
    end
    local actor = self._worldActorsByTag[tag]
    if actor == nil or actor:isDestroyed() then
        return nil
    end
    local root = self._worldActorRoots[actor] or actor
    local state = self._worldRootStates[root]
    if state == "Active" or state == "Dormant" then
        return actor
    end
    return nil
end

function WorldGameMapActors:removeActorsByTags(tags)
    if not bool(tags) then
        return
    end
    for _, tag in ipairs(tags) do
        self:suppressActorTag(tag)
    end
end

function WorldGameMapActors:getActorLayer(actor)
    local layer = self._worldActorLayers[actor]
    if layer ~= nil then
        return layer
    end
    return GameMap.getActorLayer(self, actor)
end

---@param actor    Engine.Actor
---@param position sf.Vector2i | nil
function WorldGameMapActors:recordWorldActorPosition(actor, position)
    position = position or actor:getMapPosition()
    local root = self._worldActorRoots[actor] or actor
    local regionIndex = self:getSparseWorldRegionIndexAt(position)
    local currentRegion = regionIndex ~= nil and self._worldRegions[regionIndex] or nil
    self:_recordWorldRootPosition(root, currentRegion ~= nil and currentRegion.path or "", position)
end

---@param actor             Engine.Actor
---@param currentRegionPath string
---@param position          sf.Vector2i
function WorldGameMapActors:_recordWorldRootPosition(actor, currentRegionPath, position)
    local definitionRegion = self._worldActorDefinitionRegions[actor]
    if definitionRegion ~= nil then
        local actorTag = actor:getMapTag()
        assert(bool(actorTag), "Authored world Actor must have a non-empty MapTag")
        local layerName = self._worldActorLayers[actor]
        assert(layerName ~= nil, "Authored world Actor layer is not registered")
        assert
            (self._worldMovedActorRecorder, "World moved-Actor persistence is not configured")
            (actor, definitionRegion, currentRegionPath, layerName, position)
    elseif actor ~= self._player and self._worldAddedActorPositionRecorder ~= nil then
        self._worldAddedActorPositionRecorder(actor, position)
    end
end

---@param root     Engine.Actor
---@param position sf.Vector2i | nil
function WorldGameMapActors:_rememberWorldRootPosition(root, position)
    position = position or root:getMapPosition()
    local observed = self._worldObservedRootPositions[root]
    if observed == nil then
        observed = { x = position.x, y = position.y }
        self._worldObservedRootPositions[root] = observed
    else
        observed.x = position.x
        observed.y = position.y
    end
end

---@param root Engine.Actor
---@return sf.Vector2i | nil
function WorldGameMapActors:_getChangedWorldRootPosition(root)
    local position = root:getMapPosition()
    local observed = self._worldObservedRootPositions[root]
    if observed == nil then
        self:_rememberWorldRootPosition(root, position)
        return nil
    end
    if observed.x == position.x and observed.y == position.y then
        return nil
    end
    return position
end

---@param actor        Engine.Actor
---@param layerName    string
---@param visibleRect? Global.WorldGeometry.CellRect
---@return boolean
function WorldGameMapActors:_isWorldActorLayerVisible(actor, layerName, visibleRect)
    local root = self._worldActorRoots[actor] or actor
    ---@type Source.SceneComponents.WorldRegionData | nil
    local region = self._worldActorRegions[root]
    if region == nil and actor == self._player then
        local regionIndex = self:getSparseWorldRegionIndexAt(actor:getMapPosition())
        region = regionIndex ~= nil and self._worldRegions[regionIndex] or nil
    end
    if region ~= nil and region.payload ~= nil then
        local layer = region.payload.tilemap:getLayer(layerName)
        if layer ~= nil and not layer.visible then
            return false
        end
    end
    if visibleRect == nil then
        return true
    end
    local bounds = actor:getGlobalBounds()
    local left = visibleRect.x * Engine.CellSize
    local top = visibleRect.y * Engine.CellSize
    local right = (visibleRect.x + visibleRect.width) * Engine.CellSize
    local bottom = (visibleRect.y + visibleRect.height) * Engine.CellSize
    return bounds.position.x + bounds.size.x >= left and bounds.position.x <= right
        and bounds.position.y + bounds.size.y >= top and bounds.position.y <= bottom
end

---@param roots Engine.Actor[]
---@param root  Engine.Actor
---@diagnostic disable-next-line: unused
function WorldGameMapActors:_removeWorldRoot(roots, root)
    removeRoot(roots, root)
end

---@param roots Engine.Actor[]
---@param root  Engine.Actor
---@diagnostic disable-next-line: unused
function WorldGameMapActors:_appendWorldActorOnce(roots, root)
    appendActorOnce(roots, root)
end

---@param root Engine.Actor
function WorldGameMapActors:_sleepWorldRoot(root)
    sleepRoots(self, { root })
end

---@param roots Engine.Actor[]
function WorldGameMapActors:_sleepWorldRoots(roots)
    sleepRoots(self, roots)
end

---@param roots Engine.Actor[]
function WorldGameMapActors:_activateWorldRoots(roots)
    activateRoots(self, roots)
end

---@param payload Global.WorldGameMap.RegionPayload
---@param root    Engine.Actor
function WorldGameMapActors:_removeRegionRootMetadata(payload, root)
    removeRegionRootMetadata(self, payload, root)
end

---@param payload Global.WorldGameMap.RegionPayload
---@param region  Source.SceneComponents.WorldRegionData
---@diagnostic disable-next-line: unused
function WorldGameMapActors:_initialiseRegionActorPayload(payload, region)
    initialisePayloadActorState(payload, region)
end

return WorldGameMapActors
