local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local FileBatch = require("Global.Utils.FileBatch")
local Logging = require("Global.Utils.Logging")
local Data = require("Global.Data")
local AnimationCache = require("Source.Utils.AnimationCache")
local SceneInitController = require("Source.Scenes.SceneInit.Controller")

local Display = GlobalCore.Display
local SceneManager = GlobalCore.SceneManager
local ResourceFileConstants = Engine.ResourceFileConstants
local SceneBase = GlobalCore.SceneBase

local SPLASH_HOLD_SECONDS = 0.20

---@class Source.Scenes.SceneInit.SceneInit
local Scene = {
    _DISPLAY_PROGRESS_COMPLETE_THRESHOLD = 0.999
}

---@diagnostic disable-next-line: unused
function Scene:onEnter() end

function Scene:onCreate()
    self._preflightStart = perfCounter()
    self.phaseKey = ""
    self._displayPhaseKey = self.phaseKey
    local gameSize = Display.getGameSize()
    self._ui = SceneInitController.new(self, gameSize)
    self._ui:mount(self:getUIManager(), gameSize)
    self._bg = self._ui:getBackground()
    self.progressValue = 0.0
    self._displayProgress = 0.0
    self.progressTotal = 0
    self.processedCount = 0
    self.progressDone = false
    self._loadCancelled = false
    self._loadStage = Data.BeginInitialLoad()
    self._activeBatch = nil
    self._cacheRequest = nil
    self._animationSourceKeys = {}
    self._pendingAnimations = {}
    self._alternateAnimationCaches = {}
    self._phases = {}
    self._phaseIndex = 0
    self._splashFadeOutAt = nil
    self._ui:playAnimation("FadeIn", "Splash", function ()
        if not self._loadCancelled then
            self._splashFadeOutAt = perfCounter() + SPLASH_HOLD_SECONDS
        end
    end)
    self._loadTask = asyncio.create_required_task(function ()
        self:prepareAssets()
        while not self._loadCancelled do
            if self.progressDone and self._displayProgress >= Scene._DISPLAY_PROGRESS_COMPLETE_THRESHOLD then
                local SceneTitle = require("Source.Scenes.SceneTitle")

                SceneManager.setScene(SceneTitle.new())
                break
            end
            asyncio.sleep(0.0)
        end
        self._loadTask = nil
    end)
end

function Scene:onLateTick(_)
    if self._loadCancelled then
        return
    end
    if self._splashFadeOutAt ~= nil and #self._phases > 0 and perfCounter() >= self._splashFadeOutAt then
        self._splashFadeOutAt = nil
        self._ui:playAnimation("FadeOut", "Splash", function ()
            self.phaseKey = self._phases[1]
        end)
    end
    local target = self.progressDone and 1.0 or self.progressValue
    if target ~= self._displayProgress or self.phaseKey ~= self._displayPhaseKey then
        self._displayProgress = target
        self._displayPhaseKey = self.phaseKey
        SceneInitController.Publish({
            progress = target,
            phaseKey = self.phaseKey
        })
    end
end

function Scene:onQuit()
    self._loadCancelled = true
    self._ui:stopAnimation("FadeIn", "Splash")
    self._ui:stopAnimation("FadeOut", "Splash")
    if self._cacheRequest ~= nil then
        self._cacheRequest:cancel()
        self._cacheRequest = nil
    end
    if self._activeBatch ~= nil then
        asyncio.cancel_file_batch(self._activeBatch)
        self._activeBatch = nil
    end
    if self._loadTask ~= nil then
        asyncio.cancel_task(self._loadTask)
        self._loadTask = nil
    end
    if self._loadStage ~= nil and not self.progressDone then
        Data.AbortInitialLoad(self._loadStage)
        self._loadStage = nil
    end
end

