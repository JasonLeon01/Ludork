local Ui = require("Source.UI.Ui")

---@class Source.UI.Parts.WindowMessage.MessageOptionRow
local MessageOptionRowUI = {}

function MessageOptionRowUI:bind()
    ---@cast self.root Engine.FunctionalPlainText
    if self.model.onConfirm ~= nil then
        self.root:addConfirmCallback(self.model.onConfirm)
    end
    if self.model.onCancel ~= nil then
        self.root:addCancelCallback(self.model.onCancel)
    end
end

function MessageOptionRowUI:refresh()
    self:setText("Root", self.model.text)
end

return Ui.Define("Parts/WindowMessage/MessageOptionRow", MessageOptionRowUI)
