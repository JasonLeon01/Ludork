local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local RegionDict = require("Source.Config.RegionDict")
local LocaleCore = require("Source.Locale.Core")
local MapPath = require("Source.MapPath")
local SceneMapBuilder = require("Source.SceneComponents.MapBuilder")
local GameSystem = require("Source.System")
local WindowFloorMapPreviewUI = require("Source.UI.Parts.WindowFloorTeleporter.WindowFloorMapPreview")
local UiLayout = require("Source.UI.UiLayout")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local WindowCommand = require("Source.Windows.WindowCommand")

local Input = Engine.Input
local ManagerFunctions = GlobalFunctions.Manager
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat
local WindowCommandController = WindowCommand.Controller

local _LIST_WIDTH = 208
local _PREVIEW_WINDOW_WIDTH = 240
local _PREVIEW_WINDOW_HEIGHT = 280
local _LIST_ROW_HEIGHT = 32
local _TELEPOINT_LIST_HEIGHT = 32

---@param point sf.Vector2u
---@return tuple<any>
local function telepointKey(point)
    return tuple { point.x, point.y }
end

---@class Source.Windows.WindowFloorMapCommandController: Source.Windows.WindowCommand.Controller
local WindowFloorMapCommandController = {}

function WindowFloorMapCommandController:init(model, size, rowHeight, columns)
    super(WindowFloorMapCommandController, self).init(model, size, rowHeight, columns)
    ---@cast self.model Source.Windows.WindowFloorMapCommand
    self._mapKeys = {}
    self.model._mapKeys = self._mapKeys
end

function WindowFloorMapCommandController:refreshMaps(entries)
    ---@cast self.model Source.Windows.WindowFloorMapCommand
    local previousMapKey = self:getCurrentMapKey()
    self._mapKeys = {}
    self.model._mapKeys = self._mapKeys
    self._rowControllers = {}
    self.root:clearChildren()
    for index, entry in ipairs(entries) do
        self._mapKeys[index] = entry[1]
        local child = self:createRow({
            text = entry[2],
            callback = function ()
                self.model._owner:activateTelepointSelector()
            end
        })
        self.model:_applyItem(child)
        self.root:addChild(child)
    end
    self:prepare()
    if not bool(self._mapKeys) then
        self.model.index = nil
    else
        local previousIndex = nil
        if previousMapKey ~= nil then
            for index, mapKey in ipairs(self._mapKeys) do
                if mapKey == previousMapKey then
                    previousIndex = index - 1
                    break
                end
            end
        end
        self.model.index = previousIndex or 0
    end
    self.model._owner:notifyMapIndexMaybeChanged(self.model.index)
end

function WindowFloorMapCommandController:getCurrentMapKey()
    local index = self.model.index
    if index == nil or index >= #self._mapKeys then
        return nil
    end
    return self._mapKeys[index + 1]
end

function WindowFloorMapCommandController:afterTick()
    ---@cast self.model Source.Windows.WindowFloorMapCommand
    self.model._owner:notifyMapIndexMaybeChanged(self.model.index)
end

function WindowFloorMapCommandController:handleKeyDown()
    ---@cast self.model Source.Windows.WindowFloorMapCommand
    if not Input.isActionTriggered(Input.getCancelKeys(), false) then
        return false
    end
    self.model:onReturn()
    Input.isActionTriggered(Input.getCancelKeys(), true)
    return true
end

function WindowFloorMapCommandController:handleMouseButtonDown(kwargs)
    ---@cast self.model Source.Windows.WindowFloorMapCommand
    if kwargs.button ~= sf.Mouse.Button.Right then
        return false
    end
    self.model:onReturn()
    return true
end

local FinalWindowFloorMapCommandController = class(WindowFloorMapCommandController, WindowCommandController)

---@class Source.Windows.WindowFloorMapCommand: Source.Windows.WindowCommand
local WindowFloorMapCommand = {}

WindowFloorMapCommand.controllerClass = FinalWindowFloorMapCommandController

function WindowFloorMapCommand:init(rect, owner)
    self._owner = owner
    super(WindowFloorMapCommand, self).init(rect, {}, nil, _LIST_ROW_HEIGHT)
    self:setHasReturnBtn(true)
    ---@cast self._commandController Source.Windows.WindowFloorMapCommandController
    self._mapController = self._commandController
end

