local WindowCommand = require("Source.Windows.WindowCommand")
local WindowSaveCommandController = {}

function WindowSaveCommandController.CreateCommands(owner)
    return {
        {
            key = "Load",
            localeKey = "MENU_LOAD",
            callback = function (_obj, _kwargs)
                owner:onCommandConfirm("load")
            end
        },
        {
            key = "Save",
            localeKey = "MENU_SAVE",
            callback = function (_obj, _kwargs)
                owner:onCommandConfirm("save")
            end
        }
    }
end

local FinalWindowSaveCommandController = class(WindowSaveCommandController, WindowCommand.Controller)

local WindowSaveCommand = {}

WindowSaveCommand.controllerClass = FinalWindowSaveCommandController

function WindowSaveCommand:init(rect, owner)
    local commands = FinalWindowSaveCommandController.CreateCommands(owner)
    super(WindowSaveCommand, self).init(rect, commands, nil, 32, nil, nil, 2)
    self:setHasReturnBtn(true)
    self._owner = owner
end

function WindowSaveCommand:onReturn()
    self._owner:closeByCancel()
end

return class(WindowSaveCommand, WindowCommand)
