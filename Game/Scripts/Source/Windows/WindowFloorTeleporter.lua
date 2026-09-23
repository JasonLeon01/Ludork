local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local RegionDict = require("Source.Configs.RegionDict")
local LocaleCore = require("Source.Locale.Core")
local MapPath = require("Source.Utils.MapPath")
local SceneMapBuilder = require("Source.SceneComponents.MapBuilder")
local GameSystem = require("Source.System")
local TelepointKey = require("Source.Configs.TelepointKey")
local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.WindowFloorTeleporter")
local WindowFloorMapCommand = require("Source.Windows.WindowFloorTeleporter.Command")
local WindowFloorMapPreview = require("Source.Windows.WindowFloorTeleporter.Preview")

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

local Canvas = Engine.Canvas

---@class Source.Windows.WindowFloorTeleporter.Controller
local Controller = {}

Controller.windowOptions = { centered = true, hidden = true }

function Controller:init(inst, loadPreview, onConfirm, onClose, resolvePreviewMapPath, clearPreviewCache)
    self._inst = inst
    self._onConfirmCallback = onConfirm
    self._onCloseCallback = onClose
    self._clearPreviewCacheCallback = clearPreviewCache
    self._lastMapKey = nil
    self._telepointIndexes = {}
    self._telepointEntriesCache = dict()
    self._previewWindow = self:createChild(
        "PreviewAsset", WindowFloorMapPreview, self.host, loadPreview, resolvePreviewMapPath
    )
    self._commandWindow = self:createChild("CommandAsset", WindowFloorMapCommand, self.host)
end

function Controller:getCommandWindow()
    return self._commandWindow
end

function Controller:getPreviewWindow()
    return self._previewWindow
end

function Controller:getVisible()
    return self:isBlocking()
end

function Controller:open(inst)
    if inst ~= nil then
        self._inst = inst
    end
    self:clearPreviewCache()
    self:getPreviewWindow():clearPreviewCache()
    self._telepointEntriesCache = dict()
    self._telepointIndexes = {}
    self._lastMapKey = nil
    self:getCommandWindow().index = nil
    local entries = self:getVisitedRegionEntries()
    self:getCommandWindow():refreshMaps(entries)
    self:getCommandWindow():resetSelection()
    local currentMapKey = self:getGameInstance():getCurrentMapPath() ~= nil
        and MapPath.WithoutExtension(self:getGameInstance():getCurrentMapPath())
        or nil
    for index, entry in ipairs(entries) do
        if MapPath.WithoutExtension(entry[1]) == currentMapKey then
            self:getCommandWindow():selectIndex(index - 1)
            break
        end
    end
    self:notifyMapIndexMaybeChanged(self:getCommandWindow().index)
    if self:getCommandWindow().index == nil then
        self:refreshPreview()
    end
    self:getPreviewWindow():resetSelection()
    self:getCommandWindow():setVisible(true)
    self:getCommandWindow():setActive(false)
    self:getPreviewWindow():setVisible(true)
    self:getPreviewWindow():setActive(false)
    self._transition:show("FadeIn", function ()
        self.host:setActive(true)
        self:getCommandWindow():setActive(true)
        self:getCommandWindow():requestKeyboardFocus()
    end)
end

function Controller:close(onHidden)
    self:getCommandWindow():setActive(false)
    self:getPreviewWindow():setActive(false)
    self.host:setActive(false)
    self._transition:hide("FadeOut", function ()
        self:getCommandWindow():setVisible(false)
        self:getPreviewWindow():setVisible(false)
        if onHidden ~= nil then
            onHidden()
        end
    end)
end

function Controller:closeByCancel()
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:close(self:bindCallback(Controller.notifyClosed))
end

function Controller:refreshLocale()
    if not self:getPreviewWindow():getVisible() then
        return
    end
    self._telepointEntriesCache = dict()
    self:getCommandWindow():refreshMaps(self:getVisitedRegionEntries())
    self:refreshPreview()
end

function Controller:activateTelepointSelector()
    local mapKey = self:getCommandWindow():getCurrentMapKey()
    if mapKey == nil or not bool(mapKey) or not bool(self:getTelepointsForMap(mapKey)) then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:getCommandWindow():setActive(false)
    self:getCommandWindow():setVisible(false)
    self:getPreviewWindow():setActive(true)
    self:getPreviewWindow():requestKeyboardFocusAtCursor()
end

function Controller:activateMapList(playCancelSE)
    if bool(playCancelSE) then
        AudioManager.playSound(GameSystem.GetCancelSE())
    end
    self:getPreviewWindow():setActive(false)
    self.host:setVisible(true)
    self:getCommandWindow():setVisible(true)
    self:getCommandWindow():setActive(true)
    self:getCommandWindow():requestKeyboardFocus()
