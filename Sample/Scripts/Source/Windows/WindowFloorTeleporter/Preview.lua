local WindowFloorMapPreviewUI = require("Source.UI.Parts.WindowFloorTeleporter.WindowFloorMapPreview")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local _TELEPOINT_LIST_HEIGHT = 32
local WindowFloorMapPreview = {}

WindowFloorMapPreview.uiClass = WindowFloorMapPreviewUI

function WindowFloorMapPreview:init(rect, owner, loadPreview, resolvePreviewMapPath, instance)
    self._owner = owner
    self._telepointItemWidth = self.uiClass.GetTelepointItemWidth()
    super
        (WindowFloorMapPreview, self)
        .init(rect, nil, self._telepointItemWidth, _TELEPOINT_LIST_HEIGHT, nil, nil, nil, nil, true)
    self:setHasReturnBtn(true)
    self._previewUI = self.uiClass.new(self, rect.size, loadPreview, resolvePreviewMapPath, instance)
    self._previewUI:attach(instance ~= nil)
    self:setScrollBox(self._previewUI:getScrollBox())
    self:setListView(self._previewUI:getListView())
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
    local previousIndex = self.index ~= nil and self.index or nil
    super(WindowFloorMapPreview, self).onTick(deltaTime)
    self._previewUI:afterSelectionUpdate(previousIndex)
end

function WindowFloorMapPreview:onKeyDown(kwargs)
    local previousIndex = self.index ~= nil and self.index or nil
    super(WindowFloorMapPreview, self).onKeyDown(kwargs)
    self._previewUI:afterSelectionUpdate(previousIndex)
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

return class(WindowFloorMapPreview, WindowSelectable)
