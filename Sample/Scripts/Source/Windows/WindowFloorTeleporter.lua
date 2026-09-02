local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local RegionDict = require("Source.Configs.RegionDict")
local LocaleCore = require("Source.Locale.Core")
local MapPath = require("Source.MapPath")
local SceneMapBuilder = require("Source.SceneComponents.MapBuilder")
local GameSystem = require("Source.System")
local WindowFloorTeleporterUI = require("Source.UI.WindowFloorTeleporter")
local TelepointKey = require("Source.UI.Helpers.TelepointKey")
local UiLayout = require("Source.UI.UiLayout")
local WindowFloorMapCommand = require("Source.Windows.WindowFloorTeleporter.Command")
local WindowFloorMapPreview = require("Source.Windows.WindowFloorTeleporter.Preview")

local ManagerFunctions = GlobalFunctions.Manager
local Canvas = Engine.Canvas
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local _LIST_WIDTH = 176
local _TELEPOINT_PREVIEW_WIDTH = 416
local _PREVIEW_WINDOW_HEIGHT = 240

local function formatMapName(mapName)
    return LOC(tostring(mapName))
end
local function getDefaultRects()
    local bounds = UiLayout.GetCenteredRect(_TELEPOINT_PREVIEW_WIDTH, _PREVIEW_WINDOW_HEIGHT)
    return Engine.ToIntRect(bounds.position.x, bounds.position.y, _LIST_WIDTH, _PREVIEW_WINDOW_HEIGHT),
        Engine.ToIntRect(bounds.position.x, bounds.position.y, _TELEPOINT_PREVIEW_WIDTH, _PREVIEW_WINDOW_HEIGHT)
end
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
    self.model._commandWindow:refreshMaps(self:getVisitedRegionEntries())
    self.model._commandWindow:resetSelection()
    self.model._previewWindow:resetSelection()
    self.model._commandWindow:setVisible(true)
    self.model._commandWindow:setActive(true)
    self.model._previewWindow:setVisible(true)
    self.model._previewWindow:setActive(false)
    self.model:setVisible(true)
    self.model._commandWindow:requestKeyboardFocus()
end

function WindowFloorTeleporterController:close()
    self.model._commandWindow:setVisible(false)
    self.model._commandWindow:setActive(false)
    self.model._previewWindow:setVisible(false)
    self.model._previewWindow:setActive(false)
end

function WindowFloorTeleporterController:closeByCancel()
    ManagerFunctions.playSE(GameSystem.GetCancelSE())
    self:close()
    if self.model._onCloseCallback ~= nil then
        self.model._onCloseCallback()
    end
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
    if not bool(mapKey) or not bool(self:getTelepointsForMap(mapKey)) then
        ManagerFunctions.playSE(GameSystem.GetBuzzerSE())
        return
    end
    ManagerFunctions.playSE(GameSystem.GetDecisionSE())
    self.model._commandWindow:setActive(false)
    self.model._commandWindow:setVisible(false)
    self.model._previewWindow:setActive(true)
    self.model._previewWindow:requestKeyboardFocusAtCursor()
end

