local GlobalCore = require("GlobalCore")
local WorldGeometry = require("Global.WorldGeometry")
local WorldMapConstants = require("Global.WorldMapConstants")

---@class (partial) Global.WorldGameMap.WorldGameMap
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

---@param world      Global.WorldGameMap.WorldGameMap
---@param rootChunks table<string, Engine.Actor[]>
---@param active     Global.WorldGeometry.CellRect | nil
---@param region     Source.SceneComponents.WorldRegionData | nil
---@return table<Engine.Actor, boolean>
local function collectDesiredRoots(world, rootChunks, active, region)
    local desired = {}
    if active == nil or active.width <= 0 or active.height <= 0 then
        return desired
    end
    local firstX = math.floor(active.x / WorldMapConstants.SPATIAL_CHUNK_SIZE)
    local firstY = math.floor(active.y / WorldMapConstants.SPATIAL_CHUNK_SIZE)
    local lastX = math.floor((active.x + active.width - 1) / WorldMapConstants.SPATIAL_CHUNK_SIZE)
    local lastY = math.floor((active.y + active.height - 1) / WorldMapConstants.SPATIAL_CHUNK_SIZE)
    for chunkY = firstY, lastY do
        for chunkX = firstX, lastX do
            local roots = rootChunks[WorldGeometry.GridKey(chunkX, chunkY)] or {}
            for _, root in ipairs(roots) do
                if region == nil then
                    if world._worldPendingRehomes[root] == nil then
                        desired[root] = true
                    end
                elseif root:isDestroyed() then
                    world._worldDestroyedRootsDirty = true
                elseif not world._worldSuppressedActorObjects[root]
                    and world:_isWorldCellReady(region, root:getMapPosition()) then
                    desired[root] = true
                end
            end
        end
    end
    return desired
end

---@return boolean
function WorldGameMapActorStreaming:_updateWorldActiveChunkGeneration()
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

function WorldGameMapActorStreaming:_syncWorldActiveChunkActivation()
    self:_updateWorldActiveChunkGeneration()
    if not self._worldActiveChunkReconcilePending then
        return
    end
    for _, region in ipairs(self._worldRegions) do
        if region.payload ~= nil and region.state == "Active"
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
function WorldGameMapActorStreaming:_syncRegionActorActivation(region)
    local payload = assert(region.payload)
    self:_initialiseRegionActorPayload(payload, region)
    self:_updateWorldActiveChunkGeneration()
    if self._worldActivationDeferred then
        region.activeChunkGeneration = nil
        self._worldActiveChunkReconcilePending = true
        return
    end
    local desired = collectDesiredRoots(self, payload.rootChunks, self._worldActiveRect, region)
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

function WorldGameMapActorStreaming:_syncLooseRootActivation()
    self:_updateWorldActiveChunkGeneration()
    if self._worldActivationDeferred then
        self._worldLooseActiveChunkGeneration = -1
        self._worldActiveChunkReconcilePending = true
        return
    end
    local desired = collectDesiredRoots(self, self._worldLooseRootChunks, self._worldActiveRect, nil)
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
function WorldGameMapActorStreaming:_activateRegion(region)
    self:_updateWorldActiveChunkGeneration()
    if self._worldActivationDeferred then
        if region.state == "Active" then
            return false
        end
        local payload = assert(region.payload)
        self:_filterSuppressedRegionActors(region, payload)
        region.wasActive = true
        region.state = "Active"
        region.lastUsed = perfCounter()
        region.preparedEvicted = nil
        region.activeChunkGeneration = nil
        self._worldCacheBytesDirty = true
        return true
    end
    if self._worldLooseActiveChunkGeneration ~= self._worldActiveChunkGeneration then
        self:_syncLooseRootActivation()
    end
    if region.state == "Active" then
        if region.activeChunkGeneration ~= self._worldActiveChunkGeneration then
            self:_syncRegionActorActivation(region)
        end
        return false
    end
    local payload = assert(region.payload)
    self:_filterSuppressedRegionActors(region, payload)
    region.wasActive = true
    region.state = "Active"
    region.lastUsed = perfCounter()
    region.preparedEvicted = nil
    self:_syncRegionActorActivation(region)
    self._worldCacheBytesDirty = true
    self:_refreshWorldLights()
    return true