function WindowFloorMapCommand:refreshMaps(entries)
    self._mapController:refreshMaps(entries)
end

function WindowFloorMapCommand:getCurrentMapKey()
    return self._mapController:getCurrentMapKey()
end

function WindowFloorMapCommand:onTick(deltaTime)
    super(WindowFloorMapCommand, self).onTick(deltaTime)
    self._mapController:afterTick()
end

function WindowFloorMapCommand:onKeyDown(kwargs)
    if self._mapController:handleKeyDown() then
        return
    end
    super(WindowFloorMapCommand, self).onKeyDown(kwargs)
end

function WindowFloorMapCommand:onMouseButtonDown(kwargs)
    return self._mapController:handleMouseButtonDown(kwargs)
end

function WindowFloorMapCommand:onReturn()
    self._owner:closeByCancel()
end

---@type Class.ClassType<Source.Windows.WindowFloorMapCommand>
local FinalWindowFloorMapCommand = class(WindowFloorMapCommand, WindowCommand)

local WindowFloorMapPreview = {}

WindowFloorMapPreview.uiClass = WindowFloorMapPreviewUI

function WindowFloorMapPreview:init(rect, owner, loadPreview, resolvePreviewMapPath)
    self._owner = owner
    self._telepointItemWidth = self.uiClass.getTelepointItemWidth(rect)
    super(WindowFloorMapPreview, self).init(rect, nil, self._telepointItemWidth, _TELEPOINT_LIST_HEIGHT)
    self:setHasReturnBtn(true)
    self._previewUI = self.uiClass.new(self, rect.size, loadPreview, resolvePreviewMapPath)
    self._previewUI:attach()
    self._listView = self._previewUI:getListView()
end

function WindowFloorMapPreview:clearPreviewCache()
    self._previewUI:clearPreviewCache()
end

function WindowFloorMapPreview:setActive(active)
    local wasActive = self:getActive()
    super(WindowFloorMapPreview, self).setActive(active)
    self._previewUI:onActiveChanged(active, wasActive)
end

function WindowFloorMapPreview:setMapKeyAndTelepoints(mapKey, entries, selectedIndex)
    self._previewUI:setMapKeyAndTelepoints(mapKey, entries, selectedIndex)
end

function WindowFloorMapPreview:onTick(deltaTime)
    local previousIndex = self.index
    super(WindowFloorMapPreview, self).onTick(deltaTime)
    self._previewUI:afterSelectionUpdate(previousIndex)
end

function WindowFloorMapPreview:onKeyDown(kwargs)
    if self._previewUI:handleKeyDown() then
        return
    end
    local previousIndex = self.index
    super(WindowFloorMapPreview, self).onKeyDown(kwargs)
    self._previewUI:afterSelectionUpdate(previousIndex)
end

function WindowFloorMapPreview:onMouseButtonDown(kwargs)
    return self._previewUI
        :handleMouseButtonDown(kwargs)
end

function WindowFloorMapPreview:onReturn()
    self._owner:activateMapList(true)
end

---@param entries table
function WindowFloorMapPreview:_rebuildTelepointList(entries)
    self._previewUI:rebuildTelepointList(entries)
end

function WindowFloorMapPreview:_refreshSelectedPreview()
    self._previewUI:refreshSelectedPreview()
end

---@return sf.Vector2u | nil
function WindowFloorMapPreview:_getSelectedTelepoint()
    return self._previewUI:getSelectedTelepoint()
end

function WindowFloorMapPreview:_hidePreview()
    self._previewUI:hidePreview()
end

---@return integer
function WindowFloorMapPreview:_getRectWidth()
    return self._telepointItemWidth
end

---@param rect sf.IntRect
---@return integer
function WindowFloorMapPreview.GetTelepointItemWidth(rect)
    return WindowFloorMapPreviewUI
        .getTelepointItemWidth(rect)
end

local FinalWindowFloorMapPreview = class(WindowFloorMapPreview, WindowSelectable)

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
    self.model._lastMapKey = nil
    self.model._commandWindow:refreshMaps(self:getVisitedRegionEntries())
    self.model._commandWindow:setVisible(true)
    self.model._commandWindow:setActive(true)
    self.model._previewWindow:setVisible(true)
    self.model._previewWindow:setActive(false)
    self.model._commandWindow:requestKeyboardFocus()
end

