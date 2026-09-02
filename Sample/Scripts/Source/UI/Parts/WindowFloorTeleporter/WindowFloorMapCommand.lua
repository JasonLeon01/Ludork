local Ui = require("Source.UI.Ui")

---@class Source.UI.Parts.WindowFloorTeleporter.WindowFloorMapCommand
local WindowFloorMapCommandUI = {}

function WindowFloorMapCommandUI:init(model, instance)
    super(WindowFloorMapCommandUI, self).init(model, instance)
    local logicalSize = sf.Vector2u.new(176, 240)
    ---@cast logicalSize sf.Vector2u
    self._logicalSize = logicalSize
end

function WindowFloorMapCommandUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._scrollBox = self:requireControl("CommandScrollBox")
    self._listView = self:requireControl("CommandList")
    ---@cast self._windowFrame Engine.Window
    ---@cast self._content Engine.Canvas
    ---@cast self._scrollBox Engine.ScrollBox
    ---@cast self._listView Engine.ListView
    self._listView:clearChildren()
end

function WindowFloorMapCommandUI:attach()
    self:attachNestedWindowView(self.model, self._logicalSize)
end

function WindowFloorMapCommandUI:getWindowFrame()
    return self._windowFrame
end

function WindowFloorMapCommandUI:getContent()
    return self._content
end

function WindowFloorMapCommandUI:getScrollBox()
    return self._scrollBox
end

function WindowFloorMapCommandUI:getListView()
    return self._listView
end

return Ui.Define("Parts/WindowFloorTeleporter/WindowFloorMapCommand", WindowFloorMapCommandUI)
