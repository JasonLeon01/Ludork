local CommandRowController = require("Internal.UIBase.CommandRow.Controller")
local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Parts.Title.CommandWindow")
local WindowSelectable = require("Internal.UIBase.WindowSelectable")

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
    local list = self.ui.controls["CommandList"]
    local rows = {
        self.ui.assets["TitleCommand1"], self.ui.assets["TitleCommand2"], self.ui.assets["TitleCommand3"],
        self.ui.assets["TitleCommand4"]
    }
    local commandCount = #commands
    assert(commandCount == 3 or commandCount == #rows, "Title commands require three or four authored rows")
    for index, row in ipairs(rows) do
        if index <= commandCount then
            local controller = CommandRowController.new(assert(commands[index]), row)
            self._rows[#self._rows + 1] = controller
        else
            row.root:setVisible(false)
            list:removeChild(row.root)
        end
    end
    if commandCount < #rows then
        local removedHeight = (#rows - commandCount) * list:getDefaultItemSize().y
        local listSize = list:getSize()
        list:setSize(sf.Vector2f.new(listSize.x, listSize.y - removedHeight))
        local rootSize = self.ui.root:getSize()
        local windowSize = sf.Vector2u.new(math.ceil(rootSize.x), math.ceil(rootSize.y - removedHeight))
        self.host:resize(windowSize)
        self.host:setView(self.host:getDefaultView())
        self.ui:prepare(windowSize)
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
