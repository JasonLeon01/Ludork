local cjson = require("cjson")
local CoreSystem = require("CoreSystem")
local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local FileBatch = require("Global.Utils.FileBatch")
local Logging = require("Global.Utils.Logging")
local Data = require("Source.Data")
local SceneInitAnimationCache = require("Source.Scenes.SceneInitAnimationCache")
local SceneInitUI = require("Source.UI.Init")

local GlobalSystem = GlobalCore.System
local SceneBase = GlobalCore.SceneBase

local ANIMATION_SOURCE_SUFFIX = ".json"
local ENCRYPTED_DATA_SUFFIX = ".ldc"
local ANIMATION_CACHE_SUFFIX = ".anim.json"
local ENCRYPTED_ANIMATION_CACHE_SUFFIX = ".anim.ldc"
local ANIMATION_PROGRESS_WEIGHT = 0.5

---@class Source.Scenes.SceneInit.SceneInit
local Scene = {}

function Scene:onCreate()
    local gameSize = GlobalSystem.getGameSize()
    self._ui = SceneInitUI.new(self, gameSize)
    self._ui:mount(self:getUIManager(), gameSize)
    self._bg = self._ui:getBackground()
    self.progressValue = 0.0
    self._displayProgress = 0.0
    self.progressTotal = 0
    self.processedCount = 0
    self.progressDone = false
    self.hasSwitched = false
    self._loadCancelled = false
    self._loadStage = Data.BeginInitialLoad()
    self._activeBatch = nil
    self._animationSourceKeys = {}
    self._loadTask = asyncio.create_task(function ()
        self:prepareAssets()
        self._loadTask = nil
    end)
end

function Scene:onTick(_)
    if self.progressDone and not self.hasSwitched and self._displayProgress >= 0.999 then
        local SceneTitle = require("Source.Scenes.SceneTitle")

        self.hasSwitched = true
        GlobalSystem.setScene(SceneTitle.new())
    end
end

function Scene:onLateTick(_)
    local target = self.progressDone and 1.0 or self.progressValue
    if target ~= self._displayProgress then
        self._displayProgress = target
        SceneInitUI.Publish({
            progress = target
        })
    end
end

function Scene:onQuit()
    self._loadCancelled = true
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
    self.progressTotal = 0
    self.processedCount = 0
    self._activeBatch = asyncio.start_file_batch({
        {
            category = "animations",
            root = Engine.getAnimationCacheRoot(),
            suffix = ANIMATION_CACHE_SUFFIX,
            recursive = true,
            required = true
        },
        {
            category = "commonFunctions",
            root = os.path.join(".", "Data", "CommonFunctions"),
            suffix = ".json",
            recursive = false,
            required = true
        },
        {
            category = "tilesets",
            root = os.path.join(".", "Data", "Tilesets"),
            suffix = ".json",
            recursive = false,
            required = true
        },
        {
            category = "autoTiles",
            root = os.path.join(".", "Data", "AutoTiles"),
            suffix = ".json",
            recursive = false,
            required = false
        },
        {
            category = "general",
            root = os.path.join(".", "Data", "General"),
            suffix = ".json",
            recursive = false,
            required = true
        },
        {
            category = "curves",
            root = os.path.join(".", "Data", "Curves"),
            suffix = ".json",
            recursive = true,
            required = false
        },
        {
            category = "textConfigs",
            root = os.path.join(".", "Data", "TextConfigs"),
            suffix = ".json",
            recursive = true,
            required = true
        }
    })
    self:_pumpInitialLoad()
end

function Scene:prepareAssets()
    local startTime = perfCounter()
    Logging.info("Preparing initial assets")
    Logging.info("Preparing animation cache")
    local animationStartTime = perfCounter()
    self:compressAnimations()
    if self:_isLoadCancelled() then
        return
    end
    Logging.info("Prepared animation cache files in %.3fs", perfCounter() - animationStartTime)
    Logging.info("Loading game data")
    local loadStartTime = perfCounter()
    self:loadGameData()
    if self:_isLoadCancelled() then
        return
    end
    Logging.info("Loaded game data in %.3fs", perfCounter() - loadStartTime)
    Logging.info("Init asset preparation finished in %.3fs", perfCounter() - startTime)
end

function Scene:onDestroy()
    self._ui:dispose()
end

---@return boolean
function Scene:_isLoadCancelled()
    return self._loadCancelled
end

---@param offset number
---@param weight number
function Scene:_setPhaseProgress(offset, weight)
    local fraction = 0.0
    if self.progressTotal > 0 then
        fraction = self.processedCount / self.progressTotal
    end
    local value = offset + weight * Engine.Clamp(fraction, 0.0, 1.0)
    self.progressValue = math.max(self.progressValue, value)
end

