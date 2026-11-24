local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local FileBatch = require("Global.Utils.FileBatch")
local WorldPublishPhase = require("Enums.WorldPublishPhase")

local WorldRegionDemand = GlobalCore.WorldRegionDemand
local WorldRegionState = GlobalCore.WorldRegionState

---@return number, number, number, number
---@param self WorldGameMapImplState
local function getVisibleCellBounds(self)
    local camera = self._camera
    ---@cast camera GlobalCore.Camera
    local viewport = camera:getViewport()
    ---@cast viewport sf.FloatRect
    local pixelWidth = math.max(1, math.ceil(viewport.size.x))
    local pixelHeight = math.max(1, math.ceil(viewport.size.y))
    ---@cast pixelWidth integer
    ---@cast pixelHeight integer
    local topLeftPixel = sf.Vector2i.new(0, 0)
    local topRightPixel = sf.Vector2i.new(pixelWidth, 0)
    local bottomLeftPixel = sf.Vector2i.new(0, pixelHeight)
    local bottomRightPixel = sf.Vector2i.new(pixelWidth, pixelHeight)
    ---@cast topLeftPixel sf.Vector2i
    ---@cast topRightPixel sf.Vector2i
    ---@cast bottomLeftPixel sf.Vector2i
    ---@cast bottomRightPixel sf.Vector2i
    local topLeft = camera:mapPixelToCoords(topLeftPixel)
    local topRight = camera:mapPixelToCoords(topRightPixel)
    local bottomLeft = camera:mapPixelToCoords(bottomLeftPixel)
    local bottomRight = camera:mapPixelToCoords(bottomRightPixel)
    local minimumX = math.min(topLeft.x, topRight.x, bottomLeft.x, bottomRight.x)
    local minimumY = math.min(topLeft.y, topRight.y, bottomLeft.y, bottomRight.y)
    local maximumX = math.max(topLeft.x, topRight.x, bottomLeft.x, bottomRight.x)
    local maximumY = math.max(topLeft.y, topRight.y, bottomLeft.y, bottomRight.y)
    local cellSize = Engine.GetCellSize()
    return minimumX / cellSize, minimumY / cellSize, maximumX / cellSize, maximumY / cellSize
end

---@param self     WorldGameMapImplState
---@param minimumX number
---@param minimumY number
---@param maximumX number
---@param maximumY number
---@return Global.WorldGeometry.CellRect
local function getClippedCellRect(self, minimumX, minimumY, maximumX, maximumY)
    local cellX = math.max(0, math.min(self._worldConfig.width, math.floor(minimumX)))
    local cellY = math.max(0, math.min(self._worldConfig.height, math.floor(minimumY)))
    local cellRight = math.min(self._worldConfig.width, math.ceil(maximumX))
    local cellBottom = math.min(self._worldConfig.height, math.ceil(maximumY))
    return { x = cellX, y = cellY, width = math.max(0, cellRight - cellX), height = math.max(0, cellBottom - cellY) }
end

local WorldGameMapStreaming = {}

---@param self WorldGameMapImplState
function WorldGameMapStreaming.SyncStreamingCamera(self)
    if self._camera == nil then
        self._worldStreamingCameraPosition = nil
        return
    end
    self._camera:syncFollowTarget()
    local position = self._camera:getViewPosition()
    self._worldStreamingCameraPosition = position ~= nil and position:copy() or nil
end

---@return Global.WorldGeometry.CellRect
---@param self WorldGameMapImplState
function WorldGameMapStreaming.GetVisibleCellRect(self)
    local minimumX, minimumY, maximumX, maximumY = getVisibleCellBounds(self)
    local cellX = math.floor(minimumX)
    local cellY = math.floor(minimumY)
    return {
        x = cellX,
        y = cellY,
        width = math.max(1, math.ceil(maximumX) - cellX),
        height = math.max(1, math.ceil(maximumY) - cellY)
    }
end

---@return Global.WorldGeometry.CellRect
---@param self WorldGameMapImplState
function WorldGameMapStreaming.GetGameplayCellRect(self)
    if self._worldActiveRect ~= nil then
        return {
            x = self._worldActiveRect.x,
            y = self._worldActiveRect.y,
            width = self._worldActiveRect.width,
            height = self._worldActiveRect.height
        }
    end
    return self:_getVisibleCellRect()
end