function WindowFloorTeleporterController:close()
    self.model._commandWindow:setVisible(false)
    self.model._commandWindow:setActive(false)
    self.model._previewWindow:setVisible(false)
    self.model._previewWindow:setActive(false)
end

function WindowFloorTeleporterController:closeByCancel()
    ManagerFunctions.playSE(GameSystem.getCancelSE())
    self:close()
    if self.model._onCloseCallback ~= nil then
        self.model._onCloseCallback()
    end
end

function WindowFloorTeleporterController:refreshLocale()
    if not self.model._commandWindow:getVisible() then
        return
    end
    self.model._telepointEntriesCache = dict()
    self.model._commandWindow:refreshMaps(self:getVisitedRegionEntries())
    self:refreshPreview()
end

function WindowFloorTeleporterController:activateTelepointSelector()
    local mapKey = self.model._commandWindow:getCurrentMapKey()
    if not bool(mapKey) or not bool(self:getTelepointsForMap(mapKey)) then
        ManagerFunctions.playSE(GameSystem.getBuzzerSE())
        return
    end
    ManagerFunctions.playSE(GameSystem.getDecisionSE())
    self.model._commandWindow:setActive(false)
    self.model._previewWindow:setActive(true)
    self.model._previewWindow:requestKeyboardFocusAtCursor()
end

function WindowFloorTeleporterController:activateMapList(playCancelSE)
    if bool(playCancelSE) then
        ManagerFunctions.playSE(GameSystem.getCancelSE())
    end
    self.model._previewWindow:setActive(false)
    self.model._commandWindow:setActive(true)
    self.model._commandWindow:requestKeyboardFocus()
end