---@param item       FileBatchItem
---@param sourceRoot string
---@param cacheRoot  string
---@param assetsRoot string
function Scene:_processAnimationSource(item, sourceRoot, cacheRoot, assetsRoot)
    local relativePath = item.relativePath
    local sourceKey = SceneInitAnimationCache.SourceKey(relativePath, ANIMATION_SOURCE_SUFFIX)
    local encryptedSource = item.encryptedData == true
    assert(bool(sourceKey), "Animation source filename must not be empty")
    assert(not self._animationSourceKeys[sourceKey], "Duplicate animation source key: " .. sourceKey)
    local content = assert(item.content, "Animation source content is missing: " .. relativePath)
    local payload = cjson.decode(content)
    ---@cast payload Engine.AnimationSourceData
    assert(payload.type == "animation", "Animation source has invalid type: " .. relativePath)
    self._animationSourceKeys[sourceKey] = true

    local sourceRelativePath = encryptedSource and sourceKey .. ENCRYPTED_DATA_SUFFIX or relativePath
    local sourcePath = os.path.join(sourceRoot, sourceRelativePath)
    local cacheRelativePath = sourceKey
        .. (encryptedSource and ENCRYPTED_ANIMATION_CACHE_SUFFIX or ANIMATION_CACHE_SUFFIX)
    local cachePath = os.path.join(cacheRoot, cacheRelativePath)
    local frameAssets = SceneInitAnimationCache.GetFrameAssets(payload, assetsRoot, relativePath)
    if SceneInitAnimationCache.NeedsCompression(sourcePath, cachePath, frameAssets) then
        for _, asset in ipairs(frameAssets) do
            assert(
                os.path.isfile(asset.path),
                string.format(
                    "Cannot compress animation %s: referenced frame asset is missing: %s", relativePath, asset.name
                )
            )
        end
        CoreSystem.createDirectories(os.path.dirname(cachePath))
        Logging.debug("Compressing animation: %s", relativePath)
        local compressed = Engine.compressAnimation(payload, assetsRoot, "png")
        Engine.writeJSON(cachePath, compressed)
    end
    local alternateCachePath = os.path.join(
        cacheRoot, sourceKey .. (encryptedSource and ANIMATION_CACHE_SUFFIX or ENCRYPTED_ANIMATION_CACHE_SUFFIX)
    )
    if os.path.isfile(alternateCachePath) then
        CoreSystem.removeFile(alternateCachePath)
        Logging.debug("Removed alternate animation cache: %s", alternateCachePath)
    end
end

function Scene:compressAnimations()
    local sourceRoot = Engine.getAnimationSourceRoot()
    local cacheRoot = Engine.getAnimationCacheRoot()
    local assetsRoot = os.path.join(".", "Assets", "Animations")
    self.progressTotal = 0
    self.processedCount = 0
    self._activeBatch = asyncio.start_file_batch({
        {
            category = "animationSources",
            root = sourceRoot,
            suffix = ANIMATION_SOURCE_SUFFIX,
            excludeSuffix = ANIMATION_CACHE_SUFFIX,
            recursive = true,
            required = true
        }
    })
    while not self._loadCancelled do
        local snapshot = asyncio.poll_file_batch(self._activeBatch, 1)
        self.progressTotal = snapshot.total
        if snapshot.state == "failed" then
            error(FileBatch.FormatError(snapshot.error))
        end
        if snapshot.state == "cancelled" then
            error("Animation source file batch was cancelled")
        end
        local item = snapshot.items ~= nil and snapshot.items[1] or nil
        if item ~= nil then
            self:_processAnimationSource(item, sourceRoot, cacheRoot, assetsRoot)
            self.processedCount = self.processedCount + 1
            self:_setPhaseProgress(0.0, ANIMATION_PROGRESS_WEIGHT)
        end
        if snapshot.state == "completed" and snapshot.drained and self.processedCount == snapshot.total then
            self._activeBatch = nil
            self.progressValue = math.max(self.progressValue, ANIMATION_PROGRESS_WEIGHT)
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
    local sourceKey = SceneInitAnimationCache.SourceKey(relativePath, ANIMATION_CACHE_SUFFIX)
    if self._animationSourceKeys[sourceKey] then
        return false
    end
    local cacheRelativePath = item.encryptedData == true and sourceKey .. ENCRYPTED_ANIMATION_CACHE_SUFFIX
        or relativePath
    local cachePath = os.path.join(Engine.getAnimationCacheRoot(), cacheRelativePath)
    CoreSystem.removeFile(cachePath)
    Logging.debug("Removed orphaned animation cache: %s", cachePath)
    return true
end

function Scene:_pumpInitialLoad()
    ---@cast self._activeBatch FileBatchJob
    ---@cast self._loadStage Source.Data.InitialLoadStage
    while not self._loadCancelled do
        local snapshot = asyncio.poll_file_batch(self._activeBatch, 1)
        self.progressTotal = snapshot.total
        if snapshot.state == "failed" then
            error(FileBatch.FormatError(snapshot.error))
        end
        if snapshot.state == "cancelled" then
            error("Initial data file batch was cancelled")
        end
        local item = snapshot.items ~= nil and snapshot.items[1] or nil
        if item ~= nil then
            if not self:_removeOrphanedAnimation(item) then
                Data.ApplyInitialLoadItem(self._loadStage, item)
            end
            self.processedCount = self.processedCount + 1
            self:_setPhaseProgress(ANIMATION_PROGRESS_WEIGHT, 1.0 - ANIMATION_PROGRESS_WEIGHT)
        end
        if snapshot.state == "completed" and snapshot.drained and self.processedCount == snapshot.total then
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
