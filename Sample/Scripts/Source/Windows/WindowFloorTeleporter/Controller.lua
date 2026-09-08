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

function WindowFloorTeleporterController:init(model, transition)
    self._transition = transition
    self._lastMapKey = nil
    self._telepointIndexes = {}
    self._telepointEntriesCache = dict()
    self.model = model
end

function WindowFloorTeleporterController:open()
    self.model:clearPreviewCache()
    self.model:getPreviewWindow():clearPreviewCache()
    self._telepointEntriesCache = dict()
    self._telepointIndexes = {}
    self._lastMapKey = nil
    self.model:getCommandWindow().index = nil
    local entries = self:getVisitedRegionEntries()
    self.model:getCommandWindow():refreshMaps(entries)
    self.model:getCommandWindow():resetSelection()
    local currentMapKey = self.model:getGameInstance():getCurrentMapPath() ~= nil
        and MapPath.WithoutExtension(self.model:getGameInstance():getCurrentMapPath())
        or nil
    for index, entry in ipairs(entries) do
        if MapPath.WithoutExtension(entry[1]) == currentMapKey then
            self.model:getCommandWindow():selectIndex(index - 1)
            break
        end
    end
    self:notifyMapIndexMaybeChanged(self.model:getCommandWindow().index)
    if self.model:getCommandWindow().index == nil then
        self:refreshPreview()
    end
    self.model:getPreviewWindow():resetSelection()
    self.model:getCommandWindow():setVisible(true)
    self.model:getCommandWindow():setActive(false)
    self.model:getPreviewWindow():setVisible(true)
    self.model:getPreviewWindow():setActive(false)
    self._transition:show("FadeIn", function ()
        self.model:setActive(true)
        self.model:getCommandWindow():setActive(true)
        self.model:getCommandWindow():requestKeyboardFocus()
    end)
end

function WindowFloorTeleporterController:close(onHidden)
    self.model:getCommandWindow():setActive(false)
    self.model:getPreviewWindow():setActive(false)
    self.model:setActive(false)
    self._transition:hide("FadeOut", function ()
        self.model:getCommandWindow():setVisible(false)
        self.model:getPreviewWindow():setVisible(false)
        if onHidden ~= nil then
            onHidden()
        end
    end)
end

function WindowFloorTeleporterController:hideImmediate()
    self.model:getCommandWindow():setVisible(false)
    self.model:getCommandWindow():setActive(false)
    self.model:getPreviewWindow():setVisible(false)
    self.model:getPreviewWindow():setActive(false)
    self._transition:hideImmediate()
end

function WindowFloorTeleporterController:closeByCancel()
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:close(function ()
        self.model:notifyClosed()
    end)
end

function WindowFloorTeleporterController:refreshLocale()
    if not self.model:getPreviewWindow():getVisible() then
        return
    end
    self._telepointEntriesCache = dict()
    self.model:getCommandWindow():refreshMaps(self:getVisitedRegionEntries())
    self:refreshPreview()
end

function WindowFloorTeleporterController:activateTelepointSelector()
    local mapKey = self.model:getCommandWindow():getCurrentMapKey()
    if mapKey == nil or not bool(mapKey) or not bool(self:getTelepointsForMap(mapKey)) then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self.model:getCommandWindow():setActive(false)
    self.model:getCommandWindow():setVisible(false)
    self.model:getPreviewWindow():setActive(true)
    self.model:getPreviewWindow():requestKeyboardFocusAtCursor()
end

function WindowFloorTeleporterController:activateMapList(playCancelSE)
    if bool(playCancelSE) then
        AudioManager.playSound(GameSystem.GetCancelSE())
    end
    self.model:getPreviewWindow():setActive(false)
    self.model:setVisible(true)
    self.model:getCommandWindow():setVisible(true)
    self.model:getCommandWindow():setActive(true)
    self.model:getCommandWindow():requestKeyboardFocus()
end