function WindowFloorTeleporterController:confirmSelectedTelepoint()
    local mapKey = self.model._commandWindow:getCurrentMapKey()
    local telepoint = self:getCurrentTelepoint()
    if mapKey == nil or telepoint == nil then
        ManagerFunctions.playSE(GameSystem.getBuzzerSE())
        return
    end
    ManagerFunctions.playSE(GameSystem.getDecisionSE())
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
    self.model._telepointIndexes[mapKey] = math.max(0, math.min(index, #telepoints - 1))
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
    index = math.max(0, math.min(index, #telepoints - 1))
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
        if visited[WindowFloorTeleporterController.NormaliseMapName(mapKey)]
            and bool(self:getTelepointsForMap(mapKey))
        then
            result[#result + 1] = { mapKey, self:getMapDisplayName(mapKey) }
        end
    end
    return result
end

function WindowFloorTeleporterController:getTelepointsForMap(mapKey)
    local telepoints = self.model._inst._cachedTelepoints
    local normalisedMapKey = WindowFloorTeleporterController.NormaliseMapName(mapKey)
    for mapPath in pairs(telepoints) do
        if WindowFloorTeleporterController.NormaliseMapName(tostring(mapPath)) == normalisedMapKey then
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
        telepointKeys[index] = telepointKey(telepoint)
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
    local cachedMap = self.model._inst._cachedMap
    if bool(cachedMap) then
        visited[WindowFloorTeleporterController.NormaliseMapName(cachedMap)] = true
    end
    local telepoints = self.model._inst._cachedTelepoints
    for mapPath in pairs(telepoints) do
        visited[WindowFloorTeleporterController.NormaliseMapName(tostring(mapPath))] = true
    end
    return visited
end

function WindowFloorTeleporterController:getMapDisplayName(mapKey)
    local _, mapData = SceneMapBuilder
        .new()
        :loadMapData(mapKey, self.model._inst._cachedMap or GameSystem.getStartMap())
    local mapName = mapData.mapName
    if not bool(mapName) then
        return WindowFloorTeleporterController
            .FormatMapName(mapKey)
    end
    return WindowFloorTeleporterController.FormatMapName(tostring(mapName))
end

function WindowFloorTeleporterController:formatTelepointName(mapKey, telepoint, index)
    local tag = self.model._getTelepointTagCallback ~= nil and self.model._getTelepointTagCallback(mapKey, telepoint)
        or nil
    if tag ~= nil and bool(tag) and tag:sub(1, #"Data.Blueprints.Teleportations") ~= "Data.Blueprints.Teleportations" then
        return LOC(tag)
    end
    local fallback = "Point_" .. tostring(index + 1)
    local pointFormat = LOC("POINT")
    if pointFormat == "POINT" then
        return fallback
    end
    local pointNumber = tostring(index + 1)
    local formatted = pointFormat:gsub("{index}", pointNumber)
    formatted = formatted:gsub("{0}", pointNumber)
    if formatted == pointFormat then
        return fallback
    end
    return formatted
end

function WindowFloorTeleporterController.NormaliseMapName(mapPath)
    local path = MapPath.Normalise(mapPath)
    return path:gsub("%.[^%.]+$", "")
end

function WindowFloorTeleporterController.FormatMapName(mapName)
    return LOC(tostring(mapName))
end

function WindowFloorTeleporterController.GetDefaultRects()
    local totalWidth = _LIST_WIDTH + _PREVIEW_WINDOW_WIDTH
    local bounds = UiLayout.GetCenteredRect(totalWidth, _PREVIEW_WINDOW_HEIGHT)
    return Engine.ToIntRect(bounds.position.x, bounds.position.y, _LIST_WIDTH, _PREVIEW_WINDOW_HEIGHT),
        Engine.ToIntRect(
            bounds.position.x + _LIST_WIDTH, bounds.position.y, _PREVIEW_WINDOW_WIDTH, _PREVIEW_WINDOW_HEIGHT
        )
end

local FinalWindowFloorTeleporterController = class(WindowFloorTeleporterController)

local WindowFloorTeleporter = {}

WindowFloorTeleporter.controllerClass = FinalWindowFloorTeleporterController

function WindowFloorTeleporter:init(
    inst, listRect, previewRect, loadPreview, onConfirm, onClose, getTelepointTag, resolvePreviewMapPath,
    clearPreviewCache
)
    self._inst = inst
    self._getTelepointTagCallback = getTelepointTag
    self._onConfirmCallback = onConfirm
    self._onCloseCallback = onClose
    self._clearPreviewCacheCallback = clearPreviewCache
    self._commandWindow = FinalWindowFloorMapCommand.new(listRect, self)
    self._previewWindow = FinalWindowFloorMapPreview.new(previewRect, self, loadPreview, resolvePreviewMapPath)
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
    return self._commandWindow:getVisible()
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

function WindowFloorTeleporter:_refreshPreview()
    self._teleporterController:refreshPreview()
end

---@return table
function WindowFloorTeleporter:_getVisitedRegionEntries()
    return self._teleporterController
        :getVisitedRegionEntries()
end

---@param mapKey string
---@return sf.Vector2u[]
function WindowFloorTeleporter:_getTelepointsForMap(mapKey)
    return self._teleporterController
        :getTelepointsForMap(mapKey)
end

---@param mapKey     string | nil
---@param telepoints sf.Vector2u[]
---@return table
function WindowFloorTeleporter:_getTelepointEntries(mapKey, telepoints)
    return self._teleporterController
        :getTelepointEntries(mapKey, telepoints)
end

---@return table
function WindowFloorTeleporter:_getVisitedMapNames()
    return self._teleporterController
        :getVisitedMapNames()
end

---@param mapKey string
---@return string
function WindowFloorTeleporter:_getMapDisplayName(mapKey)
    return self._teleporterController
        :getMapDisplayName(mapKey)
end

---@param mapName string
---@return string
function WindowFloorTeleporter.FormatMapName(mapName)
    return WindowFloorTeleporterController
        .FormatMapName(mapName)
end

---@param mapKey    string
---@param telepoint sf.Vector2u
---@param index     integer
---@return string
function WindowFloorTeleporter:_formatTelepointName(mapKey, telepoint, index)
    return self._teleporterController
        :formatTelepointName(mapKey, telepoint, index)
end

local WindowFloorTeleporterExports = {}

function WindowFloorTeleporterExports.GetDefaultFloorTeleporterRects()
    return WindowFloorTeleporterController
        .GetDefaultRects()
end

WindowFloorTeleporter.GetDefaultFloorTeleporterRects = WindowFloorTeleporterExports
    .GetDefaultFloorTeleporterRects

local FinalWindowFloorTeleporter = class(WindowFloorTeleporter)

WindowFloorTeleporterExports.WindowFloorMapCommand = FinalWindowFloorMapCommand
WindowFloorTeleporterExports.WindowFloorMapPreview = FinalWindowFloorMapPreview
WindowFloorTeleporterExports.WindowFloorTeleporter = FinalWindowFloorTeleporter

return WindowFloorTeleporterExports
