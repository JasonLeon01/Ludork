local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Parts.Shared.SelectableTextRow")

---@class Source.Windows.Shared.SelectableTextRow.Controller
local SelectableTextRowController = {}

function SelectableTextRowController:bind()
    ---@cast self.root Engine.FunctionalPlainText
    if self.model.onConfirm ~= nil then
        self.root:addConfirmCallback(self.model.onConfirm)
    end
    if self.model.onCancel ~= nil then
        self.root:addCancelCallback(self.model.onCancel)
    end
end

function SelectableTextRowController:refresh()
    self:setText("Root", self.model.text)
end

return Ui.Define(View, SelectableTextRowController)
