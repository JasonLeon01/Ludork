local GlobalCore = require("GlobalCore")
local WorldMapConstants = require("Global.WorldMapConstants")
local WorldGameMapActors = require("Global.WorldGameMap.Actors")

local WorldRegionState = GlobalCore.WorldRegionState

local WorldGameMapActorStreaming = {}

---@param active Global.WorldGeometry.CellRect | nil
---@return integer | nil, integer | nil, integer | nil, integer | nil
local function getActiveChunkBounds(active)
    if active == nil or active.width <= 0 or active.height <= 0 then
        return nil, nil, nil, nil
    end
    local firstX = math.floor(active.x / WorldMapConstants.SPATIAL_CHUNK_SIZE)
    local firstY = math.floor(active.y / WorldMapConstants.SPATIAL_CHUNK_SIZE)
    local lastX = math.floor((active.x + active.width - 1) / WorldMapConstants.SPATIAL_CHUNK_SIZE)
    local lastY = math.floor((active.y + active.height - 1) / WorldMapConstants.SPATIAL_CHUNK_SIZE)
    return firstX, firstY, lastX - firstX + 1, lastY - firstY + 1
end

---@param world             Global.WorldGameMap.WorldGameMap
---@param roots             Engine.Actor[]
---@param active            Global.WorldGeometry.CellRect | nil
---@param region            Source.SceneComponents.WorldRegionData | nil
---@param pendingRehomes    table<Engine.Actor, Source.SceneComponents.WorldRegionData>
---@param suppressedObjects table<Engine.Actor, boolean>
---@return table<Engine.Actor, boolean>, boolean
local function collectDesiredRoots(world, roots, active, region, pendingRehomes, suppressedObjects)
    local desired = {}
    local destroyedRoots = false
    if active == nil or active.width <= 0 or active.height <= 0 then
        return desired, destroyedRoots
    end
    local firstX = math.floor(active.x / WorldMapConstants.SPATIAL_CHUNK_SIZE)
    local firstY = math.floor(active.y / WorldMapConstants.SPATIAL_CHUNK_SIZE)
    local lastX = math.floor((active.x + active.width - 1) / WorldMapConstants.SPATIAL_CHUNK_SIZE)
    local lastY = math.floor((active.y + active.height - 1) / WorldMapConstants.SPATIAL_CHUNK_SIZE)
    for _, root in ipairs(roots) do
        local position = root:getMapPosition()
        local chunkX = math.floor(position.x / WorldMapConstants.SPATIAL_CHUNK_SIZE)
        local chunkY = math.floor(position.y / WorldMapConstants.SPATIAL_CHUNK_SIZE)
        if chunkX >= firstX and chunkX <= lastX and chunkY >= firstY and chunkY <= lastY then
            if region == nil then
                if pendingRehomes[root] == nil then
                    desired[root] = true
                end
            elseif root:isDestroyed() then
                destroyedRoots = true
            elseif not suppressedObjects[root] and world:isSparseWorldCellReady(position) then
                desired[root] = true
            end
        end
    end
    return desired, destroyedRoots
end

---@param world             Global.WorldGameMap.WorldGameMap
---@param payload           Global.WorldGameMap.RegionPayload
---@param active            Global.WorldGeometry.CellRect | nil
---@param region            Source.SceneComponents.WorldRegionData
---@param pendingRehomes    table<Engine.Actor, Source.SceneComponents.WorldRegionData>
---@param suppressedObjects table<Engine.Actor, boolean>
---@return table<Engine.Actor, boolean>, boolean
local function collectDesiredRegionRoots(world, payload, active, region, pendingRehomes, suppressedObjects)
    local desired = {}
    local destroyedRoots = false
    for _, roots in pairs(payload.actors) do
        local desiredRoots, hasDestroyedRoots = collectDesiredRoots(
            world, roots, active, region, pendingRehomes, suppressedObjects
        )
        destroyedRoots = destroyedRoots or hasDestroyedRoots
        for root in pairs(desiredRoots) do
            desired[root] = true
        end
    end
    return desired, destroyedRoots
end