---@param self WorldGameMapImplState
function WorldGameMapStreaming.RefreshStreamingStates(self)
    if self._worldDisposed then
        return
    end
    local minimumX, minimumY, maximumX, maximumY = getVisibleCellBounds(self)
    local width = maximumX - minimumX
    local height = maximumY - minimumY
    local active = getClippedCellRect(self, minimumX - width, minimumY - height, maximumX + width, maximumY + height)
    self._worldActiveRect = active
    local preparedLeft = minimumX - width * self._PREPARED_VIEWPORT_MARGIN_MULTIPLIER
    local preparedTop = minimumY - height * self._PREPARED_VIEWPORT_MARGIN_MULTIPLIER
    local preparedRight = maximumX + width * self._PREPARED_VIEWPORT_MARGIN_MULTIPLIER
    local preparedBottom = maximumY + height * self._PREPARED_VIEWPORT_MARGIN_MULTIPLIER
    local viewport = assert(self._camera):getViewport()
    ---@cast viewport sf.FloatRect
    local centerX = (viewport.position.x + viewport.size.x / 2) / Engine.GetCellSize()
    local centerY = (viewport.position.y + viewport.size.y / 2) / Engine.GetCellSize()
    local center = sf.Vector2f.new(centerX, centerY)
    ---@cast center sf.Vector2f
    local movement = self._worldStreamingState:updateCameraCenter(center)
    if movement.x > 0 then
        preparedRight = preparedRight + width
    elseif movement.x < 0 then
        preparedLeft = preparedLeft - width
    end
    if movement.y > 0 then
        preparedBottom = preparedBottom + height
    elseif movement.y < 0 then
        preparedTop = preparedTop - height
    end
    local prepared = getClippedCellRect(self, preparedLeft, preparedTop, preparedRight, preparedBottom)
    self._worldPreparedRect = prepared
    local preparedRect = sf.IntRect.new(prepared.x, prepared.y, prepared.width, prepared.height)
    ---@cast preparedRect sf.IntRect
    self:setSparseWorldPreparedRect(preparedRect)
    local activeRect = sf.IntRect.new(active.x, active.y, active.width, active.height)
    ---@cast activeRect sf.IntRect
    local actorDemandRegions = self:_refreshActorRegionDemands()
    self._worldStreamingState:updateDemand(activeRect, preparedRect, center, actorDemandRegions)
    for _, region in ipairs(self._worldRegions) do
        if region.publishState ~= nil and not self:_isRegionDemanded(region) and not region.publishState.forceActivate then
            self:_cancelRegionPublish(region)
        end
    end
    self:_cancelExpiredStreamingBatch()
    for _, region in ipairs(self._worldRegions) do
        local demand = self._worldStreamingState:getRegionDemand(region.index)
        local state = self._worldStreamingState:getRegionState(region.index)
        if region.publishState ~= nil and region.publishState.builder ~= nil then
            region.publishState.builder.setResourcePriority(
                (region.publishState.forceActivate or demand == WorldRegionDemand.Active) and 1 or 0
            )
        end
        if demand == WorldRegionDemand.Active then
            if region.payload ~= nil then
                self:_activateRegion(region)
            end
        elseif demand == WorldRegionDemand.Prepared then
            if region.payload ~= nil then
                self:_deactivateRegion(region, WorldRegionState.Prepared)
            end
        elseif region.payload ~= nil then
            self:_deactivateRegion(region, WorldRegionState.Dormant)
        elseif state ~= WorldRegionState.Reading and state ~= WorldRegionState.Unloaded then
            error("World region without payload has an invalid streaming state: " .. region.path)
        end
    end
end

---@param region Global.WorldGameMap.WorldRegionData
---@return boolean
---@param self   WorldGameMapImplState
function WorldGameMapStreaming.IsRegionDemanded(self, region)
    return self._worldStreamingState:isRegionDemanded(region.index)
end

---@return boolean
---@param self WorldGameMapImplState
function WorldGameMapStreaming.StreamBatchHasDemand(self)
    for _, region in pairs(self._worldStreamJobRegions) do
        if region.payload == nil and self:_isRegionDemanded(region) then
            return true
        end
    end
    for _, region in ipairs(self._worldStreamBatchRegions) do
        if region.payload == nil and region.publishState ~= nil
            and region.publishState.phase == WorldPublishPhase.CONVERT and self:_isRegionDemanded(region) then
            return true
        end
    end
    return false
end