function Scene:loadGameData()
    self:_beginPhase()
    for _, path in ipairs(self._alternateAnimationCaches) do
        if os.path.isfile(path) then
            os.removeFile(path)
            Logging.debug("Removed alternate animation cache: %s", path)
        end
    end
    self.progressTotal = 0
    self.processedCount = 0
    self._activeBatch = asyncio.start_file_batch({
        {
            category = "animations",
            root = Engine.getAnimationCacheRoot(),
            suffix = ResourceFileConstants.ANIMATION_CACHE_SUFFIX,
            recursive = true,
            required = true
        },
        {
            category = "commonFunctions",
            root = os.path.join(".", "Data", "CommonFunctions"),
            suffix = ResourceFileConstants.DATA_EXTENSION,
            recursive = false,
            required = true
        },
        {
            category = "tilesets",
            root = os.path.join(".", "Data", "Tilesets"),
            suffix = ResourceFileConstants.DATA_EXTENSION,
            recursive = false,
            required = true
        },
        {
            category = "autoTiles",
            root = os.path.join(".", "Data", "AutoTiles"),
            suffix = ResourceFileConstants.DATA_EXTENSION,
            recursive = false,
            required = false
        },
        {
            category = "general",
            root = os.path.join(".", "Data", "General"),
            suffix = ResourceFileConstants.DATA_EXTENSION,
            recursive = false,
            required = true
        },
        {
            category = "curves",
            root = os.path.join(".", "Data", "Curves"),
            suffix = ResourceFileConstants.DATA_EXTENSION,
            recursive = true,
            required = false
        },
        {
            category = "textConfigs",
            root = os.path.join(".", "Data", "TextConfigs"),
            suffix = ResourceFileConstants.DATA_EXTENSION,
            recursive = true,
            required = true
        }
    })
    self:_pumpInitialLoad()
end

