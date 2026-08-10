local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local ListViewController = require("Source.UI.Helpers.ListView")
local CommandRowController = require("Source.UI.Helpers.CommandRow")

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
    local controller = CommandRowController.new(item)
    local root = controller:prepare(sf.Vector2u.new(self._rowWidth, self._rowHeight))
    self._rowControllers[#self._rowControllers + 1] = controller
    return root
end

local FinalWindowCommandController = class(WindowCommandController, ListViewController)

---@class Source.Windows.WindowCommand
local WindowCommand = {}

WindowCommand.Controller = FinalWindowCommandController
WindowCommand.controllerClass = FinalWindowCommandController

function WindowCommand:init(rect, commands, rectWidth, rectHeight, windowSkin, repeated, columns)
    commands = commands or {}
    rectHeight = rectHeight or 32
    columns = columns or 1
    super(WindowCommand, self).init(rect, nil, rectWidth, rectHeight, windowSkin, repeated)
    self._commandController = self.controllerClass.new(self, self.content:getSize(), rectHeight, columns)
    self._commandController:attach(commands)
end

function WindowCommand:refreshRows()
    self._commandController:refreshRows()
end

return class(WindowCommand, WindowSelectable)
