local Ui = require("Source.UI.Ui")

---@class Source.UI.Parts.WindowEquip.WindowEquipSlot
local WindowEquipSlotUI = {}

function WindowEquipSlotUI:init(model, instance)
    super(WindowEquipSlotUI, self).init(model, instance)
    local logicalSize = sf.Vector2u.new(192, 160)
    ---@cast logicalSize sf.Vector2u
    self._logicalSize = logicalSize
end

function WindowEquipSlotUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._scrollBox = self:requireControl("SlotScrollBox")
    self._listView = self:requireControl("SlotList")
    ---@cast self._windowFrame Engine.Window
    ---@cast self._content Engine.Canvas
    ---@cast self._scrollBox Engine.ScrollBox
    ---@cast self._listView Engine.ListView
    self._listView:clearChildren()
end

function WindowEquipSlotUI:attach()
    self:attachNestedWindowView(self.model, self._logicalSize)
end

function WindowEquipSlotUI:getWindowFrame()
    return self._windowFrame
end

function WindowEquipSlotUI:getContent()
    return self._content
end

function WindowEquipSlotUI:getScrollBox()
    return self._scrollBox
end

function WindowEquipSlotUI:getListView()
    return self._listView
end

return Ui.Define("Parts/WindowEquip/WindowEquipSlot", WindowEquipSlotUI)
