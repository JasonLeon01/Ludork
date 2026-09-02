local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local ListViewController = require("Source.UI.Helpers.ListView")
local CommandRowUI = require("Source.UI.Parts.Shared.CommandRow")

---@class Source.Windows.WindowCommand.Controller
local WindowCommandController = {}

function WindowCommandController:init(model, size, rowHeight, columns)
    self._size = size
    self._rowHeight = rowHeight
    self._columns = columns
    self._rowWidth = math.max(1, math.floor((self._size.x - 32) / self._columns))
    self._rowControllers = {}
    super(WindowCommandController, self).init(model, sf.Vector2u.new(
        math.max(1, math.floor(self._size.x)), math.max(1, math.floor(self._size.y))
    ), self._rowHeight, true, self._columns)
end

function WindowCommandController:refresh()
    self.root:setColumns(self._columns)
end

function WindowCommandController:prepare()
    return super(WindowCommandController, self).prepare(sf.Vector2u.new(
        math.max(1, math.floor(self._size.x)), math.max(1, math.floor(self._size.y))
    ))
end

function WindowCommandController:attach(commands)
    local listView = self:prepare()
    if bool(commands) then
        for _, item in ipairs(commands) do
            self:addRow(item)
        end
    else
        for _, item in pairs(commands) do
            self:addRow(item)
        end
    end
    self.model:setListView(listView)
end

function WindowCommandController:attachTo(listView, commands)
    self.root = listView
    self.root:clearChildren()
    self:attach(commands)
end

function WindowCommandController:addRow(item)
    local child = self:createRow(item)
    self.model:_applyItem(child)
    self.root:addChild(child)
end

function WindowCommandController:refreshRows()
    for _, controller in ipairs(self._rowControllers) do
        controller:refresh()
    end
end

function WindowCommandController:createRow(item)
    local controller = CommandRowUI.new(item)
    local logicalSize = sf.Vector2u.new(self._rowWidth, self._rowHeight)
    ---@cast logicalSize sf.Vector2u
    local root = controller:prepare(logicalSize)
    self._rowControllers[#self._rowControllers + 1] = controller
    return root
end

local FinalWindowCommandController = class(WindowCommandController, ListViewController)

---@class Source.Windows.WindowCommand
local WindowCommand = {}

WindowCommand.Controller = FinalWindowCommandController
WindowCommand.controllerClass = FinalWindowCommandController

function WindowCommand:init(rect, commands, rectWidth, rectHeight, windowSkin, repeated, columns, externalView)
    commands = commands or {}
    rectHeight = rectHeight or 32
    columns = columns or 1
    if externalView == nil then
        super(WindowCommand, self).init(rect, nil, rectWidth, rectHeight, windowSkin, repeated)
    else
        super(WindowCommand, self).init(rect, nil, rectWidth, rectHeight, windowSkin, repeated, nil, nil, true)
        if externalView.uiController ~= nil then
            externalView.uiController:attach()
            self:setScrollBox(externalView.uiController:getScrollBox())
            self:setListView(externalView.uiController:getListView())
        else
            self._window = externalView.windowFrame
            self.content = externalView.content
            self:setScrollBox(externalView.scrollBox)
            self:setListView(externalView.listView)
        end
    end
    self._commandController = self.controllerClass.new(self, self.content:getSize(), rectHeight, columns)
    if externalView == nil then
        self._commandController:attach(commands)
    else
        local listView = assert(self:getListView(), "External command ListView is unavailable")
        self._commandController:attachTo(listView, commands)
    end
end

function WindowCommand:refreshRows()
    self._commandController:refreshRows()
end

return class(WindowCommand, WindowSelectable)