function WindowFloorTeleporterController:activateMapList(playCancelSE)
    if bool(playCancelSE) then
        ManagerFunctions.playSE(GameSystem.GetCancelSE())
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
        ManagerFunctions.playSE(GameSystem.GetBuzzerSE())
        return
    end
    ManagerFunctions.playSE(GameSystem.GetDecisionSE())
    if self.model._onConfirmCallback ~= nil then
        self.model._onConfirmCallback(mapKey, telepoint)
    end
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
    self.model._telepointIndexes[mapKey] = Engine.Clamp(index, 0, #telepoints - 1)
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
    index = Engine.Clamp(index, 0, #telepoints - 1)
    self.model._telepointIndexes[mapKey] = index
    return telepoints[index + 1]
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
        telepointKeys[index] = TelepointKey.FromPoint(telepoint)
    end
    local cacheKey = tuple { mapKey, tuple(telepointKeys) }
    local cached = self.model._telepointEntriesCache:get(cacheKey)
    if cached ~= nil then
        return cached
    end
    local result = {}
    for index, telepoint in ipairs(telepoints) do
        result[#result + 1] = { telepoint, self:formatTelepointName(mapKey, telepoint, index - 1) }
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
        return formatMapName(mapKey)
    end
    return formatMapName(tostring(mapName))
end

function WindowFloorTeleporterController:formatTelepointName(mapKey, telepoint, index)
    local tag = self.model._getTelepointTagCallback ~= nil and self.model._getTelepointTagCallback(mapKey, telepoint)
        or nil
    if tag ~= nil and bool(tag) and not string.startsWith(tag, "Data.Blueprints.Teleportations") then
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

local FinalWindowFloorTeleporterController = class(WindowFloorTeleporterController)

---@class Source.Windows.WindowFloorTeleporter
local WindowFloorTeleporter = {}

WindowFloorTeleporter.controllerClass = FinalWindowFloorTeleporterController

function WindowFloorTeleporter:init(
    inst, _listRect, previewRect, loadPreview, onConfirm, onClose, getTelepointTag, resolvePreviewMapPath,
    clearPreviewCache
)
    super(WindowFloorTeleporter, self).init(Engine.ToIntRect(
        previewRect.position.x, previewRect.position.y, _TELEPOINT_PREVIEW_WIDTH, _PREVIEW_WINDOW_HEIGHT
    ))
    self._inst = inst
    self._getTelepointTagCallback = getTelepointTag
    self._onConfirmCallback = onConfirm
    self._onCloseCallback = onClose
    self._clearPreviewCacheCallback = clearPreviewCache
    self._ui = WindowFloorTeleporterUI.new(self)
    self._ui:attach()
    self._commandWindow = WindowFloorMapCommand.new(
        Engine.ToIntRect(0, 0, _LIST_WIDTH, _PREVIEW_WINDOW_HEIGHT), self, self._ui:getCommandAsset()
    )
    self._previewWindow = WindowFloorMapPreview.new(
        Engine.ToIntRect(0, 0, _TELEPOINT_PREVIEW_WIDTH, _PREVIEW_WINDOW_HEIGHT), self, loadPreview,
        resolvePreviewMapPath, self._ui:getPreviewAsset()
    )
    self:addChild(self._previewWindow)
    self:addChild(self._commandWindow)
    self._lastMapKey = nil
    self._telepointIndexes = {}
    self._telepointEntriesCache = dict()
    self._teleporterController = self.controllerClass.new(self)
    self._teleporterController:close()
end

function WindowFloorTeleporter:getCommandWindow()
    return self._commandWindow
end

function WindowFloorTeleporter:getPreviewWindow()
    return self._previewWindow
end

function WindowFloorTeleporter:getVisible()
    return self._previewWindow:getVisible()
end

function WindowFloorTeleporter:open(inst)
    self._teleporterController:open(inst)
end

function WindowFloorTeleporter:close()
    self._teleporterController:close()
end

function WindowFloorTeleporter:closeByCancel()
    self._teleporterController:closeByCancel()
end

function WindowFloorTeleporter:refreshLocale()
    self._teleporterController:refreshLocale()
end

function WindowFloorTeleporter:activateTelepointSelector()
    self._teleporterController
        :activateTelepointSelector()
end

function WindowFloorTeleporter:activateMapList(playCancelSE)
    self._teleporterController:activateMapList(playCancelSE)
end

function WindowFloorTeleporter:confirmSelectedTelepoint()
    self._teleporterController
        :confirmSelectedTelepoint()
end

function WindowFloorTeleporter:notifyTelepointIndexMaybeChanged(index)
    self._teleporterController
        :notifyTelepointIndexMaybeChanged(index)
end

function WindowFloorTeleporter:getCurrentTelepoint()
    return self._teleporterController
        :getCurrentTelepoint()
end

function WindowFloorTeleporter:notifyMapIndexMaybeChanged(index)
    self._teleporterController
        :notifyMapIndexMaybeChanged(index)
end

function WindowFloorTeleporter.GetDefaultFloorTeleporterRects()
    return getDefaultRects()
end

function WindowFloorTeleporter:dispose()
    self:close()
    self._ui:dispose()
    self._inst = nil
    self._getTelepointTagCallback = nil
    self._onConfirmCallback = nil
    self._onCloseCallback = nil
    self._clearPreviewCacheCallback = nil
end

return class(WindowFloorTeleporter, Canvas)
