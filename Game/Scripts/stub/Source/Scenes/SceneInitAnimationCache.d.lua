---@meta Source.Scenes.SceneInitAnimationCache

local SceneInitAnimationCache = {}

---@param value  string
---@param suffix string
---@return string
function SceneInitAnimationCache.SourceKey(value, suffix) end

---@param payload      Engine.AnimationSourceData
---@param relativePath string
---@return Source.Scenes.SceneInit.FrameAsset[]
function SceneInitAnimationCache.GetFrameAssets(payload, relativePath) end

---@param sourcePath  string
---@param cachePath   string
---@param frameAssets Source.Scenes.SceneInit.FrameAsset[]
---@return boolean
function SceneInitAnimationCache.NeedsCompression(sourcePath, cachePath, frameAssets) end

return SceneInitAnimationCache
