---@meta Source.Utils.AnimationCache

---@class Source.Utils.AnimationCache.FrameAsset
---@field name string
---@field path string

local AnimationCache = {}

---@param value  string
---@param suffix string
---@return string
function AnimationCache.SourceKey(value, suffix) end

---@param payload      Engine.AnimationSourceData
---@param relativePath string
---@return Source.Utils.AnimationCache.FrameAsset[]
function AnimationCache.GetFrameAssets(payload, relativePath) end

---@param sourcePath  string
---@param cachePath   string
---@param frameAssets Source.Utils.AnimationCache.FrameAsset[]
---@return boolean
function AnimationCache.NeedsCompression(sourcePath, cachePath, frameAssets) end

return AnimationCache