---@return boolean
---@param self WorldGameMapImplState
function WorldGameMapActorStreaming.UpdateWorldActiveChunkGeneration(self)
    local chunkX, chunkY, chunkWidth, chunkHeight = getActiveChunkBounds(self._worldActiveRect)
    local bounds = self._worldActiveChunkBounds
    if chunkX == nil then
        if bounds == nil then
            return false
        end
        self._worldActiveChunkBounds = nil
    elseif bounds ~= nil and bounds.x == chunkX and bounds.y == chunkY and bounds.width == chunkWidth
        and bounds.height == chunkHeight then
        return false
    else
        ---@cast chunkX integer
        ---@cast chunkY integer
        ---@cast chunkWidth integer
        ---@cast chunkHeight integer
        if bounds == nil then
            bounds = { x = chunkX, y = chunkY, width = chunkWidth, height = chunkHeight }
            self._worldActiveChunkBounds = bounds
        else
            bounds.x = chunkX
            bounds.y = chunkY
            bounds.width = chunkWidth
            bounds.height = chunkHeight
        end
    end
    self._worldActiveChunkGeneration = self._worldActiveChunkGeneration + 1
    self._worldActiveChunkReconcilePending = true
    return true
end

---@param self WorldGameMapImplState
function WorldGameMapActorStreaming.SyncWorldActiveChunkActivation(self)
    self:_updateWorldActiveChunkGeneration()
    if not self._worldActiveChunkReconcilePending then
        return
    end
    for _, region in ipairs(self._worldRegions) do
        if region.payload ~= nil and self._worldStreamingState:getRegionState(region.index) == WorldRegionState.Active
            and region.activeChunkGeneration ~= self._worldActiveChunkGeneration then
            self:_syncRegionActorActivation(region)
        end
    end
    if self._worldLooseActiveChunkGeneration ~= self._worldActiveChunkGeneration then
        self:_syncLooseRootActivation()
    end
    self._worldActiveChunkReconcilePending = false
end

