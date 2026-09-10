local Engine = require("Engine")
---@type Global.Utils.Path.Module
local Path = require("Global.Utils.Path")

local SceneInitAnimationCache = {}

function SceneInitAnimationCache.SourceKey(value, suffix)
    local normalized = Path.NormaliseSeparators(value)
    assert(string.endsWith(normalized, suffix), "Unexpected animation file suffix: " .. normalized)
    return normalized:sub(1, #normalized - #suffix)
end

function SceneInitAnimationCache.GetFrameAssets(payload, relativePath)
    local result = {}
    local seen = {}
    local assets = payload.assets or {}
    for _, timeLine in ipairs(payload.timeLines or {}) do
        for _, segment in ipairs(timeLine.timeSegments or {}) do
            if segment.type == "frame" then
                local assetIndex = tonumber(segment.asset)
                assert(
                    assetIndex ~= nil and assetIndex >= 0 and assetIndex % 1 == 0,
                    "Invalid frame asset index in animation: " .. relativePath
                )
                local assetName = assets[assetIndex + 1]
                assert(
                    Class.isInstance(assetName, "string") and bool(assetName),
                    string.format("Frame asset index %d is unavailable in animation: %s", assetIndex, relativePath)
                )
                if not seen[assetName] then
                    seen[assetName] = true
                    result[#result + 1] = { name = assetName, path = assetName }
                end
            end
        end
    end
    return result
end

function SceneInitAnimationCache.NeedsCompression(sourcePath, cachePath, frameAssets)
    if not os.path.isfile(cachePath) then
        return true
    end
    local cacheTime = os.path.getmtime(cachePath)
    if cacheTime < os.path.getmtime(sourcePath) then
        return true
    end
    for _, asset in ipairs(frameAssets) do
        if not Engine.assetExists(asset.path) then
            return true
        end
        if cacheTime < Engine.assetModificationTime(asset.path) then
            return true
        end
    end
    return false
end

return SceneInitAnimationCache