---@param requeue boolean
---@param self    WorldGameMapImplState
function WorldGameMapStreaming.FinishStreamingBatch(self, requeue)
    local regions = self._worldStreamBatchRegions
    self._worldStreamJob = nil
    self._worldStreamJobRegions = {}
    self._worldStreamBatchRegions = {}
    for _, region in ipairs(regions) do
        if region.payload == nil and region.publishState == nil then
            self._worldStreamingState:cancelRead(region.index, requeue)
        end
    end
end

---@param self WorldGameMapImplState
function WorldGameMapStreaming.CancelExpiredStreamingBatch(self)
    if self._worldStreamJob == nil or self:_streamBatchHasDemand() then
        return
    end
    asyncio.cancel_file_batch(self._worldStreamJob)
    self:_finishStreamingBatch(true)
end

---@param self   WorldGameMapImplState
---@param region Global.WorldGameMap.WorldRegionData
---@return FileBatchSpec
local function regionSpec(self, region)
    return { category = tostring(region.index), root = self._worldDataRoot, file = region.map, parseData = true }
end

---@param self WorldGameMapImplState
function WorldGameMapStreaming.StartStreamingBatch(self)
    if self._worldStreamJob ~= nil then
        return
    end
    local specs = {}
    local jobRegions = {}
    local batchRegions = {}
    for _, regionIndex in ipairs(self._worldStreamingState:takeReadBatch(asyncio.file_worker_count())) do
        local region = assert(self._worldRegions[regionIndex], "Native world read queue returned an invalid region")
        if region.viewportRead == nil then
            local spec = regionSpec(self, region)
            jobRegions[spec.category] = region
            batchRegions[#batchRegions + 1] = region
            specs[#specs + 1] = spec
        end
    end
    if not bool(specs) then
        return
    end
    self._worldStreamJobRegions = jobRegions
    self._worldStreamBatchRegions = batchRegions
    self._worldStreamJob = asyncio.start_file_batch(specs)
end

---@param self          WorldGameMapImplState
---@param region        Global.WorldGameMap.WorldRegionData | nil
---@param item          FileBatchItem
---@param forceActivate boolean
---@param priorityRect  Global.WorldGeometry.CellRect | nil
local function consumeRegionItem(self, region, item, forceActivate, priorityRect)
    local conversion = assert(item.conversion, "World region JSON conversion is unavailable")
    local contentBytes = assert(item.contentBytes, "World region JSON byte size is unavailable")
    if region == nil or item.category ~= tostring(region.index) or item.relativePath ~= region.map then
        asyncio.clear_file_batch_data(conversion)
        error("World region file batch returned an unexpected item: " .. item.relativePath)
    end
    if region.payload == nil then
        if forceActivate or self:_isRegionDemanded(region) then
            self:_beginRegionConversion(region, conversion, contentBytes, forceActivate, priorityRect)
        else
            asyncio.clear_file_batch_data(conversion)
            self._worldStreamingState:cancelRead(region.index, false)
        end
    else
        asyncio.clear_file_batch_data(conversion)
    end
end

---@param self          WorldGameMapImplState
---@param forcedRegion? Global.WorldGameMap.WorldRegionData
---@param priorityRect? Global.WorldGeometry.CellRect
---@return boolean
local function pollStreamingBatch(self, forcedRegion, priorityRect)
    local job = assert(self._worldStreamJob)
    local pollStarted = perfCounter()
    local snapshot = asyncio.poll_file_batch(job, 1, forcedRegion ~= nil)
    local item = snapshot.items[1]
    if item ~= nil then
        self._worldPublishMilliseconds = self._worldPublishMilliseconds + (perfCounter() - pollStarted) * 1000.0
        local region = self._worldStreamJobRegions[item.category]
        consumeRegionItem(
            self, region, item, forcedRegion ~= nil and region == forcedRegion,
            priorityRect or (self._camera ~= nil and self:_getVisibleCellRect() or nil)
        )
        self._worldStreamJobRegions[item.category] = nil
    end
    if snapshot.state == asyncio.FILE_BATCH_FAILED then
        local errorData = snapshot.error
        local failedRegion = errorData ~= nil and self._worldStreamJobRegions[errorData.category] or nil
        local required = failedRegion ~= nil and (failedRegion == forcedRegion or self:_isRegionDemanded(failedRegion))
            or failedRegion == nil and (forcedRegion ~= nil or self:_streamBatchHasDemand())
        self:_finishStreamingBatch(not required)
        if required then
            error(FileBatch.FormatError(errorData))
        end
        return true
    end
    if snapshot.state == asyncio.FILE_BATCH_CANCELLED then
        for _, region in ipairs(self._worldStreamBatchRegions) do
            self:_cancelRegionPublish(region)
        end
        self:_finishStreamingBatch(true)
        return true
    end
    if snapshot.state == asyncio.FILE_BATCH_COMPLETED and snapshot.drained then
        for _, region in pairs(self._worldStreamJobRegions) do
            assert(
                region.payload ~= nil or not self:_isRegionDemanded(region),
                "World region was not read: " .. region.path
            )
        end
        self:_finishStreamingBatch(true)
        return true
    end
    return item ~= nil
end

---@param self         WorldGameMapImplState
---@param region       Global.WorldGameMap.WorldRegionData
---@param priorityRect Global.WorldGeometry.CellRect
local function waitForViewportRead(self, region, priorityRect)
    while region.viewportRead ~= nil do
        local job = region.viewportRead
        local snapshot = asyncio.poll_file_batch(job, 1, true)
        if snapshot.state == asyncio.FILE_BATCH_FAILED then
            error(FileBatch.FormatError(snapshot.error))
        end
        local item = assert(snapshot.items[1], "World region was not read: " .. region.path)
        local delivered = assert(self._worldRegions[assert(tonumber(item.category))])
        assert(delivered.viewportRead == job, "Unexpected viewport file batch item: " .. item.relativePath)
        consumeRegionItem(self, delivered, item, true, priorityRect)
        delivered.viewportRead = nil
    end
end

---@param self         WorldGameMapImplState
---@param regions      Global.WorldGameMap.WorldRegionData[]
---@param priorityRect Global.WorldGeometry.CellRect
function WorldGameMapStreaming.EnsureRegionsLoaded(self, regions, priorityRect)
    local specs = {}
    local scheduled = {}
    for _, region in ipairs(regions) do
        if region.payload == nil and region.publishState == nil and region.viewportRead == nil
            and self._worldStreamJobRegions[tostring(region.index)] == nil then
            specs[#specs + 1] = regionSpec(self, region)
            scheduled[#scheduled + 1] = region
        end
    end
    local job = #specs > 0 and asyncio.start_file_batch(specs) or nil
    for _, region in ipairs(scheduled) do
        region.viewportRead = job
    end
    local publishing
    local _ <close> = setmetatable({}, {
        __close = function (_, failure)
            if failure == nil then
                return
            end
            if job ~= nil then
                asyncio.cancel_file_batch(job)
                for _, region in ipairs(scheduled) do
                    region.viewportRead = nil
                    self:_cancelRegionPublish(region)
                    if region.payload == nil then
                        self._worldStreamingState:cancelRead(region.index, true)
                    end
                end
            end
            if publishing ~= nil then
                self:_cancelRegionPublish(publishing)
            end
        end
    })
    for _, region in ipairs(regions) do
        waitForViewportRead(self, region, priorityRect)
        while self._worldStreamJobRegions[tostring(region.index)] ~= nil do
            pollStreamingBatch(self, region, priorityRect)
        end
        if region.payload ~= nil then
            self:_activateRegion(region)
        else
            publishing = region
            if region.publishState == nil then
                -- An obsolete sibling failure may have cancelled this in-flight read.
                self:_beginRegionPublish(
                    region, Engine.loadData(os.path.join(self._worldDataRoot, region.map)), true, priorityRect
                )
            else
                region.publishState.forceActivate = true
            end
            self:_drainRegionPublish(region)
            publishing = nil
        end
    end
end

---@param self                 WorldGameMapImplState
---@param publishBudgetSeconds number
function WorldGameMapStreaming.PumpStreaming(self, publishBudgetSeconds)
    if self._worldDisposed then
        return
    end
    local deadline = perfCounter() + publishBudgetSeconds
    local waitingBuilders = {}
    self:_startStreamingBatch()
    self:_pumpRegionPublishing(deadline, waitingBuilders)
    while perfCounter() < deadline do
        self:_startStreamingBatch()
        if self._worldStreamJob == nil then
            break
        end
        local progressed = pollStreamingBatch(self)
        self:_pumpRegionPublishing(deadline, waitingBuilders)
        if not progressed then
            break
        end
    end
    self:_pumpRegionBackgroundBuilds(deadline)
end

return WorldGameMapStreaming