end

---@param region Source.SceneComponents.WorldRegionData
---@param state  "Prepared" | "Dormant"
function WorldGameMapActorStreaming:_deactivateRegion(region, state)
    if region.state ~= "Active" then
        if state == "Dormant" and not region.wasActive then
            return
        end
        if region.state ~= state and state == "Prepared" then
            region.lastUsed = perfCounter()
        end
        region.state = state
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
    region.lastUsed = region.sleepTime
    region.state = state
    region.activeChunkGeneration = nil
    self._worldCacheBytesDirty = true
    self:_refreshWorldLights()
end

---@param region Source.SceneComponents.WorldRegionData
function WorldGameMapActorStreaming:_evictRegion(region)
    assert(region.state ~= "Active", "Cannot evict an Active world region: " .. region.path)
    assert(self._worldLoadedRegions[region], "World region is missing from the loaded set: " .. region.path)
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
    payload.rootChunks = {}
    payload.rootChunkKeys = {}
    payload.actors = {}
    payload.definitionRegions = {}
    payload.lights = {}
    GlobalCore.FogController.removeWorldRegionFog(region.path)
    self:_releaseWorldRegionTileMaskCache(region)
    region.backgroundBuilder = nil
    region.payload = nil
    region.payloadBytes = nil
    self._worldLoadedRegions[region] = nil
    region.preparedEvicted = region.demand == "Prepared" or nil
    region.state = "Unloaded"
    region.activeChunkGeneration = nil
    self._worldCacheBytesDirty = true
    self:markPassabilityDirty()
end

function WorldGameMapActorStreaming:_refreshActorRegionDemands()
    self._worldActorDemandRegions = {}
    for root in pairs(self._worldPendingRehomes) do
        if root:isDestroyed() then
            self._worldPendingRehomes[root] = nil
        else
            local region = self:_findRegionAt(root:getMapPosition())
            if region ~= nil and not self:_isWorldCellReady(region, root:getMapPosition()) then
                self._worldActorDemandRegions[region] = true
            end
        end
    end
end

---@param _region Source.SceneComponents.WorldRegionData
---@return table<string, boolean>
function WorldGameMapActorStreaming:_getPendingWorldActorTags(_region)
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
function WorldGameMapActorStreaming:_isPendingWorldActorTag(_region, tag)
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

---@param world             Global.WorldGameMap.WorldGameMap
---@param root              Engine.Actor
---@param destinationRegion Source.SceneComponents.WorldRegionData
---@param sourceRegion      Source.SceneComponents.WorldRegionData | nil
---@param position          sf.Vector2i
---@param touchedRegions    table<Source.SceneComponents.WorldRegionData, boolean> | nil
---@return table<Source.SceneComponents.WorldRegionData, boolean> | nil, boolean
local function queuePendingRehome(world, root, destinationRegion, sourceRegion, position, touchedRegions)
    local looseTouched = false
    if sourceRegion ~= nil then
        local layerName = world._worldActorLayers[root]
        local sourcePayload = assert(sourceRegion.payload)
        world:_removeWorldRoot(sourcePayload.actors[layerName] or {}, root)
        world:_removeRegionRootMetadata(sourcePayload, root)
        touchedRegions = touchedRegions or {}
        touchedRegions[sourceRegion] = true
        world:_appendWorldActorOnce(world._worldLooseRoots, root)
        world._worldActorRegions[root] = nil
        world:_refreshLooseRootChunk(root)
        looseTouched = true
    else
        looseTouched = world:_refreshLooseRootChunk(root)
    end
    world._worldPendingRehomes[root] = destinationRegion
    world._worldActorDemandRegions[destinationRegion] = true
    world:_recordWorldRootPosition(root, destinationRegion.path, position)
    world:_rememberWorldRootPosition(root, position)
    world:_sleepWorldRoot(root)
    world._worldCacheBytesDirty = true
    if destinationRegion.state == "Unloaded" then
        world:_queueRegion(destinationRegion)
    end
    return touchedRegions, looseTouched
end

