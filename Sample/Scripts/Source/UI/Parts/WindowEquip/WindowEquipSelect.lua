local Ui = require("Source.UI.Ui")

---@class Source.UI.Parts.WindowEquip.WindowEquipSelect
local WindowEquipSelectUI = {}

function WindowEquipSelectUI:init(model, instance)
    super(WindowEquipSelectUI, self).init(model, instance)
    local logicalSize = sf.Vector2u.new(192, 192)
    ---@cast logicalSize sf.Vector2u
    self._logicalSize = logicalSize
end

function WindowEquipSelectUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._scrollBox = self:requireControl("SelectScrollBox")
    self._listView = self:requireControl("SelectList")
    ---@cast self._windowFrame Engine.Window
    ---@cast self._content Engine.Canvas
    ---@cast self._scrollBox Engine.ScrollBox
    ---@cast self._listView Engine.ListView
    self._listView:clearChildren()
end

function WindowEquipSelectUI:attach()
    self:attachNestedWindowView(self.model, self._logicalSize)
end

function WindowEquipSelectUI:getWindowFrame()
    return self._windowFrame
end

function WindowEquipSelectUI:getContent()
    return self._content
end

function WindowEquipSelectUI:getScrollBox()
    return self._scrollBox
end

function WindowEquipSelectUI:getListView()
    return self._listView
end

return Ui.Define("Parts/WindowEquip/WindowEquipSelect", WindowEquipSelectUI)
