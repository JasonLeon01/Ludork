local ListViewController = require("Source.UI.Helpers.ListView")
local CommandRowUI = require("Source.UI.Parts.Shared.CommandRow")

---@class (partial) Source.Windows.WindowCommand.Controller
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
    self.model:applyItem(child)
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

return class(WindowCommandController, ListViewController)