---@param world             Global.WorldGameMap.WorldGameMap
---@param root              Engine.Actor
---@param sourceRegion      Source.SceneComponents.WorldRegionData | nil
---@param destinationRegion Source.SceneComponents.WorldRegionData | nil
---@param layerName         string
---@param position          sf.Vector2i
---@param touchedRegions    table<Source.SceneComponents.WorldRegionData, boolean> | nil
---@return table<Source.SceneComponents.WorldRegionData, boolean> | nil, boolean
local function transferRoot(world, root, sourceRegion, destinationRegion, layerName, position, touchedRegions)
    local looseTouched = false
    if sourceRegion ~= nil then
        local sourcePayload = assert(sourceRegion.payload)
        world:_removeWorldRoot(sourcePayload.actors[layerName] or {}, root)
        world:_removeRegionRootMetadata(sourcePayload, root)
        touchedRegions = touchedRegions or {}
        touchedRegions[sourceRegion] = true
    else
        world:_removeWorldRoot(world._worldLooseRoots, root)
        world:_removeLooseRootChunk(root)
        looseTouched = true
    end
    if destinationRegion ~= nil then
        world:_attachRegionRoot(destinationRegion, root, layerName, world._worldActorDefinitionRegions[root])
        touchedRegions = touchedRegions or {}
        touchedRegions[destinationRegion] = true
        if destinationRegion.state == "Active" and world._worldRootStates[root] == "Active" then
            assert(destinationRegion.payload).activeRoots[root] = true
        elseif destinationRegion.state ~= "Active" then
            world:_sleepWorldRoot(root)
        end
    else
        world:_appendWorldActorOnce(world._worldLooseRoots, root)
        world._worldActorRegions[root] = nil
        world:_refreshLooseRootChunk(root)
        looseTouched = true
    end
    world:_recordWorldRootPosition(root, destinationRegion ~= nil and destinationRegion.path or "", position)
    world:_rememberWorldRootPosition(root, position)
    world._worldPendingRehomes[root] = nil
    world._worldCacheBytesDirty = true
    return touchedRegions, looseTouched
end

