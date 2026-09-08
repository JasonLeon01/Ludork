local WindowCommand = require("Source.Windows.WindowCommand")
local WindowFloorMapCommandUI = require("Source.UI.Parts.WindowFloorTeleporter.WindowFloorMapCommand")
local WindowFloorMapCommandController = require("Source.Windows.WindowFloorTeleporter.Command.Controller")

local _LIST_ROW_HEIGHT = 32

---@class Source.Windows.WindowFloorMapCommand: Source.Windows.WindowCommand
local WindowFloorMapCommand = {}

WindowFloorMapCommand.controllerClass = WindowFloorMapCommandController

function WindowFloorMapCommand:init(rect, owner, instance)
    self._owner = owner
    self._ui = WindowFloorMapCommandUI.new(self, instance)
    super(WindowFloorMapCommand, self).init(rect, {}, nil, _LIST_ROW_HEIGHT, nil, nil, nil, { uiController = self._ui })
    self:setHasReturnBtn(true)
    ---@cast self._commandController Source.Windows.WindowFloorTeleporter.Command.Controller
    self._mapController = self._commandController
end

function WindowFloorMapCommand:refreshMaps(entries)
    self._mapController:refreshMaps(entries)
end

function WindowFloorMapCommand:getCurrentMapKey()
    return self._mapController:getCurrentMapKey()
end

function WindowFloorMapCommand:onTick(deltaTime)
    super(WindowFloorMapCommand, self).onTick(deltaTime)
    self._mapController:afterTick()
end

function WindowFloorMapCommand:onReturn()
    self._owner:closeByCancel()
end

function WindowFloorMapCommand:activateTelepointSelector()
    self._owner:activateTelepointSelector()
end

function WindowFloorMapCommand:notifyMapIndexMaybeChanged(index)
    self._owner:notifyMapIndexMaybeChanged(index)
end

return class(WindowFloorMapCommand, WindowCommand)