---@param region Source.SceneComponents.WorldRegionData
---@param self   WorldGameMapImplState
function WorldGameMapActorStreaming.SyncRegionActorActivation(self, region)
    local payload = assert(region.payload)
    WorldGameMapActors.InitialiseRegionActorPayload(payload, region)
    self:_updateWorldActiveChunkGeneration()
    if self._worldActivationDeferred then
        region.activeChunkGeneration = nil
        self._worldActiveChunkReconcilePending = true
        return
    end
    local desired, destroyedRoots = collectDesiredRegionRoots(
        self, payload, self._worldActiveRect, region, self._worldPendingRehomes, self._worldSuppressedActorObjects
    )
    self._worldDestroyedRootsDirty = self._worldDestroyedRootsDirty or destroyedRoots
    local sleeping = {}
    for root in pairs(payload.activeRoots) do
        if not desired[root] then
            payload.activeRoots[root] = nil
            sleeping[#sleeping + 1] = root
        end
    end
    self:_sleepWorldRoots(sleeping)
    local activating = {}
    for root in pairs(desired) do
        if not payload.activeRoots[root] then
            payload.activeRoots[root] = true
            activating[#activating + 1] = root
        end
    end
    self:_activateWorldRoots(activating)
    region.activeChunkGeneration = self._worldActiveChunkGeneration
end

---@param self WorldGameMapImplState
function WorldGameMapActorStreaming.SyncLooseRootActivation(self)
    self:_updateWorldActiveChunkGeneration()
    if self._worldActivationDeferred then
        self._worldLooseActiveChunkGeneration = -1
        self._worldActiveChunkReconcilePending = true
        return
    end
    local desired = collectDesiredRoots(
        self, self._worldLooseRoots, self._worldActiveRect, nil, self._worldPendingRehomes,
        self._worldSuppressedActorObjects
    )
    if self._player ~= nil then
        desired[self._player] = true
    end
    local activating = {}
    local sleeping = {}
    for _, root in ipairs(self._worldLooseRoots) do
        if root:isDestroyed() then
            desired[root] = nil
        elseif desired[root] then
            if self._worldRootStates[root] ~= "Active" then
                activating[#activating + 1] = root
            end
        else
            sleeping[#sleeping + 1] = root
        end
    end
    self:_sleepWorldRoots(sleeping)
    self:_activateWorldRoots(activating)
    self._worldLooseActiveChunkGeneration = self._worldActiveChunkGeneration
end

---@param region Source.SceneComponents.WorldRegionData
---@return boolean
---@param self   WorldGameMapImplState
function WorldGameMapActorStreaming.ActivateRegion(self, region)
    self:_updateWorldActiveChunkGeneration()
    if self._worldActivationDeferred then
        if self._worldStreamingState:getRegionState(region.index) == WorldRegionState.Active then
            return false
        end
        local payload = assert(region.payload)
        self:_filterSuppressedRegionActors(region, payload)
        region.wasActive = true
        self._worldStreamingState:markActive(region.index)
        region.activeChunkGeneration = nil
        return true
    end
    if self._worldLooseActiveChunkGeneration ~= self._worldActiveChunkGeneration then
        self:_syncLooseRootActivation()
    end
    if self._worldStreamingState:getRegionState(region.index) == WorldRegionState.Active then
        if region.activeChunkGeneration ~= self._worldActiveChunkGeneration then
            self:_syncRegionActorActivation(region)
        end
        return false
    end
    local payload = assert(region.payload)
    self:_filterSuppressedRegionActors(region, payload)
    region.wasActive = true
    self._worldStreamingState:markActive(region.index)
    self:_syncRegionActorActivation(region)
    self:_refreshWorldLights()
    return true
end

---@param region Source.SceneComponents.WorldRegionData
---@param state  GlobalCore.WorldRegionState
---@param self   WorldGameMapImplState
function WorldGameMapActorStreaming.DeactivateRegion(self, region, state)
    local currentState = self._worldStreamingState:getRegionState(region.index)
    if currentState ~= WorldRegionState.Active then
        if state == WorldRegionState.Dormant and not region.wasActive then
            return
        end
        if currentState ~= state then
            self._worldStreamingState:markInactive(region.index, state)
        end
        region.activeChunkGeneration = nil
        return
    end
    local payload = assert(region.payload)
    local sleeping = {}
    for root in pairs(payload.activeRoots) do
        sleeping[#sleeping + 1] = root
    end
    payload.activeRoots = {}
    self:_sleepWorldRoots(sleeping)
    region.sleepTime = perfCounter()
    self._worldStreamingState:markInactive(region.index, state)
    region.activeChunkGeneration = nil
    self:_refreshWorldLights()
end

---@param region Source.SceneComponents.WorldRegionData
---@param self   WorldGameMapImplState
function WorldGameMapActorStreaming.EvictRegion(self, region)
    assert(
        self._worldStreamingState:getRegionState(region.index) ~= WorldRegionState.Active,
        "Cannot evict an Active world region: " .. region.path
    )
    assert(self._worldStreamingState:isRegionLoaded(region.index), "World region is not loaded: " .. region.path)
    local payload = assert(region.payload)
    region.wakeTags = {}
    local actors = {}
    local allRoots = {}
    for _, layerRoots in pairs(payload.actors) do
        for _, root in ipairs(layerRoots) do
            allRoots[#allRoots + 1] = root
            self:_recordWorldRootPosition(root, region.path, root:getMapPosition())
            if self._worldRootStates[root] == "Dormant" then
                region.wakeTags[root:getMapTag()] = self._worldRootSleepTimes[root] or region.sleepTime or perfCounter()
            end
        end
    end
    for actor in pairs(payload.actorSet) do
        actors[#actors + 1] = actor
    end
    self:_forgetActors(actors)
    for _, root in ipairs(allRoots) do
        self:_unindexWorldActorTree(root)
    end
    for _, actor in ipairs(actors) do
        local tag = actor:getMapTag()
        if bool(tag) and self._worldActorsByTag[tag] == actor then
            self._worldActorsByTag[tag] = nil
        end
        self._worldActorLayers[actor] = nil
        self._worldActorRoots[actor] = nil
    end
    payload.actorSet = {}
    payload.actorRoots = {}
    payload.activeRoots = {}
    payload.actors = {}
    payload.definitionRegions = {}
    payload.lights = {}
    GlobalCore.FogController.removeWorldRegionFog(region.path)
    self:_releaseWorldRegionTileMaskCache(region)
    self:detachSparseWorldRegion(region.index)
    region.backgroundBuilder = nil
    region.payload = nil
    self._worldStreamingState:markEvicted(region.index)
    region.activeChunkGeneration = nil
    self:markPassabilityDirty()
end

---@param self WorldGameMapImplState
function WorldGameMapActorStreaming.RefreshActorRegionDemands(self)
    local demands = {}
    local demanded = {}
    for root in pairs(self._worldPendingRehomes) do
        if root:isDestroyed() then
            self._worldPendingRehomes[root] = nil
        else
            local regionIndex = self:getSparseWorldRegionIndexAt(root:getMapPosition())
            local region = regionIndex ~= nil and self._worldRegions[regionIndex] or nil
            if region ~= nil and not self:isSparseWorldCellReady(root:getMapPosition()) and not demanded[region.index] then
                demanded[region.index] = true
                demands[#demands + 1] = region.index
            end
        end
    end
    return demands
end

---@param _region Source.SceneComponents.WorldRegionData
---@return table<string, boolean>
---@param self    WorldGameMapImplState
function WorldGameMapActorStreaming.GetPendingWorldActorTags(self, _region)
    local tags = {}
    for root in pairs(self._worldPendingRehomes) do
        local tag = root:getMapTag()
        if bool(tag) then
            tags[tag] = true
        end
    end
    return tags
end

---@param _region Source.SceneComponents.WorldRegionData
---@param tag     string | nil
---@return boolean
---@param self    WorldGameMapImplState
function WorldGameMapActorStreaming.IsPendingWorldActorTag(self, _region, tag)
    if not bool(tag) then
        return false
    end
    for root in pairs(self._worldPendingRehomes) do
        if root:getMapTag() == tag then
            return true
        end
    end
    return false
end

---@param root              Engine.Actor
---@param destinationRegion Source.SceneComponents.WorldRegionData
---@param sourceRegion      Source.SceneComponents.WorldRegionData | nil
---@param position          sf.Vector2i
---@param touchedRegions    table<Source.SceneComponents.WorldRegionData, boolean> | nil
---@return table<Source.SceneComponents.WorldRegionData, boolean> | nil, boolean
---@param self              WorldGameMapImplState
function WorldGameMapActorStreaming.QueuePendingWorldActorRehome(
    self, root, destinationRegion, sourceRegion, position, touchedRegions
)
    local looseTouched = false
    if sourceRegion ~= nil then
        local sourcePayload = assert(sourceRegion.payload)
        WorldGameMapActors.RemoveWorldRoot(sourcePayload.actors[self._worldActorLayers[root]] or {}, root)
        WorldGameMapActors.RemoveRegionRootMetadata(sourcePayload, root)
        touchedRegions = touchedRegions or {}
        touchedRegions[sourceRegion] = true
        WorldGameMapActors.AppendWorldActorOnce(self._worldLooseRoots, root)
        self._worldActorRegions[root] = nil
        looseTouched = true
    else
        looseTouched = true
    end
    self._worldPendingRehomes[root] = destinationRegion
    self._worldStreamingState:requestRegion(destinationRegion.index)
    self:_recordWorldRootPosition(root, destinationRegion.path, position)
    self:_rememberWorldRootPosition(root, position)
    self:_sleepWorldRoot(root)
    return touchedRegions, looseTouched
end

---@param root              Engine.Actor
---@param sourceRegion      Source.SceneComponents.WorldRegionData | nil
---@param destinationRegion Source.SceneComponents.WorldRegionData | nil
---@param layerName         string
---@param position          sf.Vector2i
---@param touchedRegions    table<Source.SceneComponents.WorldRegionData, boolean> | nil
---@return table<Source.SceneComponents.WorldRegionData, boolean> | nil, boolean
---@param self              WorldGameMapImplState
function WorldGameMapActorStreaming.TransferWorldActorRoot(
    self, root, sourceRegion, destinationRegion, layerName, position, touchedRegions
)
    local looseTouched = false
    if sourceRegion ~= nil then
        local sourcePayload = assert(sourceRegion.payload)
        WorldGameMapActors.RemoveWorldRoot(sourcePayload.actors[layerName] or {}, root)
        WorldGameMapActors.RemoveRegionRootMetadata(sourcePayload, root)
        touchedRegions = touchedRegions or {}
        touchedRegions[sourceRegion] = true
    else
        WorldGameMapActors.RemoveWorldRoot(self._worldLooseRoots, root)
        looseTouched = true
    end
    if destinationRegion ~= nil then
        self:_attachRegionRoot(destinationRegion, root, layerName, self._worldActorDefinitionRegions[root])
        touchedRegions = touchedRegions or {}
        touchedRegions[destinationRegion] = true
        if self._worldStreamingState:getRegionState(destinationRegion.index) == WorldRegionState.Active
            and self._worldRootStates[root] == "Active" then
            assert(destinationRegion.payload).activeRoots[root] = true
        elseif self._worldStreamingState:getRegionState(destinationRegion.index) ~= WorldRegionState.Active then
            self:_sleepWorldRoot(root)
        end
    else
        WorldGameMapActors.AppendWorldActorOnce(self._worldLooseRoots, root)
        self._worldActorRegions[root] = nil
        looseTouched = true
    end
    self:_recordWorldRootPosition(root, destinationRegion ~= nil and destinationRegion.path or "", position)
    self:_rememberWorldRootPosition(root, position)
    self._worldPendingRehomes[root] = nil
    return touchedRegions, looseTouched
end

---@param touchedRegions table<Source.SceneComponents.WorldRegionData, boolean> | nil
---@param looseTouched   boolean
---@return table<Source.SceneComponents.WorldRegionData, boolean> | nil, boolean
---@param self           WorldGameMapImplState
function WorldGameMapActorStreaming.AdvancePendingWorldActorRehomes(self, touchedRegions, looseTouched)
    if not bool(self._worldPendingRehomes) then
        return touchedRegions, looseTouched
    end
    local pendingRoots = {}
    for root in pairs(self._worldPendingRehomes) do
        pendingRoots[#pendingRoots + 1] = root
    end
    for _, root in ipairs(pendingRoots) do
        if self._worldPendingRehomes[root] ~= nil and root:isDestroyed() then
            self._worldPendingRehomes[root] = nil
        elseif self._worldPendingRehomes[root] ~= nil then
            local position = root:getMapPosition()
            local regionIndex = self:getSparseWorldRegionIndexAt(position)
            local destinationRegion = regionIndex ~= nil and self._worldRegions[regionIndex] or nil
            if destinationRegion == nil or self:isSparseWorldCellReady(position) then
                local changedLoose
                touchedRegions, changedLoose = self:_transferWorldActorRoot(
                    root, self._worldActorRegions[root], destinationRegion, self._worldActorLayers[root], position,
                    touchedRegions
                )
                looseTouched = looseTouched or changedLoose
            elseif destinationRegion ~= self._worldPendingRehomes[root] then
                local changedLoose
                touchedRegions, changedLoose = self:_queuePendingWorldActorRehome(
                    root, destinationRegion, nil, position, touchedRegions
                )
                looseTouched = looseTouched or changedLoose
            end
        end
    end
    return touchedRegions, looseTouched
end

---@param root           Engine.Actor
---@param position       sf.Vector2i
---@param touchedRegions table<Source.SceneComponents.WorldRegionData, boolean> | nil
---@param looseTouched   boolean
---@param sourceRegion   Source.SceneComponents.WorldRegionData | nil
---@return table<Source.SceneComponents.WorldRegionData, boolean> | nil, boolean
---@param self           WorldGameMapImplState
function WorldGameMapActorStreaming.RehomeChangedWorldActorRoot(
    self, root, position, touchedRegions, looseTouched, sourceRegion
)
    if root:isDestroyed() then
        return touchedRegions, looseTouched
    end
    local regionIndex = self:getSparseWorldRegionIndexAt(position)
    local destinationRegion = regionIndex ~= nil and self._worldRegions[regionIndex] or nil
    if sourceRegion ~= nil and destinationRegion ~= nil and destinationRegion.path == sourceRegion.path then
        if self:isSparseWorldCellReady(position) then
            touchedRegions = touchedRegions or {}
            touchedRegions[sourceRegion] = true
            self:_recordWorldRootPosition(root, sourceRegion.path, position)
            self:_rememberWorldRootPosition(root, position)
        else
            local changedLoose
            touchedRegions, changedLoose = self:_queuePendingWorldActorRehome(
                root, sourceRegion, sourceRegion, position, touchedRegions
            )
            looseTouched = looseTouched or changedLoose
        end
    elseif destinationRegion == nil then
        if sourceRegion == nil then
            looseTouched = true
            self:_recordWorldRootPosition(root, "", position)
            self:_rememberWorldRootPosition(root, position)
        else
            local changedLoose
            touchedRegions, changedLoose = self:_transferWorldActorRoot(
                root, sourceRegion, nil, self._worldActorLayers[root], position, touchedRegions
            )
            looseTouched = looseTouched or changedLoose
        end
    else
        if not self:isSparseWorldCellReady(position) then
            if root:getCollisionEnabled() then
                self:ensureRegionLoadedAt(position)
            end
            if not self:isSparseWorldCellReady(position) then
                local changedLoose
                touchedRegions, changedLoose = self:_queuePendingWorldActorRehome(
                    root, destinationRegion, sourceRegion, position, touchedRegions
                )
                return touchedRegions, looseTouched or changedLoose
            end
        end
        local changedLoose
        touchedRegions, changedLoose = self:_transferWorldActorRoot(
            root, sourceRegion, destinationRegion, self._worldActorLayers[root], position, touchedRegions
        )
        looseTouched = looseTouched or changedLoose
    end
    return touchedRegions, looseTouched
end

---@param self WorldGameMapImplState
function WorldGameMapActorStreaming.RehomeRegionActors(self)
    local touchedRegions
    local looseTouched = false
    touchedRegions, looseTouched = self:_advancePendingWorldActorRehomes(touchedRegions, looseTouched)

    ---@type Engine.Actor[] | nil
    local changedRoots
    ---@type table<Engine.Actor, sf.Vector2i> | nil
    local changedPositions
    for _, sourceRegion in ipairs(self._worldRegions) do
        if sourceRegion.payload ~= nil
            and self._worldStreamingState:getRegionState(sourceRegion.index) == WorldRegionState.Active then
            for root in pairs(sourceRegion.payload.activeRoots) do
                if root:isDestroyed() then
                    self._worldDestroyedRootsDirty = true
                else
                    local position = self:_getChangedWorldRootPosition(root)
                    if position ~= nil then
                        changedRoots = changedRoots or {}
                        changedPositions = changedPositions or {}
                        changedRoots[#changedRoots + 1] = root
                        changedPositions[root] = position
                    end
                end
            end
        end
    end
    ---@type Engine.Actor[] | nil
    local destroyedLooseRoots
    for _, root in ipairs(self._worldLooseRoots) do
        if root:isDestroyed() then
            destroyedLooseRoots = destroyedLooseRoots or {}
            destroyedLooseRoots[#destroyedLooseRoots + 1] = root
        else
            local position = self:_getChangedWorldRootPosition(root)
            if position ~= nil then
                changedRoots = changedRoots or {}
                changedPositions = changedPositions or {}
                changedRoots[#changedRoots + 1] = root
                changedPositions[root] = position
            end
        end
    end
    if destroyedLooseRoots ~= nil then
        for _, root in ipairs(destroyedLooseRoots) do
            WorldGameMapActors.RemoveWorldRoot(self._worldLooseRoots, root)
            self:_unindexWorldActorTree(root)
            looseTouched = true
        end
    end
    if changedRoots ~= nil then
        for _, root in ipairs(changedRoots) do
            local position = assert(changedPositions)[root]
            ---@cast position - nil
            touchedRegions, looseTouched = self:_rehomeChangedWorldActorRoot(
                root, position, touchedRegions, looseTouched
            )
        end
    end
    if touchedRegions ~= nil then
        for _, region in ipairs(self._worldRegions) do
            if touchedRegions[region] and region.payload ~= nil
                and self._worldStreamingState:getRegionState(region.index) == WorldRegionState.Active then
                self:_syncRegionActorActivation(region)
            end
        end
    end
    if looseTouched then
        self:_syncLooseRootActivation()
    end
end

---@param self WorldGameMapImplState
function WorldGameMapActorStreaming.PruneDestroyedRegionActors(self)
    if not self._worldDestroyedRootsDirty then
        return
    end
    self._worldDestroyedRootsDirty = false
    for _, region in ipairs(self._worldRegions) do
        if region.payload ~= nil then
            for layerName, roots in pairs(region.payload.actors) do
                local kept = {}
                for _, root in ipairs(roots) do
                    if root:isDestroyed() then
                        WorldGameMapActors.RemoveRegionRootMetadata(region.payload, root)
                        self:_unindexWorldActorTree(root)
                    else
                        kept[#kept + 1] = root
                    end
                end
                region.payload.actors[layerName] = kept
            end
        end
    end
end

return WorldGameMapActorStreaming