function Scene:prepareAssets()
    local request = Engine.planGraphicsCaches()
    self._cacheRequest = request
    self:_planAnimations()
    if not self:_awaitCache(request, "planned", false) then
        return
    end
    local materials = request:needsMaterialExport()
    local shaders = request:needsShaderCompilation()
    local animations = #self._pendingAnimations > 0
    local phases = {}
    if materials then
        phases[#phases + 1] = "INIT_MATERIALS"
    end
    if shaders then
        phases[#phases + 1] = "INIT_SHADERS"
    end
    if animations then
        phases[#phases + 1] = "INIT_ANIMATIONS"
    end
    phases[#phases + 1] = "INIT_DATA"
    self._phases = phases
    Logging.info(
        "Init preflight finished in %.3fs (materials=%s, shaders=%s, animations=%d)",
        perfCounter() - self._preflightStart, tostring(materials), tostring(shaders), #self._pendingAnimations
    )
    if not self:_awaitSplash() then
        return
    end

    if materials then
        self:_beginPhase()
    end
    request:startMaterials()
    if not self:_awaitCache(request, "materialsReady", materials) then
        return
    end
    if shaders then
        self:_beginPhase()
    end
    request:startShaders()
    if not self:_awaitCache(request, "ready", shaders) then
        return
    end
    request:getResult()
    self._cacheRequest = nil
    if animations then
        self:compressAnimations()
    end
    if not self._loadCancelled then
        self:loadGameData()
    end
end

function Scene:onDestroy()
    self._ui:dispose()
end

---@return boolean
function Scene:_awaitSplash()
    while not self._loadCancelled and self._displayPhaseKey == "" do
        asyncio.sleep(0.0)
    end
    return not self._loadCancelled
end

function Scene:_beginPhase()
    self._phaseIndex = self._phaseIndex + 1
    self.phaseKey = self._phases[self._phaseIndex]
    self:_setPhaseProgress(0)
    Logging.info("Init stage %d/%d: %s", self._phaseIndex, #self._phases, self.phaseKey)
end

---@param fraction number
function Scene:_setPhaseProgress(fraction)
    self.progressValue = math.max(
        self.progressValue, (self._phaseIndex - 1 + math.clamp(fraction, 0.0, 1.0)) / #self._phases
    )
end

---@param item       FileBatchItem
---@param payload    Engine.AnimationSourceData
---@param sourceRoot string
---@param cacheRoot  string
function Scene:_planAnimationSource(item, payload, sourceRoot, cacheRoot)
    local relativePath = item.relativePath
    local sourceKey = AnimationCache.SourceKey(relativePath, ResourceFileConstants.DATA_EXTENSION)
    local encryptedSource = item.encryptedData == true
    assert(bool(sourceKey), "Animation source filename must not be empty")
    assert(not self._animationSourceKeys[sourceKey], "Duplicate animation source key: " .. sourceKey)
    assert(payload.type == "animation", "Animation source has invalid type: " .. relativePath)
    self._animationSourceKeys[sourceKey] = true

    local sourceRelativePath = encryptedSource and sourceKey .. ResourceFileConstants.ENCRYPTED_DATA_EXTENSION
        or relativePath
    local sourcePath = os.path.join(sourceRoot, sourceRelativePath)
    local cacheRelativePath = sourceKey
        .. (encryptedSource and ResourceFileConstants.ENCRYPTED_ANIMATION_CACHE_SUFFIX
            or ResourceFileConstants.ANIMATION_CACHE_SUFFIX)
    local cachePath = os.path.join(cacheRoot, cacheRelativePath)
    local alternateCachePath = os.path.join(
        cacheRoot,
        sourceKey
            .. (encryptedSource and ResourceFileConstants.ANIMATION_CACHE_SUFFIX
                or ResourceFileConstants.ENCRYPTED_ANIMATION_CACHE_SUFFIX)
    )
    local frameAssets = AnimationCache.GetFrameAssets(payload, relativePath)
    if AnimationCache.NeedsCompression(sourcePath, cachePath, frameAssets) then
        for _, asset in ipairs(frameAssets) do
            assert(
                Engine.assetExists(asset.path),
                string.format(
                    "Cannot compress animation %s: referenced frame asset is missing: %s", relativePath, asset.name
                )
            )
        end
        self._pendingAnimations[#self._pendingAnimations + 1] = {
            payload = payload,
            cachePath = cachePath,
            relativePath = relativePath,
            sourcePath = sourcePath,
            sourceTime = os.path.getmtime(sourcePath)
        }
    end
    if os.path.isfile(alternateCachePath) then
        self._alternateAnimationCaches[#self._alternateAnimationCaches + 1] = alternateCachePath
    end
end

---@param request Engine.GraphicsCachePlan
---@param target  string
---@param visible boolean
---@return boolean
function Scene:_awaitCache(request, target, visible)
    while not self._loadCancelled do
        local status = request:getStatus()
        if status == "failed" or status == "cancelled" then
            request:getResult()
        end
        if status == target then
            if visible then
                self:_setPhaseProgress(1)
            end
            return true
        end
        if visible then
            self:_setPhaseProgress(request:getProgress())
        end
        asyncio.sleep(0.0)
    end
    return false
end

function Scene:compressAnimations()
    self:_beginPhase()
    for index, animation in ipairs(self._pendingAnimations) do
        os.createDirectories(os.path.dirname(animation.cachePath))
        Logging.debug("Compressing animation: %s", animation.relativePath)
        local compressed = Engine.compressAnimation(animation.payload, "png")
        assert(Class.isInstance(compressed, "table"), "Compressed animation must be a data table")
        ---@cast compressed table<string, Engine.RuntimeDataValue>
        assert(
            os.path.getmtime(animation.sourcePath) == animation.sourceTime,
            "Animation source changed during preparation: " .. animation.relativePath
        )
        Engine.saveData(animation.cachePath, compressed)
        self:_setPhaseProgress(index / #self._pendingAnimations)
        asyncio.sleep(0.0)
        if self._loadCancelled then
            return
        end
    end
    self._pendingAnimations = {}
end

function Scene:_planAnimations()
    local sourceRoot = Engine.getAnimationSourceRoot()
    local cacheRoot = Engine.getAnimationCacheRoot()
    self.progressTotal = 0
    self.processedCount = 0
    self._activeBatch = asyncio.start_file_batch({
        {
            category = "animationSources",
            root = sourceRoot,
            suffix = ResourceFileConstants.DATA_EXTENSION,
            excludeSuffix = ResourceFileConstants.ANIMATION_CACHE_SUFFIX,
            recursive = true,
            parseData = true,
            required = true
        }
    })
    while not self._loadCancelled do
        local snapshot = asyncio.poll_file_batch(self._activeBatch, 32)
        self.progressTotal = snapshot.total
        if snapshot.state == asyncio.FILE_BATCH_FAILED then
            error(FileBatch.FormatError(snapshot.error))
        end
        if snapshot.state == asyncio.FILE_BATCH_CANCELLED then
            error("Animation source file batch was cancelled")
        end
        local deadline = perfCounter() + 0.004
        for _, item in ipairs(snapshot.items) do
            local conversion = assert(item.conversion, "Animation source data is missing: " .. item.relativePath)
            while true do
                local remaining = (deadline - perfCounter()) * 1000
                if remaining <= 0 then
                    asyncio.sleep(0.0)
                    deadline = perfCounter() + 0.004
                else
                    local done, _, payload = asyncio.step_file_batch_data(conversion, 4096, remaining)
                    if done then
                        self:_planAnimationSource(item, payload, sourceRoot, cacheRoot)
                        break
                    end
                end
            end
            self.processedCount = self.processedCount + 1
        end
        if snapshot.state == asyncio.FILE_BATCH_COMPLETED and snapshot.drained and self.processedCount == snapshot.total then
            self._activeBatch = nil
            return
        end
        asyncio.sleep(0.0)
    end
end

---@param item FileBatchItem
---@return boolean
function Scene:_removeOrphanedAnimation(item)
    if item.category ~= "animations" then
        return false
    end
    local relativePath = item.relativePath
    local sourceKey = AnimationCache.SourceKey(relativePath, ResourceFileConstants.ANIMATION_CACHE_SUFFIX)
    if self._animationSourceKeys[sourceKey] then
        return false
    end
    local cacheRelativePath = item.encryptedData == true
        and sourceKey .. ResourceFileConstants.ENCRYPTED_ANIMATION_CACHE_SUFFIX
        or relativePath
    local cachePath = os.path.join(Engine.getAnimationCacheRoot(), cacheRelativePath)
    os.removeFile(cachePath)
    Logging.debug("Removed orphaned animation cache: %s", cachePath)
    return true
end

function Scene:_pumpInitialLoad()
    ---@cast self._activeBatch FileBatchJob
    ---@cast self._loadStage Global.Data.InitialLoadStage
    while not self._loadCancelled do
        local snapshot = asyncio.poll_file_batch(self._activeBatch, 1)
        self.progressTotal = snapshot.total
        if snapshot.state == asyncio.FILE_BATCH_FAILED then
            error(FileBatch.FormatError(snapshot.error))
        end
        if snapshot.state == asyncio.FILE_BATCH_CANCELLED then
            error("Initial data file batch was cancelled")
        end
        local item = snapshot.items ~= nil and snapshot.items[1] or nil
        if item ~= nil then
            if not self:_removeOrphanedAnimation(item) then
                Data.ApplyInitialLoadItem(self._loadStage, item)
            end
            self.processedCount = self.processedCount + 1
            self:_setPhaseProgress(self.processedCount / self.progressTotal)
        end
        if snapshot.state == asyncio.FILE_BATCH_COMPLETED and snapshot.drained and self.processedCount == snapshot.total then
            Data.CommitInitialLoad(self._loadStage)
            self._loadStage = nil
            self._activeBatch = nil
            self.progressValue = 1.0
            self.progressDone = true
            return
        end
        asyncio.sleep(0.0)
    end
end

return class(Scene, SceneBase)
