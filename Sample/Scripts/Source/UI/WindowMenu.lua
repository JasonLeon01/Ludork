local Ui = require("Source.UI.Ui")

---@class Source.UI.WindowMenu
local WindowMenuUI = {}

function WindowMenuUI:init(model)
    super(WindowMenuUI, self).init(model)
    self._logicalSize = sf.Vector2u.new(192, 192)
end

function WindowMenuUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._scrollBox = self:requireControl("MenuScrollBox")
    self._listView = self:requireControl("MenuList")
    ---@cast self._windowFrame Engine.Window
    ---@cast self._content Engine.Canvas
    ---@cast self._scrollBox Engine.ScrollBox
    ---@cast self._listView Engine.ListView
    self._listView:clearChildren()
end

function WindowMenuUI:attach()
    local logicalSize = self._logicalSize
    ---@cast logicalSize sf.Vector2u
    self:attachWindowView(self.model, logicalSize)
end

function WindowMenuUI:getWindowFrame()
    return self._windowFrame
end

function WindowMenuUI:getContent()
    return self._content
end

function WindowMenuUI:getScrollBox()
    return self._scrollBox
end

function WindowMenuUI:getListView()
    return self._listView
end

return Ui.Define("WindowMenu", WindowMenuUI)
