local CommandRowController = require("Source.UIBase.CommandRow.Controller")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowFloorTeleporter.WindowFloorMapCommand")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local _LIST_ROW_HEIGHT = 32

---@class Source.Windows.WindowFloorMapCommand.Controller
local Controller = {}

Controller.windowOptions = {
    returnButton = true,
    hidden = true,
    list = "CommandList",
    scroll = "CommandScrollBox",
    itemHeight = _LIST_ROW_HEIGHT
}

function Controller:init(owner)
    self._owner = owner
    self._mapKeys = {}
    self._commands = self:createCollection(self.ui.controls["CommandList"], CommandRowController)
end

function Controller:refreshMaps(entries)
    local previousMapKey = self:getCurrentMapKey()
    self._mapKeys = {}
    self._commands:clear()
    local rowSize = sf.Vector2u.new(math.max(1, math.floor(self.ui.controls["Content"]:getSize().x - 32)), 32)
    ---@cast rowSize sf.Vector2u
    for index, entry in ipairs(entries) do
        self._mapKeys[index] = entry[1]
        local row = self._commands:add({
            text = entry[2],
            callback = self:bindCallback(Controller.activateTelepointSelector)
        }, rowSize)
        self.host:applyItem(row.ui.root)
    end
    self._commands:layout()
    if not bool(self._mapKeys) then
        self.host.index = nil
    else
        local previousIndex = nil
        if previousMapKey ~= nil then
            local index = table.index(self._mapKeys, previousMapKey)
            if index ~= nil then
                previousIndex = index - 1
            end
        end
        self.host.index = previousIndex or 0
    end
    self:notifyMapIndexMaybeChanged(self.host.index)
end

function Controller:getCurrentMapKey()
    if self.host.index == nil or self.host.index >= #self._mapKeys then
        return nil
    end
    return self._mapKeys[self.host.index + 1]
end

function Controller:onTick(deltaTime)
    WindowSelectable.onTick(self.host, deltaTime)
    self:afterTick()
end

function Controller:onReturn()
    self._owner:closeByCancel()
end

function Controller:activateTelepointSelector()
    self._owner:activateTelepointSelector()
end

function Controller:notifyMapIndexMaybeChanged(index)
    self._owner:notifyMapIndexMaybeChanged(index)
end

function Controller:refreshRows()
    for _, row in ipairs(self._commands.items) do
        row:prepare()
    end
end

function Controller:afterTick()
    self:notifyMapIndexMaybeChanged(self.host.index)
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
