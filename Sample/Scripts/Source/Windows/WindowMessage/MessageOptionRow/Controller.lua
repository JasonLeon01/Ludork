local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowMessage.MessageOptionRow")

---@class Source.Windows.WindowMessage.MessageOptionRow.Controller
local MessageOptionRowController = {}

function MessageOptionRowController:bind()
    ---@cast self.root Engine.FunctionalPlainText
    if self.model.onConfirm ~= nil then
        self.root:addConfirmCallback(self.model.onConfirm)
    end
    if self.model.onCancel ~= nil then
        self.root:addCancelCallback(self.model.onCancel)
    end
end

function MessageOptionRowController:refresh()
    self:setText("Root", self.model.text)
end

return Ui.Define(View, MessageOptionRowController)
