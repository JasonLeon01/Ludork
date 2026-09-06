local GlobalCore = require("GlobalCore")
local RegionDict = require("Source.Configs.RegionDict")
local LocaleCore = require("Source.Locale.Core")
local MapPath = require("Source.MapPath")
local SceneMapBuilder = require("Source.SceneComponents.MapBuilder")
local GameSystem = require("Source.System")
local TelepointKey = require("Source.UI.Helpers.TelepointKey")

local AudioManager = GlobalCore.AudioManager
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

---@param tag   string
---@param index integer
---@return string
local function formatTelepointName(tag, index)
    local isDefaultTag = bool(
        tag:match("^.+_default_%-?%d+_%-?%d+$") or tag:match("^.+_default_%-?%d+_%-?%d+_%d+$")
            or tag:match("^.+%.runtime_default_%d+$")
    )
    if bool(tag) and not isDefaultTag then
        return LOC(tag)
    end
    local fallback = "Point_" .. tostring(index + 1)
    local pointFormat = LOC("POINT")
    if pointFormat == "POINT" then
        return fallback
    end
    local pointNumber = tostring(index + 1)
    local formatted = string.replace(pointFormat, "{index}", pointNumber)
    formatted = string.replace(formatted, "{0}", pointNumber)
    if formatted == pointFormat then
        return fallback
    end
    return formatted
end

---@class (partial) Source.Windows.WindowFloorTeleporter.Controller
local WindowFloorTeleporterController = {}

function WindowFloorTeleporterController:init(model)
    self.model = model
end

function WindowFloorTeleporterController:open(inst)
    if inst ~= nil then
        self.model._inst = inst
    end
    if self.model._clearPreviewCacheCallback ~= nil then
        self.model._clearPreviewCacheCallback()
    end
    self.model._previewWindow:clearPreviewCache()
    self.model._telepointEntriesCache = dict()
    self.model._telepointIndexes = {}
    self.model._lastMapKey = nil
    self.model._commandWindow.index = nil
    local entries = self:getVisitedRegionEntries()
    self.model._commandWindow:refreshMaps(entries)
    self.model._commandWindow:resetSelection()
    local currentMapKey = self.model._inst._cachedMap ~= nil and MapPath.WithoutExtension(self.model._inst._cachedMap)
        or nil
    for index, entry in ipairs(entries) do
        if MapPath.WithoutExtension(entry[1]) == currentMapKey then
            self.model._commandWindow.index = index - 1
            self.model._commandWindow._oldIndex = index - 1
            break
        end
    end
    self:notifyMapIndexMaybeChanged(self.model._commandWindow.index)
    if self.model._commandWindow.index == nil then
        self:refreshPreview()
    end
    self.model._previewWindow:resetSelection()
    self.model._commandWindow:setVisible(true)
    self.model._commandWindow:setActive(false)
    self.model._previewWindow:setVisible(true)
    self.model._previewWindow:setActive(false)
    self.model._transition:show("FadeIn", function ()
        self.model:setActive(true)
        self.model._commandWindow:setActive(true)
        self.model._commandWindow:requestKeyboardFocus()
    end)
end

function WindowFloorTeleporterController:close(onHidden)
    self.model._commandWindow:setActive(false)
    self.model._previewWindow:setActive(false)
    self.model:setActive(false)
    self.model._transition:hide("FadeOut", function ()
        self.model._commandWindow:setVisible(false)
        self.model._previewWindow:setVisible(false)
        if onHidden ~= nil then
            onHidden()
        end
    end)
end

function WindowFloorTeleporterController:hideImmediate()
    self.model._commandWindow:setVisible(false)
    self.model._commandWindow:setActive(false)
    self.model._previewWindow:setVisible(false)
    self.model._previewWindow:setActive(false)
    self.model._transition:hideImmediate()
end

function WindowFloorTeleporterController:closeByCancel()
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:close(function ()
        if self.model._onCloseCallback ~= nil then
            self.model._onCloseCallback()
        end
    end)
end

function WindowFloorTeleporterController:refreshLocale()
    if not self.model._previewWindow:getVisible() then
        return
    end
    self.model._telepointEntriesCache = dict()
    self.model._commandWindow:refreshMaps(self:getVisitedRegionEntries())
    self:refreshPreview()
end

function WindowFloorTeleporterController:activateTelepointSelector()
    local mapKey = self.model._commandWindow:getCurrentMapKey()
    if mapKey == nil or not bool(mapKey) or not bool(self:getTelepointsForMap(mapKey)) then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self.model._commandWindow:setActive(false)
    self.model._commandWindow:setVisible(false)
    self.model._previewWindow:setActive(true)
    self.model._previewWindow:requestKeyboardFocusAtCursor()
end

function WindowFloorTeleporterController:activateMapList(playCancelSE)
    if bool(playCancelSE) then
        AudioManager.playSound(GameSystem.GetCancelSE())
    end
    self.model._previewWindow:setActive(false)
    self.model:setVisible(true)
    self.model._commandWindow:setVisible(true)
    self.model._commandWindow:setActive(true)
    self.model._commandWindow:requestKeyboardFocus()