function WindowFloorTeleporterController:confirmSelectedTelepoint()
    local mapKey = self.model:getCommandWindow():getCurrentMapKey()
    local telepoint = self:getCurrentTelepoint()
    if mapKey == nil or telepoint == nil then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:close(function ()
        self.model:confirmTelepoint(mapKey, telepoint)
    end)
end

function WindowFloorTeleporterController:notifyTelepointIndexMaybeChanged(index)
    local mapKey = self.model:getCommandWindow():getCurrentMapKey()
    if mapKey == nil or index == nil then
        return
    end
    local telepoints = self:getTelepointsForMap(mapKey)
    if not bool(telepoints) then
        return
    end
    self._telepointIndexes[mapKey] = math.trunc(math.clamp(index, 0, #telepoints - 1))
end

function WindowFloorTeleporterController:getCurrentTelepoint()
    local mapKey = self.model:getCommandWindow():getCurrentMapKey()
    if mapKey == nil then
        return nil
    end
    local telepoints = self:getTelepointsForMap(mapKey)
    if not bool(telepoints) then
        return nil
    end
    local index = self._telepointIndexes[mapKey] or 0
    local clampedIndex = math.clamp(index, 0, #telepoints - 1)
    ---@cast clampedIndex integer
    self._telepointIndexes[mapKey] = clampedIndex
    return assert(telepoints[clampedIndex + 1]).position
end

function WindowFloorTeleporterController:notifyMapIndexMaybeChanged(index)
    local mapKey = index ~= nil and self.model:getCommandWindow():getCurrentMapKey() or nil
    if mapKey == self._lastMapKey then
        return
    end
    self._lastMapKey = mapKey
    if mapKey ~= nil and self._telepointIndexes[mapKey] == nil then
        self._telepointIndexes[mapKey] = 0
    end
    self:refreshPreview()
end

function WindowFloorTeleporterController:refreshPreview()
    local mapKey = self.model:getCommandWindow():getCurrentMapKey()
    local telepoints = mapKey ~= nil and self:getTelepointsForMap(mapKey) or {}
    local selectedIndex = mapKey ~= nil and (self._telepointIndexes[mapKey] or 0) or 0
    local entries = self:getTelepointEntries(mapKey, telepoints)
    self.model:getPreviewWindow():setMapKeyAndTelepoints(mapKey, entries, selectedIndex)
end

function WindowFloorTeleporterController:getVisitedRegionEntries()
    local regionMaps = RegionDict[self.model:getGameInstance():getCurrentRegion()] or {}
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
    return self.model:getGameInstance():getTelepointsForMap(mapKey)
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
    local cached = self._telepointEntriesCache:get(cacheKey)
    if cached ~= nil then
        return cached
    end
    local result = {}
    for index, telepoint in ipairs(telepoints) do
        result[#result + 1] = { telepoint.position, formatTelepointName(telepoint.tag, index - 1) }
    end
    self._telepointEntriesCache[cacheKey] = result
    return result
end

function WindowFloorTeleporterController:getVisitedMapNames()
    local visited = {}
    if bool(self.model:getGameInstance():getCurrentMapPath()) then
        visited[MapPath.WithoutExtension(self.model:getGameInstance():getCurrentMapPath())] = true
    end
    for _, mapPath in ipairs(self.model:getGameInstance():getVisitedMapPaths()) do
        visited[MapPath.WithoutExtension(tostring(mapPath))] = true
    end
    return visited
end

function WindowFloorTeleporterController:getMapDisplayName(mapKey)
    local _, mapData = SceneMapBuilder
        .new()
        :loadMapData(mapKey, self.model:getGameInstance():getCurrentMapPath() or GameSystem.GetStartMap())
    local mapName = mapData.type == "worldMap" and mapData.worldName or mapData.mapName
    if not bool(mapName) then
        return LOC(tostring(mapKey))
    end
    return LOC(tostring(mapName))
end

function WindowFloorTeleporterController:isBlocking()
    return self._transition:isBlocking()
end

return class(WindowFloorTeleporterController)
