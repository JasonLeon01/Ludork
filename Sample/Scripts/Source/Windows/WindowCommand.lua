local CommandRowController = require("Source.UIBase.CommandRow.Controller")
local UiCollection = require("Source.UIBase.UiCollection")
local UiControlFactory = require("Source.UIBase.UiControlFactory")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

---@class Source.Windows.WindowCommand
local WindowCommand = {}

function WindowCommand:init(rect, commands, rectWidth, rectHeight, windowSkin, repeated, columns, externalView)
    commands = commands or {}
    rectHeight = rectHeight or 32
    columns = columns or 1
    if externalView == nil then
        super(WindowCommand, self).init(rect, nil, rectWidth, rectHeight, windowSkin, repeated)
    else
        super(WindowCommand, self).init(rect, nil, rectWidth, rectHeight, windowSkin, repeated, nil, nil, true)
        self._window = externalView.windowFrame
        self.content = externalView.content
        self:setScrollBox(externalView.scrollBox)
        self:setListView(externalView.listView)
    end
    local size = self.content:getSize()
    local logicalSize = sf.Vector2u.new(math.max(1, math.floor(size.x)), math.max(1, math.floor(size.y)))
    local rowSize = sf.Vector2u.new(math.max(1, math.floor((size.x - 32) / columns)), rectHeight)
    ---@cast logicalSize sf.Vector2u
    ---@cast rowSize sf.Vector2u
    local listView = externalView ~= nil and externalView.listView
        or UiControlFactory.CreateListView(logicalSize, rectHeight, true, columns)
    local listSize = sf.Vector2i.new(logicalSize.x, logicalSize.y)
    ---@cast listSize sf.Vector2i
    listView:setSize(listSize)
    listView:setColumns(columns)
    self._rows = UiCollection.new(listView, CommandRowController)
    for _, item in ipairs(commands) do
        local controller = self._rows:add(item, rowSize)
        self:applyItem(controller.ui.root)
    end
    self._rows:layout()
    self:setListView(listView)
end

function WindowCommand:refreshRows()
    for _, controller in ipairs(self._rows.items) do
        controller:refresh()
    end
end

function WindowCommand:dispose()
    self._rows:dispose()
    super(WindowCommand, self).dispose()
end

return class(WindowCommand, WindowSelectable)