end

function Controller:confirmSelectedTelepoint()
    local mapKey = self:getCommandWindow():getCurrentMapKey()
    local telepoint = self:getCurrentTelepoint()
    if mapKey == nil or telepoint == nil then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    self:close(function ()
        self:confirmTelepoint(mapKey, telepoint)
    end)
end

function Controller:notifyTelepointIndexMaybeChanged(index)
    local mapKey = self:getCommandWindow():getCurrentMapKey()
    if mapKey == nil or index == nil then
        return
    end
    local telepoints = self:getTelepointsForMap(mapKey)
    if not bool(telepoints) then
        return
    end
    self._telepointIndexes[mapKey] = math.trunc(math.clamp(index, 0, #telepoints - 1))
end

function Controller:getCurrentTelepoint()
    local mapKey = self:getCommandWindow():getCurrentMapKey()
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

function Controller:notifyMapIndexMaybeChanged(index)
    local mapKey = index ~= nil and self:getCommandWindow():getCurrentMapKey() or nil
    if mapKey == self._lastMapKey then
        return
    end
    self._lastMapKey = mapKey
    if mapKey ~= nil and self._telepointIndexes[mapKey] == nil then
        self._telepointIndexes[mapKey] = 0
    end
    self:refreshPreview()
end

function Controller:dispose()
    self:hideImmediate()
    super(Controller, self).dispose()
    self._inst = nil
    self._onConfirmCallback = nil
    self._onCloseCallback = nil
    self._clearPreviewCacheCallback = nil
end

function Controller:getGameInstance()
    return self._inst
end

function Controller:clearPreviewCache()
    if self._clearPreviewCacheCallback ~= nil then
        self._clearPreviewCacheCallback()
    end
end

function Controller:notifyClosed()
    if self._onCloseCallback ~= nil then
        self._onCloseCallback()
    end
end

function Controller:confirmTelepoint(mapKey, telepoint)
    if self._onConfirmCallback ~= nil then
        self._onConfirmCallback(mapKey, telepoint)
    end
end

function Controller:hideImmediate()
    self:getCommandWindow():setVisible(false)
    self:getCommandWindow():setActive(false)
    self:getPreviewWindow():setVisible(false)
    self:getPreviewWindow():setActive(false)
    self._transition:hideImmediate()
end

function Controller:refreshPreview()
    local mapKey = self:getCommandWindow():getCurrentMapKey()
    local telepoints = mapKey ~= nil and self:getTelepointsForMap(mapKey) or {}
    local selectedIndex = mapKey ~= nil and (self._telepointIndexes[mapKey] or 0) or 0
    local entries = self:getTelepointEntries(mapKey, telepoints)
    self:getPreviewWindow():setMapKeyAndTelepoints(mapKey, entries, selectedIndex)
end

function Controller:getVisitedRegionEntries()
    local regionMaps = RegionDict[self:getGameInstance():getCurrentRegion()] or {}
    local visited = self:getVisitedMapNames()
    local result = {}
    for _, mapKey in ipairs(regionMaps) do
        if visited[MapPath.WithoutExtension(mapKey)] and bool(self:getTelepointsForMap(mapKey)) then
            result[#result + 1] = { mapKey, self:getMapDisplayName(mapKey) }
        end
    end
    return result
end

function Controller:getTelepointsForMap(mapKey)
    return self:getGameInstance():getTelepointsForMap(mapKey)
end

function Controller:getTelepointEntries(mapKey, telepoints)
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

function Controller:getVisitedMapNames()
    local visited = {}
    if bool(self:getGameInstance():getCurrentMapPath()) then
        visited[MapPath.WithoutExtension(self:getGameInstance():getCurrentMapPath())] = true
    end
    for _, mapPath in ipairs(self:getGameInstance():getVisitedMapPaths()) do
        visited[MapPath.WithoutExtension(tostring(mapPath))] = true
    end
    return visited
end

function Controller:getMapDisplayName(mapKey)
    local _, mapData = SceneMapBuilder
        .new()
        :loadMapData(mapKey, self:getGameInstance():getCurrentMapPath() or GameSystem.GetStartMap())
    local mapName = mapData.type == "worldMap" and mapData.worldName or mapData.mapName
    if not bool(mapName) then
        return LOC(tostring(mapKey))
    end
    return LOC(tostring(mapName))
end

function Controller:isBlocking()
    return self._transition:isBlocking()
end

return Ui.DefineWindow(View, Controller, Canvas)
