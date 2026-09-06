local WindowCommandController = require("Source.Windows.WindowCommand.Controller")

---@class (partial) Source.Windows.WindowFloorTeleporter.Command.Controller
local WindowFloorMapCommandController = {}

function WindowFloorMapCommandController:init(model, size, rowHeight, columns)
    super(WindowFloorMapCommandController, self).init(model, size, rowHeight, columns)
    ---@cast self.model Source.Windows.WindowFloorMapCommand
    self._mapKeys = {}
    self.model._mapKeys = self._mapKeys
end

function WindowFloorMapCommandController:refreshMaps(entries)
    ---@cast self.model Source.Windows.WindowFloorMapCommand
    local previousMapKey = self:getCurrentMapKey()
    self._mapKeys = {}
    self.model._mapKeys = self._mapKeys
    self._rowControllers = {}
    self.root:clearChildren()
    for index, entry in ipairs(entries) do
        self._mapKeys[index] = entry[1]
        local child = self:createRow({
            text = entry[2],
            callback = function ()
                self.model._owner:activateTelepointSelector()
            end
        })
        self.model:_applyItem(child)
        self.root:addChild(child)
    end
    self:prepare()
    if not bool(self._mapKeys) then
        self.model.index = nil
    else
        local previousIndex = nil
        if previousMapKey ~= nil then
            local index = table.index(self._mapKeys, previousMapKey)
            if index ~= nil then
                previousIndex = index - 1
            end
        end
        self.model.index = previousIndex or 0
    end
    self.model._owner:notifyMapIndexMaybeChanged(self.model.index)
end

function WindowFloorMapCommandController:getCurrentMapKey()
    if self.model.index == nil or self.model.index >= #self._mapKeys then
        return nil
    end
    return self._mapKeys[self.model.index + 1]
end

function WindowFloorMapCommandController:afterTick()
    ---@cast self.model Source.Windows.WindowFloorMapCommand
    self.model._owner:notifyMapIndexMaybeChanged(self.model.index)
end

return class(WindowFloorMapCommandController, WindowCommandController)