---@param world          Global.WorldGameMap.WorldGameMap
---@param touchedRegions table<Source.SceneComponents.WorldRegionData, boolean> | nil
---@param looseTouched   boolean
---@return table<Source.SceneComponents.WorldRegionData, boolean> | nil, boolean
local function advancePendingRehomes(world, touchedRegions, looseTouched)
    if not bool(world._worldPendingRehomes) then
        return touchedRegions, looseTouched
    end
    local pendingRoots = {}
    for root in pairs(world._worldPendingRehomes) do
        pendingRoots[#pendingRoots + 1] = root
    end
    for _, root in ipairs(pendingRoots) do
        local requestedRegion = world._worldPendingRehomes[root]
        if requestedRegion ~= nil and root:isDestroyed() then
            world._worldPendingRehomes[root] = nil
        elseif requestedRegion ~= nil then
            local position = root:getMapPosition()
            local destinationRegion = world:_findRegionAt(position)
            if destinationRegion == nil or world:_isWorldCellReady(destinationRegion, position) then
                local sourceRegion = world._worldActorRegions[root]
                local layerName = world._worldActorLayers[root]
                local changedLoose
                touchedRegions, changedLoose = transferRoot(
                    world, root, sourceRegion, destinationRegion, layerName, position, touchedRegions
                )
                looseTouched = looseTouched or changedLoose
            elseif destinationRegion ~= requestedRegion then
                local changedLoose
                touchedRegions, changedLoose = queuePendingRehome(
                    world, root, destinationRegion, nil, position, touchedRegions
                )
                looseTouched = looseTouched or changedLoose
            end
        end
    end
    return touchedRegions, looseTouched
end

---@param world          Global.WorldGameMap.WorldGameMap
---@param root           Engine.Actor
---@param position       sf.Vector2i
---@param touchedRegions table<Source.SceneComponents.WorldRegionData, boolean> | nil
---@param looseTouched   boolean
---@return table<Source.SceneComponents.WorldRegionData, boolean> | nil, boolean
local function rehomeChangedRoot(world, root, position, touchedRegions, looseTouched)
    if root:isDestroyed() then
        return touchedRegions, looseTouched
    end
    local sourceRegion = world._worldActorRegions[root]
    local destinationRegion = world:_findRegionAt(position)
    if sourceRegion ~= nil and destinationRegion ~= nil and destinationRegion.path == sourceRegion.path then
        if world:_isWorldCellReady(sourceRegion, position) then
            if world:_refreshRegionRootChunk(assert(sourceRegion.payload), root) then
                touchedRegions = touchedRegions or {}
                touchedRegions[sourceRegion] = true
            end
            world:_recordWorldRootPosition(root, sourceRegion.path, position)
            world:_rememberWorldRootPosition(root, position)
        else
            local changedLoose
            touchedRegions, changedLoose = queuePendingRehome(
                world, root, sourceRegion, sourceRegion, position, touchedRegions
            )
            looseTouched = looseTouched or changedLoose
        end
    elseif destinationRegion == nil then
        if sourceRegion == nil then
            if world:_refreshLooseRootChunk(root) then
                looseTouched = true
            end
            world:_recordWorldRootPosition(root, "", position)
            world:_rememberWorldRootPosition(root, position)
        else
            local layerName = world._worldActorLayers[root]
            local changedLoose
            touchedRegions, changedLoose = transferRoot(
                world, root, sourceRegion, nil, layerName, position, touchedRegions
            )
            looseTouched = looseTouched or changedLoose
        end
    else
        if not world:_isWorldCellReady(destinationRegion, position) then
            if root:getCollisionEnabled() then
                world:ensureRegionLoadedAt(position)
            end
            if not world:_isWorldCellReady(destinationRegion, position) then
                local changedLoose
                touchedRegions, changedLoose = queuePendingRehome(
                    world, root, destinationRegion, sourceRegion, position, touchedRegions
                )
                return touchedRegions, looseTouched or changedLoose
            end
        end
        local layerName = world._worldActorLayers[root]
        local changedLoose
        touchedRegions, changedLoose = transferRoot(
            world, root, sourceRegion, destinationRegion, layerName, position, touchedRegions
        )
        looseTouched = looseTouched or changedLoose
    end
    return touchedRegions, looseTouched
end

function WorldGameMapActorStreaming:_rehomeRegionActors()
    local touchedRegions
    local looseTouched = false
    touchedRegions, looseTouched = advancePendingRehomes(self, touchedRegions, looseTouched)

    ---@type Engine.Actor[] | nil
    local changedRoots
    ---@type table<Engine.Actor, sf.Vector2i> | nil
    local changedPositions
    for _, sourceRegion in ipairs(self._worldRegions) do
        if sourceRegion.payload ~= nil and sourceRegion.state == "Active" then
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
            self:_removeWorldRoot(self._worldLooseRoots, root)
            self:_removeLooseRootChunk(root)
            self:_unindexWorldActorTree(root)
            self._worldCacheBytesDirty = true
            looseTouched = true
        end
    end
    if changedRoots ~= nil then
        for _, root in ipairs(changedRoots) do
            local position = assert(changedPositions)[root]
            ---@cast position - nil
            touchedRegions, looseTouched = rehomeChangedRoot(self, root, position, touchedRegions, looseTouched)
        end
    end
    if touchedRegions ~= nil then
        for _, region in ipairs(self._worldRegions) do
            if touchedRegions[region] and region.payload ~= nil and region.state == "Active" then
                self:_syncRegionActorActivation(region)
            end
        end
    end
    if looseTouched then
        self:_syncLooseRootActivation()
    end
end

function WorldGameMapActorStreaming:_pruneDestroyedRegionActors()
    if not self._worldDestroyedRootsDirty then
        return
    end
    self._worldDestroyedRootsDirty = false
    local cacheChanged = false
    for _, region in ipairs(self._worldRegions) do
        if region.payload ~= nil then
            for layerName, roots in pairs(region.payload.actors) do
                local kept = {}
                for _, root in ipairs(roots) do
                    if root:isDestroyed() then
                        cacheChanged = true
                        self:_removeRegionRootMetadata(region.payload, root)
                        self:_unindexWorldActorTree(root)
                    else
                        kept[#kept + 1] = root
                    end
                end
                region.payload.actors[layerName] = kept
            end
        end
    end
    if cacheChanged then
        self._worldCacheBytesDirty = true
    end
end

return class(WorldGameMapActorStreaming)