end

function WindowFloorTeleporterController:confirmSelectedTelepoint()
    local mapKey = self.model._commandWindow:getCurrentMapKey()
    local telepoint = self:getCurrentTelepoint()
    if mapKey == nil or telepoint == nil then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:close(function ()
        if self.model._onConfirmCallback ~= nil then
            self.model._onConfirmCallback(mapKey, telepoint)
        end
    end)
end

function WindowFloorTeleporterController:notifyTelepointIndexMaybeChanged(index)
    local mapKey = self.model._commandWindow:getCurrentMapKey()
    if mapKey == nil or index == nil then
        return
    end
    local telepoints = self:getTelepointsForMap(mapKey)
    if not bool(telepoints) then
        return
    end
    self.model._telepointIndexes[mapKey] = math.clamp(index, 0, #telepoints - 1)
end

function WindowFloorTeleporterController:getCurrentTelepoint()
    local mapKey = self.model._commandWindow:getCurrentMapKey()
    if mapKey == nil then
        return nil
    end
    local telepoints = self:getTelepointsForMap(mapKey)
    if not bool(telepoints) then
        return nil
    end
    local index = self.model._telepointIndexes[mapKey] or 0
    local clampedIndex = math.clamp(index, 0, #telepoints - 1)
    ---@cast clampedIndex integer
    self.model._telepointIndexes[mapKey] = clampedIndex
    return assert(telepoints[clampedIndex + 1]).position
end

function WindowFloorTeleporterController:notifyMapIndexMaybeChanged(index)
    local mapKey = index ~= nil and self.model._commandWindow:getCurrentMapKey() or nil
    if mapKey == self.model._lastMapKey then
        return
    end
    self.model._lastMapKey = mapKey
    if mapKey ~= nil and self.model._telepointIndexes[mapKey] == nil then
        self.model._telepointIndexes[mapKey] = 0
    end
    self:refreshPreview()
end

function WindowFloorTeleporterController:refreshPreview()
    local mapKey = self.model._commandWindow:getCurrentMapKey()
    local telepoints = mapKey ~= nil and self:getTelepointsForMap(mapKey) or {}
    local selectedIndex = mapKey ~= nil and (self.model._telepointIndexes[mapKey] or 0) or 0
    local entries = self:getTelepointEntries(mapKey, telepoints)
    self.model._previewWindow:setMapKeyAndTelepoints(mapKey, entries, selectedIndex)
end

function WindowFloorTeleporterController:getVisitedRegionEntries()
    local regionMaps = RegionDict[self.model._inst:getCurrentRegion()] or {}
    local visited = self:getVisitedMapNames()
    local result = {}
    for _, mapKey in ipairs(regionMaps) do
        if visited[MapPath.WithoutExtension(mapKey)] and bool(self:getTelepointsForMap(mapKey)) then
            result[#result + 1] = { mapKey, self:getMapDisplayName(mapKey) }
        end
    end
    return result
end

function WindowFloorTeleporterController:getTelepointsForMap(mapKey)
    local normalisedMapKey = MapPath.WithoutExtension(mapKey)
    for mapPath in pairs(self.model._inst._cachedTelepoints) do
        if MapPath.WithoutExtension(tostring(mapPath)) == normalisedMapKey then
            return self.model._inst:getTelepoints(tostring(mapPath))
        end
    end
    return {}
end

function WindowFloorTeleporterController:getTelepointEntries(mapKey, telepoints)
    if mapKey == nil then
        return {}
    end
    local telepointKeys = {}
    for index, telepoint in ipairs(telepoints) do
        telepointKeys[index] = tuple { TelepointKey.FromPoint(telepoint.position), telepoint.tag }
    end
    local cacheKey = tuple { mapKey, tuple(telepointKeys) }
    local cached = self.model._telepointEntriesCache:get(cacheKey)
    if cached ~= nil then
        return cached
    end
    local result = {}
    for index, telepoint in ipairs(telepoints) do
        result[#result + 1] = { telepoint.position, formatTelepointName(telepoint.tag, index - 1) }
    end
    self.model._telepointEntriesCache[cacheKey] = result
    return result
end

function WindowFloorTeleporterController:getVisitedMapNames()
    local visited = {}
    if bool(self.model._inst._cachedMap) then
        visited[MapPath.WithoutExtension(self.model._inst._cachedMap)] = true
    end
    for mapPath in pairs(self.model._inst._cachedTelepoints) do
        visited[MapPath.WithoutExtension(tostring(mapPath))] = true
    end
    return visited
end

function WindowFloorTeleporterController:getMapDisplayName(mapKey)
    local _, mapData = SceneMapBuilder
        .new()
        :loadMapData(mapKey, self.model._inst._cachedMap or GameSystem.GetStartMap())
    local mapName = mapData.type == "worldMap" and mapData.worldName or mapData.mapName
    if not bool(mapName) then
        return LOC(tostring(mapKey))
    end
    return LOC(tostring(mapName))
end

return class(WindowFloorTeleporterController)
