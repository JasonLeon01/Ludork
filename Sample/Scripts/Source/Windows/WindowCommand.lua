local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local WindowCommandController = require("Source.Windows.WindowCommand.Controller")

---@class Source.Windows.WindowCommand
local WindowCommand = {}

WindowCommand.controllerClass = WindowCommandController

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
