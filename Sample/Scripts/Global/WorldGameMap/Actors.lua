local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameMap = require("Global.GameMap")

local Actor = Engine.Actor
local WorldRegionState = GlobalCore.WorldRegionState

local WorldGameMapActors = {}

---@param actors Engine.Actor[]
---@param actor  Engine.Actor
local function appendActorOnce(actors, actor)
    if table.contains(actors, actor) then
        return
    end
    actors[#actors + 1] = actor
end

---@param player        Engine.Actor | nil
---@param actorLayers   table<Engine.Actor, string>
---@param actorsByLayer table<string, Engine.Actor[]>
local function keepPlayerAtLayerEnd(player, actorLayers, actorsByLayer)
    if player == nil then
        return
    end
    local layerName = actorLayers[player]
    if layerName == nil then
        return
    end
    local actors = actorsByLayer[layerName]
    if actors == nil or actors[#actors] == player then
        return
    end
    local index = table.index(actors, player)
    if index ~= nil then
        table.remove(actors, index)
        actors[#actors + 1] = player
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

---@param actorsByLayer table<string, Engine.Actor[]>
---@param roots         Engine.Actor[]
---@return boolean
local function removeRootsFromLiveActors(actorsByLayer, roots)
    local targets = {}
    for _, root in ipairs(roots) do
        for _, actor in ipairs(root:collectTree()) do
            targets[actor] = true
        end
    end
    local changed = false
    for layerName, actors in pairs(actorsByLayer) do
        local kept = {}
        for _, actor in ipairs(actors) do
            if targets[actor] then
                changed = true
            else
                kept[#kept + 1] = actor
            end
        end
        actorsByLayer[layerName] = kept
    end
    return changed
end

---@param payload Global.WorldGameMap.RegionPayload
---@param root    Engine.Actor
local function removeRegionRootMetadata(payload, root)
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

---@param actorsByTag        table<string, Engine.Actor>
---@param reservedTagSources table<string, string>
---@param tag                string
---@param source             string
---@param actor              Engine.Actor
local function claimWorldTag(actorsByTag, reservedTagSources, tag, source, actor)
    local resident = actorsByTag[tag]
    assert(resident == nil or resident == actor, "Duplicate world MapTag: " .. tag)
    local existingSource = reservedTagSources[tag]
    assert(
        existingSource == nil or existingSource == "reserved" or existingSource == source,
        "Duplicate world MapTag: " .. tag
    )
    reservedTagSources[tag] = source
    actorsByTag[tag] = actor
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

---@param self WorldGameMapImplState
function WorldGameMapActors.InitialiseWorldActorState(self, config, reservedTags)
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
---@param self                      WorldGameMapImplState
function WorldGameMapActors.SetDestroyedActorTagProvider(self, destroyedActorTagProvider)
    self._worldDestroyedActorTagProvider = destroyedActorTagProvider
    self:_refreshSuppressedActorTags()
end

---@param addedActorPositionRecorder fun(actor: Engine.Actor, position: sf.Vector2i)
---@param self                       WorldGameMapImplState
function WorldGameMapActors.SetAddedActorPositionPersistenceCallback(self, addedActorPositionRecorder)
    self._worldAddedActorPositionRecorder = addedActorPositionRecorder
end

---@return boolean
---@param self WorldGameMapImplState
function WorldGameMapActors.RefreshSuppressedActorTags(self)
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

---@param self WorldGameMapImplState
function WorldGameMapActors.ApplySuppressedActorTags(self)
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

---@param tag  string
---@param self WorldGameMapImplState
function WorldGameMapActors.SuppressActorTag(self, tag)
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
---@param self    WorldGameMapImplState
function WorldGameMapActors.FilterSuppressedRegionActors(self, region, payload)
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
                removeRegionRootMetadata(payload, root)
                self:_unindexWorldActorTree(root)
            end
        end
        payload.actors[layerName] = kept
    end
end

---@param layerName string
---@param self      WorldGameMapImplState
function WorldGameMapActors.EnsureWorldLayer(self, layerName)
    if self._worldLayerNames[layerName] then
        return
    end
    self._worldLayerNames[layerName] = true
    self._worldLayerOrder[#self._worldLayerOrder + 1] = layerName
end

---@param position sf.Vector2i
---@return string
---@param self     WorldGameMapImplState
function WorldGameMapActors.GetRuntimeTagNamespace(self, position)
    local regionIndex = self:getSparseWorldRegionIndexAt(position)
    local region = regionIndex ~= nil and self._worldRegions[regionIndex] or nil
    if region ~= nil then
        local stem = os.path.splitext(os.path.basename(region.map))
        return stem
    end
    return self._worldConfig.worldName
end

---@param tag  string
---@param self WorldGameMapImplState
function WorldGameMapActors.TrackRuntimeTag(self, tag)
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
---@param self     WorldGameMapImplState
function WorldGameMapActors.AllocateRuntimeTag(self, position)
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
---@param self      WorldGameMapImplState
function WorldGameMapActors.IndexRegionActor(self, payload, layerName, actor, region, root)
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
    claimWorldTag(
        self._worldActorsByTag, self._worldReservedTagSources, tag, actorTagSource(actor, definitionRegion), actor
    )
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
---@param self   WorldGameMapImplState
function WorldGameMapActors.IndexRegionActors(self, region)
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
---@param self  WorldGameMapImplState
function WorldGameMapActors.RegisterWorldActorTree(self, actor, layer)
    self:_ensureWorldLayer(layer)
    for _, listed in ipairs(actor:collectTree()) do
        if not bool(listed:getMapTag()) then
            listed:setMapTag(self:_allocateRuntimeTag(listed:getMapPosition()))
        end
        local tag = listed:getMapTag()
        self:_trackRuntimeTag(tag)
        claimWorldTag(self._worldActorsByTag, self._worldReservedTagSources, tag, "persisted:" .. tag, listed)
        self._worldActorLayers[listed] = layer
        self._worldActorRoots[listed] = actor
    end
    self._worldRootStates[actor] = self._worldRootStates[actor] or "NeverActive"
    self:_rememberWorldRootPosition(actor)
end

---@param actor Engine.Actor
---@param self  WorldGameMapImplState
function WorldGameMapActors.UnindexWorldActorTree(self, actor)
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
---@param self             WorldGameMapImplState
function WorldGameMapActors.AttachRegionRoot(self, region, actor, layer, definitionRegion)
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
---@param self            WorldGameMapImplState
function WorldGameMapActors.SpawnActor(self, actor, layer, emitCreateEvent)
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
---@param self             WorldGameMapImplState
function WorldGameMapActors.SpawnPersistedWorldActor(self, actor, layer, definitionRegion, emitCreateEvent)
    assert(bool(definitionRegion), "Persisted world Actor definition region must be a non-empty map path")
    local regionIndex = self:getSparseWorldRegionIndexAt(actor:getMapPosition())
    local region = regionIndex ~= nil and self._worldRegions[regionIndex] or nil
    assert(region == nil or region.payload ~= nil, "Cannot restore Actor into unloaded world region")
    assert(self._worldActorDefinitionRegions[actor] == nil, "World Actor definition region is already registered")
    self._worldActorDefinitionRegions[actor] = definitionRegion
    return self:spawnActor(actor, layer, emitCreateEvent)
end

---@param self WorldGameMapImplState
function WorldGameMapActors.GetAllActors(self)
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

---@param self WorldGameMapImplState
function WorldGameMapActors.UpdateActorList(self)
    keepPlayerAtLayerEnd(self._player, self._worldActorLayers, self._actors)
    self:_syncActorViews(self._actors)
end

---@param actor Engine.Actor
---@param self  WorldGameMapImplState
function WorldGameMapActors.DestroyActor(self, actor)
    self._worldDestroyedRootsDirty = true
    GameMap.destroyActor(self, actor)
end

---@param self WorldGameMapImplState
function WorldGameMapActors.GetActorByTag(self, tag)
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

---@param self WorldGameMapImplState
function WorldGameMapActors.RemoveActorsByTags(self, tags)
    if not bool(tags) then
        return
    end
    for _, tag in ipairs(tags) do
        self:suppressActorTag(tag)
    end
end

---@param self WorldGameMapImplState
function WorldGameMapActors.GetActorLayer(self, actor)
    local layer = self._worldActorLayers[actor]
    if layer ~= nil then
        return layer
    end
    return GameMap.getActorLayer(self, actor)
end

---@param actor    Engine.Actor
---@param position sf.Vector2i | nil
---@param self     WorldGameMapImplState
function WorldGameMapActors.RecordWorldActorPosition(self, actor, position)
    position = position or actor:getMapPosition()
    local root = self._worldActorRoots[actor] or actor
    local regionIndex = self:getSparseWorldRegionIndexAt(position)
    local currentRegion = regionIndex ~= nil and self._worldRegions[regionIndex] or nil
    self:_recordWorldRootPosition(root, currentRegion ~= nil and currentRegion.path or "", position)
end

---@param actor             Engine.Actor
---@param currentRegionPath string
---@param position          sf.Vector2i
---@param self              WorldGameMapImplState
function WorldGameMapActors.RecordWorldRootPosition(self, actor, currentRegionPath, position)
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
---@param self     WorldGameMapImplState
function WorldGameMapActors.RememberWorldRootPosition(self, root, position)
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
---@param self WorldGameMapImplState
function WorldGameMapActors.GetChangedWorldRootPosition(self, root)
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
---@param self         WorldGameMapImplState
function WorldGameMapActors.IsWorldActorLayerVisible(self, actor, layerName, visibleRect)
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
    local left = visibleRect.x * Engine.GetCellSize()
    local top = visibleRect.y * Engine.GetCellSize()
    local right = (visibleRect.x + visibleRect.width) * Engine.GetCellSize()
    local bottom = (visibleRect.y + visibleRect.height) * Engine.GetCellSize()
    return bounds.position.x + bounds.size.x >= left and bounds.position.x <= right
        and bounds.position.y + bounds.size.y >= top and bounds.position.y <= bottom
end

---@param roots Engine.Actor[]
---@param root  Engine.Actor
function WorldGameMapActors.RemoveWorldRoot(roots, root)
    removeRoot(roots, root)
end

---@param roots Engine.Actor[]
---@param root  Engine.Actor
function WorldGameMapActors.AppendWorldActorOnce(roots, root)
    appendActorOnce(roots, root)
end

---@param root Engine.Actor
---@param self WorldGameMapImplState
function WorldGameMapActors.SleepWorldRoot(self, root)
    self:_sleepWorldRoots({ root })
end

---@param roots Engine.Actor[]
---@param self  WorldGameMapImplState
function WorldGameMapActors.SleepWorldRoots(self, roots)
    local sleeping = {}
    local sleepTime = perfCounter()
    for _, root in ipairs(roots) do
        if self._worldRootStates[root] == "Active" then
            sleeping[#sleeping + 1] = root
            self._worldRootStates[root] = "Dormant"
            self._worldRootSleepTimes[root] = sleepTime
        end
    end
    if not bool(sleeping) then
        return
    end
    if removeRootsFromLiveActors(self._actors, sleeping) then
        self:updateActorList()
        self._materialDirty = true
    end
    for _, root in ipairs(sleeping) do
        for _, actor in ipairs(root:collectTree()) do
            if not actor:isDestroyed() then
                Actor.BlueprintEvent(actor, Actor, "onWorldSleep")
            end
        end
    end
end

---@param roots Engine.Actor[]
---@param self  WorldGameMapImplState
function WorldGameMapActors.ActivateWorldRoots(self, roots)
    if not bool(roots) then
        return
    end
    local waking = {}
    self:beginActorBatch()
    for _, root in ipairs(roots) do
        local wasDormant = self._worldRootStates[root] == "Dormant"
        if self._worldRootStates[root] ~= "Active" and not root:isDestroyed() then
            self:_addActorTreeToLayer(root, self._worldActorLayers[root])
            if wasDormant then
                waking[#waking + 1] = root
            end
            self._worldRootStates[root] = "Active"
        end
    end
    self:endActorBatch()
    self:initialiseActorsAndComponents()
    local wakeTime = perfCounter()
    for _, root in ipairs(waking) do
        local elapsedSeconds = math.max(0.0, wakeTime - (self._worldRootSleepTimes[root] or wakeTime))
        for _, actor in ipairs(root:collectTree()) do
            if not actor:isDestroyed() then
                Actor.BlueprintEvent(actor, Actor, "onWorldWake", { elapsedSeconds = elapsedSeconds })
            end
        end
        self._worldRootSleepTimes[root] = nil
    end
end

---@param payload Global.WorldGameMap.RegionPayload
---@param root    Engine.Actor
function WorldGameMapActors.RemoveRegionRootMetadata(payload, root)
    removeRegionRootMetadata(payload, root)
end

---@param payload Global.WorldGameMap.RegionPayload
---@param region  Source.SceneComponents.WorldRegionData
function WorldGameMapActors.InitialiseRegionActorPayload(payload, region)
    initialisePayloadActorState(payload, region)
end

return WorldGameMapActors
