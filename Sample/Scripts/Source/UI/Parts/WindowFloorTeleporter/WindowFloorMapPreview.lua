local Engine = require("Engine")
local CommandRowController = require("Source.UI.Helpers.CommandRow")
local Ui = require("Source.UI.Ui")

local Input = Engine.Input

local _PREVIEW_CONTENT_SIZE = 208
local _PREVIEW_SCALE = 0.5
local _TELEPOINT_LIST_HEIGHT = 32

---@param point sf.Vector2u | nil
---@return tuple<any>
local function telepointKey(point)
    if point == nil then
        return tuple()
    end
    return tuple { point.x, point.y }
end

---@param entries table
---@return tuple<any>
local function entriesKey(entries)
    local parts = {}
    for index, entry in ipairs(entries) do
        parts[index] = tuple { telepointKey(entry[1]), tostring(entry[2]) }
    end
    return tuple(parts)
end

local WindowFloorMapPreviewUI = {}

function WindowFloorMapPreviewUI:init(model, size, loadPreview, resolvePreviewMapPath)
    self._logicalSize = sf.Vector2u.new(size.x, size.y)
    self._loadPreview = loadPreview
    self._resolvePreviewMapPath = resolvePreviewMapPath
    self._columns = 1
    self._rowControllers = {}
    model._loadPreview = loadPreview
    model._resolvePreviewMapPath = resolvePreviewMapPath
    model._mapKey = nil
    model._telepoints = {}
    model._currentListKey = nil
    model._currentPreviewKey = nil
    model._previewTextureCache = dict()
    super(WindowFloorMapPreviewUI, self).init(model)
end

function WindowFloorMapPreviewUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._listView = self:requireControl("TelepointList")
    self._previewImage = self:requireControl("PreviewImage")
    self.model._previewImage = self._previewImage
end

function WindowFloorMapPreviewUI:refresh()
    self:_applyListLayout()
    self:setProperty("PreviewImage", "visible", false)
end

function WindowFloorMapPreviewUI:prepare()
    return super(WindowFloorMapPreviewUI, self).prepare(self._logicalSize)
end

function WindowFloorMapPreviewUI:attach()
    self:attachWindowView(self.model)
end

function WindowFloorMapPreviewUI:getWindowFrame()
    return self._windowFrame
end

function WindowFloorMapPreviewUI:getContent()
    return self._content
end

function WindowFloorMapPreviewUI:getListView()
    return self._listView
end

function WindowFloorMapPreviewUI:clearPreviewCache()
    self.model._previewTextureCache = dict()
    self:hidePreview()
end

function WindowFloorMapPreviewUI:onActiveChanged(active, wasActive)
    if not active then
        self.model._rect:setVisible(false)
    end
    if active ~= wasActive then
        self:refreshSelectedPreview()
    end
end

function WindowFloorMapPreviewUI:setMapKeyAndTelepoints(mapKey, entries, selectedIndex)
    local listKey = tuple { tostring(mapKey or ""), entriesKey(entries) }
    if listKey ~= self.model._currentListKey then
        self.model._currentListKey = listKey
        self.model._mapKey = mapKey
        self.model._telepoints = {}
        for index, entry in ipairs(entries) do
            self.model._telepoints[index] = entry[1]
        end
        self:rebuildTelepointList(entries)
    end
    if not bool(entries) then
        self.model.index = nil
        self:hidePreview()
        return
    end
    self.model.index = math.max(0, math.min(selectedIndex, #entries - 1))
    self:refreshSelectedPreview()
end

function WindowFloorMapPreviewUI:afterSelectionUpdate(previousIndex)
    if not self.model:getActive() then
        self.model._rect:setVisible(false)
    end
    if self.model.index == previousIndex then
        return
    end
    self.model._owner:notifyTelepointIndexMaybeChanged(self.model.index)
    self:refreshSelectedPreview()
end

function WindowFloorMapPreviewUI:handleKeyDown()
    if not Input.isActionTriggered(Input.getCancelKeys(), false) then
        return false
    end
    self.model:onReturn()
    Input.isActionTriggered(Input.getCancelKeys(), true)
    return true
end

function WindowFloorMapPreviewUI:handleMouseButtonDown(kwargs)
    if kwargs.button ~= sf.Mouse.Button.Right then
        return false
    end
    self.model:onReturn()
    return true
end

function WindowFloorMapPreviewUI:rebuildTelepointList(entries)
    self._columns = math.max(1, #entries)
    self._rowControllers = {}
    self._listView:clearChildren()
    self:_applyListLayout()
    for _, entry in ipairs(entries) do
        local controller = CommandRowController.new({
            text = entry[2],
            callback = function ()
                self.model._owner:confirmSelectedTelepoint()
            end
        })
        local child = controller:prepare(sf.Vector2u.new(self.model._telepointItemWidth, _TELEPOINT_LIST_HEIGHT))
        self._rowControllers[#self._rowControllers + 1] = controller
        self.model:_applyItem(child)
        self._listView:addChild(child)
    end
    self.view:reflow(self._logicalSize)
end

function WindowFloorMapPreviewUI:refreshSelectedPreview()
    local telepoint = self:getSelectedTelepoint()
    local showMarker = self.model:getActive()
    local mapPath = tostring(self.model._mapKey or "")
    if self._resolvePreviewMapPath ~= nil and bool(mapPath) then
        mapPath = self._resolvePreviewMapPath(mapPath)
    end
    local currentKey = tuple { tostring(mapPath or ""), telepointKey(telepoint), showMarker }
    if currentKey == self.model._currentPreviewKey then
        return
    end
    self.model._currentPreviewKey = currentKey
    if not bool(self.model._mapKey) or not bool(telepoint) then
        self:hidePreview()
        return
    end
    local texture = self.model._previewTextureCache:get(currentKey)
    if texture == nil then
        texture = self._loadPreview(self.model._mapKey, telepoint, _PREVIEW_CONTENT_SIZE, _PREVIEW_SCALE, showMarker)
        if texture ~= nil then
            self.model._previewTextureCache[currentKey] = texture
        end
    end
    if texture == nil then
        self:hidePreview()
        return
    end
    texture:setSmooth(false)
    self._previewImage:setTexture(texture, true)
    self:setProperty("PreviewImage", "visible", true)
end

function WindowFloorMapPreviewUI:getSelectedTelepoint()
    local index = self.model.index
    if index == nil or index < 0 or index >= #self.model._telepoints then
        return nil
    end
    return self.model._telepoints[index + 1]
end

function WindowFloorMapPreviewUI:hidePreview()
    self.model._currentPreviewKey = nil
    self:setProperty("PreviewImage", "visible", false)
end

function WindowFloorMapPreviewUI:_applyListLayout()
    self:setProperty("TelepointList", "size", {
        self.model._telepointItemWidth * self._columns + 32,
        _TELEPOINT_LIST_HEIGHT
    })
    self:setProperty("TelepointList", "columns", self._columns)
end

function WindowFloorMapPreviewUI.getTelepointItemWidth(rect)
    return math.max(1, math.floor((rect.size.x - 64) / 2))
end

return Ui.define("Parts/WindowFloorTeleporter/WindowFloorMapPreview", WindowFloorMapPreviewUI)
