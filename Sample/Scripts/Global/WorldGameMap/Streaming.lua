local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local FileBatch = require("Global.Utils.FileBatch")
local WorldGeometry = require("Global.WorldGeometry")
local RegionOrdering = require("Global.WorldGameMap.RegionOrdering")

assert(GlobalCore ~= nil)

local STREAM_BATCH_SIZE = 4
local STREAM_PUBLISH_BUDGET_SECONDS = 0.00025

---@class (partial) Global.WorldGameMap.WorldGameMap
local WorldGameMapStreaming = {}

function WorldGameMapStreaming:_syncStreamingCamera()
    if self._camera == nil then
        self._worldStreamingCameraPosition = nil
        return
    end
    self._camera:syncFollowTarget()
    local position = self._camera:getViewPosition()
    self._worldStreamingCameraPosition = position ~= nil and copy(position) or nil
end

---@return Global.WorldGeometry.CellRect
function WorldGameMapStreaming:_getVisibleCellRect()
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
    local cellX = math.floor(minimumX / Engine.CellSize)
    local cellY = math.floor(minimumY / Engine.CellSize)
    return {
        x = cellX,
        y = cellY,
        width = math.max(1, math.ceil(maximumX / Engine.CellSize) - cellX),
        height = math.max(1, math.ceil(maximumY / Engine.CellSize) - cellY)
    }
end

---@return Global.WorldGeometry.CellRect
function WorldGameMapStreaming:_getGameplayCellRect()
    return self._worldActiveRect or self:_getVisibleCellRect()
end

function WorldGameMapStreaming:_refreshStreamingStates()
    if self._worldDisposed then
        return
    end
    local visible = self:_getVisibleCellRect()
    local active = {
        x = visible.x - visible.width,
        y = visible.y - visible.height,
        width = visible.width * 3,
        height = visible.height * 3
    }
    local activeRight = math.min(self._worldConfig.width, active.x + active.width)
    local activeBottom = math.min(self._worldConfig.height, active.y + active.height)
    active.x = math.max(0, active.x)
    active.y = math.max(0, active.y)
    active.width = math.max(0, activeRight - active.x)
    active.height = math.max(0, activeBottom - active.y)
    self._worldActiveRect = active
    local prepared = {
        x = visible.x - visible.width * 2,
        y = visible.y - visible.height * 2,
        width = visible.width * 5,
        height = visible.height * 5
    }
    local viewport = assert(self._camera):getViewport()
    ---@cast viewport sf.FloatRect
    local centerX = (viewport.position.x + viewport.size.x / 2) / Engine.CellSize
    local centerY = (viewport.position.y + viewport.size.y / 2) / Engine.CellSize
    local moveX = self._worldPreviousCameraCenterX ~= nil and centerX - self._worldPreviousCameraCenterX or 0
    local moveY = self._worldPreviousCameraCenterY ~= nil and centerY - self._worldPreviousCameraCenterY or 0
    self._worldPreviousCameraCenterX = centerX
    self._worldPreviousCameraCenterY = centerY
    if moveX > 0 then
        prepared.width = prepared.width + visible.width
    elseif moveX < 0 then
        prepared.x = prepared.x - visible.width
        prepared.width = prepared.width + visible.width
    end
    if moveY > 0 then
        prepared.height = prepared.height + visible.height
    elseif moveY < 0 then
        prepared.y = prepared.y - visible.height
        prepared.height = prepared.height + visible.height
    end
    local preparedRight = math.min(self._worldConfig.width, prepared.x + prepared.width)
    local preparedBottom = math.min(self._worldConfig.height, prepared.y + prepared.height)
    local preparedX = math.max(0, prepared.x)
    local preparedY = math.max(0, prepared.y)
    local preparedWidth = math.max(0, preparedRight - preparedX)
    local preparedHeight = math.max(0, preparedBottom - preparedY)
    ---@cast preparedX integer
    ---@cast preparedY integer
    ---@cast preparedWidth integer
    ---@cast preparedHeight integer
    prepared.x = preparedX
    prepared.y = preparedY
    prepared.width = preparedWidth
    prepared.height = preparedHeight
    self._worldPreparedRect = prepared
    local preparedRect = sf.IntRect.new(prepared.x, prepared.y, prepared.width, prepared.height)
    ---@cast preparedRect sf.IntRect
    self:setSparseWorldPreparedRect(preparedRect)
    self._worldDemandGeneration = self._worldDemandGeneration + 1
    self:_refreshActorRegionDemands()
    for _, region in ipairs(self._worldRegions) do
        local demand = nil
        if WorldGeometry.RectIntersects(region, active) then
            demand = "Active"
        elseif WorldGeometry.RectIntersects(region, prepared) then
            demand = "Prepared"
        end
        if demand ~= "Prepared" then
            region.preparedEvicted = nil
        end
        region.demand = demand
        region.demandGeneration = self._worldDemandGeneration
    end
    self:_dropStaleStreamQueue()
    self:_dropStalePublishQueue()
    self:_cancelExpiredStreamingBatch()
    for _, region in ipairs(self._worldRegions) do
        if region.demand == "Active" then
            if region.payload ~= nil then
                self:_activateRegion(region)
            else
                self:_queueRegion(region)
            end
        elseif region.demand == "Prepared" then
            if region.payload ~= nil then
                self:_deactivateRegion(region, "Prepared")
            elseif not region.preparedEvicted then
                self:_queueRegion(region)
            end
        elseif region.payload ~= nil then
            self:_deactivateRegion(region, "Dormant")
        elseif self._worldActorDemandRegions[region] and region.state ~= "Reading" then
            region.state = "Unloaded"
            self:_queueRegion(region)
        elseif region.state ~= "Reading" then
            region.state = "Unloaded"
        end
    end
    self:_sortStreamQueue()
    self:_sortPublishQueue()
