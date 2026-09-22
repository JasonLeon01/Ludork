local TelepointKey = require("Source.UIBase.Helpers.TelepointKey")
local CommandRowController = require("Source.UIBase.CommandRow.Controller")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowFloorTeleporter.WindowFloorMapPreview")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local _PREVIEW_SCALE = 0.5

---@class Source.Windows.WindowFloorMapPreview.Controller
local Controller = {}

Controller.windowOptions = {
    returnButton = true,
    hidden = true,
    frame = "TelepointWindowFrame",
    content = "TelepointContent",
    list = "TelepointList",
    scroll = "TelepointScrollBox"
}

function Controller:init(owner, loadPreview, resolvePreviewMapPath)
    self._owner = owner
    self._loadPreview = loadPreview
    self._resolvePreviewMapPath = resolvePreviewMapPath
    self._mapKey = nil
    self._telepoints = {}
    self._currentListKey = nil
    self._currentPreviewKey = nil
    self._previewTextureCache = dict()
    self._rows = self:createCollection(self.ui.controls["TelepointList"], CommandRowController)
end

function Controller:clearPreviewCache()
    self._previewTextureCache = dict()
    self:hidePreview()
end

function Controller:setActive(active)
    local wasActive = self.host:getActive()
    WindowSelectable.setActive(self.host, active)
    self:onActiveChanged(active, wasActive)
end

function Controller:setMapKeyAndTelepoints(mapKey, entries, selectedIndex)
    local listKey = tuple { tostring(mapKey or ""), TelepointKey.FromEntries(entries) }
    if listKey ~= self._currentListKey then
        self._currentListKey = listKey
        self._mapKey = mapKey
        self._telepoints = {}
        for index, entry in ipairs(entries) do
            self._telepoints[index] = entry[1]
        end
        self:rebuildTelepointList(entries)
    end
    if not bool(entries) then
        self.host.index = nil
        self:hidePreview()
        return
    end
    self.host.index = math.trunc(math.clamp(selectedIndex, 0, #entries - 1))
    self:refreshSelectedPreview()
end

function Controller:onTick(deltaTime)
    local previousIndex = self.host.index ~= nil and self.host.index or nil
    WindowSelectable.onTick(self.host, deltaTime)
    self:afterSelectionUpdate(previousIndex)
    self:refreshSelectedPreview()
end

function Controller:onKeyDown(kwargs)
    local previousIndex = self.host.index ~= nil and self.host.index or nil
    WindowSelectable.onKeyDown(self.host, kwargs)
    self:afterSelectionUpdate(previousIndex)
end

function Controller:onReturn()
    self._owner:activateMapList(true)
end

function Controller:_setPointerIndex(index)
    local previousIndex = self.host.index
    self.host:setPointerIndex(index)
    self:afterSelectionUpdate(previousIndex)
end

function Controller:confirmSelectedTelepoint()
    self._owner:confirmSelectedTelepoint()
end

function Controller:notifyTelepointIndexMaybeChanged(index)
    self._owner:notifyTelepointIndexMaybeChanged(index)
end

function Controller:refresh()
    self:setProperty("PreviewImage", "visible", false)
end

function Controller:onActiveChanged(active, wasActive)
    self.ui.controls["TelepointWindowFrame"]:setVisible(active)
    self.ui.controls["TelepointContent"]:setVisible(active)
    if not active then
        self.host:hideSelectionCursor()
    end
    if active ~= wasActive then
        self:refreshSelectedPreview()
    end
end

function Controller:afterSelectionUpdate(previousIndex)
    if not self.host:getActive() then
        self.host:hideSelectionCursor()
    end
    if self.host.index == previousIndex then
        return
    end
    self:notifyTelepointIndexMaybeChanged(self.host.index)
    self:refreshSelectedPreview()
end

function Controller:rebuildTelepointList(entries)
    self._rows:clear()
    local itemSize = self.ui.controls["TelepointList"]:getDefaultItemSize()
    local logicalSize = sf.Vector2u.new(math.floor(itemSize.x), math.floor(itemSize.y))
    ---@cast logicalSize sf.Vector2u
    for _, entry in ipairs(entries) do
        self._rows:add({
            text = entry[2],
            callback = self:bindCallback(Controller.confirmSelectedTelepoint)
        }, logicalSize)
    end
    self._rows:layout()
    self.ui:prepare()
end

function Controller:refreshSelectedPreview()
    local telepoint = self:getSelectedTelepoint()
    local showMarker = self.host:getActive()
    local mapPath = tostring(self._mapKey or "")
    local visibilityRevision = 0
    if self._resolvePreviewMapPath ~= nil and bool(mapPath) then
        mapPath, visibilityRevision = self._resolvePreviewMapPath(mapPath)
    end
    local currentKey = tuple {
        tostring(mapPath or ""), TelepointKey.FromPoint(telepoint), showMarker, visibilityRevision
    }
    if currentKey == self._currentPreviewKey then
        return
    end
    self._currentPreviewKey = currentKey
    if not bool(self._mapKey) or not bool(telepoint) then
        self:hidePreview()
        return
    end
    local texture = self._previewTextureCache:get(currentKey)
    if texture == nil then
        local previewSize = self.ui.controls["PreviewContent"]:getSize()
        texture = self._loadPreview(
            self._mapKey, telepoint, math.min(previewSize.x, previewSize.y), _PREVIEW_SCALE, showMarker
        )
        if texture ~= nil then
            self._previewTextureCache[currentKey] = texture
        end
    end
    if texture == nil then
        self:hidePreview()
        return
    end
    texture:setSmooth(false)
    self.ui.controls["PreviewImage"]:setTexture(texture, true)
    self:setProperty("PreviewImage", "visible", true)
    self.view:reflow()
end

function Controller:getSelectedTelepoint()
    if self.host.index == nil or self.host.index < 0 or self.host.index >= #self._telepoints then
        return nil
    end
    return self._telepoints[self.host.index + 1]
end

function Controller:hidePreview()
    self._currentPreviewKey = nil
    self:setProperty("PreviewImage", "visible", false)
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
