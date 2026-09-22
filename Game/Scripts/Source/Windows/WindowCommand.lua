local CommandRowController = require("Source.UIBase.CommandRow.Controller")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.Title.CommandWindow")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

---@class Source.Windows.WindowCommand.Controller
local Controller = {}

Controller.windowOptions = {
    frame = "CommandWindowFrame",
    content = "CommandContent",
    list = "CommandList",
    scroll = "CommandScrollBox"
}

function Controller:init(commands)
    self._rows = {}
    local rows = {
        self.ui.assets["TitleCommand1"], self.ui.assets["TitleCommand2"], self.ui.assets["TitleCommand3"],
        self.ui.assets["TitleCommand4"]
    }
    assert(#commands == #rows, "Title commands must match the authored rows")
    for index, row in ipairs(rows) do
        local controller = CommandRowController.new(assert(commands[index]), row)
        self._rows[#self._rows + 1] = controller
    end
end

function Controller:refresh()
    self:refreshRows()
end

function Controller:refreshRows()
    for _, controller in ipairs(self._rows) do
        controller:prepare()
    end
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