end

---@param region Source.SceneComponents.WorldRegionData
---@return boolean
function WorldGameMapStreaming:_isRegionDemanded(region)
    return region.demandGeneration == self._worldDemandGeneration and region.demand ~= nil
        or self._worldActorDemandRegions[region] == true
end

function WorldGameMapStreaming:_dropStaleStreamQueue()
    local queue = {}
    for _, region in ipairs(self._worldStreamQueue) do
        if self:_isRegionDemanded(region) and region.payload == nil then
            queue[#queue + 1] = region
        else
            self._worldStreamQueued[region] = nil
            if region.state ~= "Reading" then
                region.state = "Unloaded"
            end
        end
    end
    self._worldStreamQueue = queue
end

function WorldGameMapStreaming:_dropStalePublishQueue()
    local regions = self._worldPublishQueue
    self._worldPublishQueue = {}
    self._worldPublishQueued = {}
    for _, region in ipairs(regions) do
        if region.publishState ~= nil and (self:_isRegionDemanded(region) or region.publishState.forceActivate) then
            self._worldPublishQueued[region] = true
            self._worldPublishQueue[#self._worldPublishQueue + 1] = region
        else
            self:_cancelRegionPublish(region)
        end
    end
end

function WorldGameMapStreaming:_sortStreamQueue()
    RegionOrdering.SortByDemand(self, self._worldStreamQueue)
end

function WorldGameMapStreaming:_sortPublishQueue()
    RegionOrdering.SortByDemand(self, self._worldPublishQueue)
end

---@return boolean
function WorldGameMapStreaming:_streamBatchHasDemand()
    for _, region in pairs(self._worldStreamJobRegions) do
        if region.payload == nil and self:_isRegionDemanded(region) then
            return true
        end
    end
    for _, region in ipairs(self._worldStreamBatchRegions) do
        if region.payload == nil and region.publishState ~= nil
            and region.publishState.phase == "convert" and self:_isRegionDemanded(region) then
            return true
        end
    end
    return false
end

---@param requeue boolean
function WorldGameMapStreaming:_finishStreamingBatch(requeue)
    local regions = self._worldStreamBatchRegions
    self._worldStreamJob = nil
    self._worldStreamJobRegions = {}
    self._worldStreamBatchRegions = {}
    for _, region in ipairs(regions) do
        if region.payload == nil and region.publishState == nil and region.state == "Reading" then
            region.state = "Unloaded"
        end
        if requeue and region.payload == nil and region.publishState == nil and self:_isRegionDemanded(region) then
            self:_queueRegion(region)
        end
    end
    if requeue then
        self:_sortStreamQueue()
    end
end

function WorldGameMapStreaming:_cancelExpiredStreamingBatch()
    if self._worldStreamJob == nil or self:_streamBatchHasDemand() then
        return
    end
    asyncio.cancel_file_batch(self._worldStreamJob)
    self:_finishStreamingBatch(true)
end

---@param region Source.SceneComponents.WorldRegionData
function WorldGameMapStreaming:_queueRegion(region)
    if not self:_isRegionDemanded(region) or region.payload ~= nil
        or region.state ~= "Unloaded" or region.publishState ~= nil
        or self._worldStreamQueued[region] or region.demand == "Prepared" and region.preparedEvicted then
        return
    end
    self._worldStreamQueued[region] = true
    self._worldStreamQueue[#self._worldStreamQueue + 1] = region
end

function WorldGameMapStreaming:_startStreamingBatch()
    if self._worldStreamJob ~= nil or not bool(self._worldStreamQueue) then
        return
    end
    local specs = {}
    local jobRegions = {}
    local batchRegions = {}
    while #specs < STREAM_BATCH_SIZE and bool(self._worldStreamQueue) do
        local region = table.remove(self._worldStreamQueue, 1)
        self._worldStreamQueued[region] = nil
        if self:_isRegionDemanded(region) and region.payload == nil and region.state == "Unloaded"
            and not (region.demand == "Prepared" and region.preparedEvicted) then
            region.state = "Reading"
            local category = tostring(region.index)
            jobRegions[category] = region
            batchRegions[#batchRegions + 1] = region
            specs[#specs + 1] = {
                category = category,
                root = self._worldDataRoot,
                suffix = region.map,
                recursive = false,
                required = true,
                parseJson = true
            }
        end
    end
    if not bool(specs) then
        return
    end
    self._worldStreamJobRegions = jobRegions
    self._worldStreamBatchRegions = batchRegions
    self._worldStreamJob = asyncio.start_file_batch(specs)
end

---@param item FileBatchItem
function WorldGameMapStreaming:_consumeStreamingItem(item)
    local region = self._worldStreamJobRegions[item.category]
    local conversion = assert(item.conversion, "World region JSON conversion is unavailable")
    local contentBytes = assert(item.contentBytes, "World region JSON byte size is unavailable")
    if region == nil or item.relativePath ~= region.map then
        asyncio.clear_file_batch_json(conversion)
        error("World region file batch returned an unexpected item: " .. item.relativePath)
    end
    if region.payload == nil then
        if self:_isRegionDemanded(region) then
            self:_beginRegionConversion(
                region, conversion, contentBytes, false, self._camera ~= nil and self:_getVisibleCellRect() or nil
            )
        else
            asyncio.clear_file_batch_json(conversion)
            region.state = "Unloaded"
        end
    else
        asyncio.clear_file_batch_json(conversion)
    end
    self._worldStreamJobRegions[item.category] = nil
end

function WorldGameMapStreaming:_pumpStreaming()
    if self._worldDisposed then
        return
    end
    local deadline = perfCounter() + STREAM_PUBLISH_BUDGET_SECONDS
    self:_startStreamingBatch()
    self:_pumpRegionPublishing(deadline)
    while perfCounter() < deadline do
        self:_startStreamingBatch()
        if self._worldStreamJob == nil then
            break
        end
        local pollStarted = perfCounter()
        local snapshot = asyncio.poll_file_batch(self._worldStreamJob, 1)
        local item = snapshot.items ~= nil and snapshot.items[1] or nil
        if item ~= nil then
            self._worldPublishMilliseconds = self._worldPublishMilliseconds + (perfCounter() - pollStarted) * 1000.0
            self:_consumeStreamingItem(item)
        end
        if snapshot.state == "failed" then
            local errorData = snapshot.error
            local failedRegion = errorData ~= nil and self._worldStreamJobRegions[errorData.category] or nil
            local failedRegionDemanded = failedRegion ~= nil and self:_isRegionDemanded(failedRegion)
            if failedRegionDemanded or failedRegion == nil and self:_streamBatchHasDemand() then
                local message = FileBatch.FormatError(errorData)
                asyncio.cancel_file_batch(self._worldStreamJob)
                self:_finishStreamingBatch(false)
                error(message)
            end
            if failedRegion ~= nil and failedRegion.payload == nil and failedRegion.publishState == nil then
                failedRegion.state = "Unloaded"
            end
            asyncio.cancel_file_batch(self._worldStreamJob)
            self:_finishStreamingBatch(true)
            self:_startStreamingBatch()
        elseif snapshot.state == "cancelled" then
            for _, region in ipairs(self._worldStreamBatchRegions) do
                self:_cancelRegionPublish(region)
            end
            self:_finishStreamingBatch(true)
            self:_startStreamingBatch()
        else
            if snapshot.state == "completed" and snapshot.drained then
                for _, region in pairs(self._worldStreamJobRegions) do
                    assert(
                        region.payload ~= nil or not self:_isRegionDemanded(region),
                        "World region was not read: " .. region.path
                    )
                end
                self:_finishStreamingBatch(true)
                self:_startStreamingBatch()
            end
        end
        self:_pumpRegionPublishing(deadline)
        if item == nil and snapshot.state ~= "completed"
            and snapshot.state ~= "cancelled" and snapshot.state ~= "failed" then
            break
        end
    end
    self:_pumpRegionBackgroundBuilds(deadline)
end

return class(WorldGameMapStreaming)
