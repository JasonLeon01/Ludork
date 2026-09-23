local LocaleCore = require("Source.Locale.Core")
local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Parts.Shared.CommandRow")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

---@class Internal.UIBase.CommandRow.Controller
local CommandRowController = {}

function CommandRowController:init(model, ui)
    assert((model.text ~= nil) ~= (model.localeKey ~= nil), "Command row requires exactly one of text or localeKey")
    super(CommandRowController, self).init(model, ui)
end

function CommandRowController:bind()
    if self.model.callback ~= nil then
        ---@cast self.root Engine.Canvas
        self.root:addConfirmCallback(self.model.callback)
    end
end

function CommandRowController:refresh()
    local text = self.model.localeKey ~= nil and LOC(self.model.localeKey) or self.model.text
    ---@cast text string
    self:setText("Label", text)
end

return Ui.Define(View, CommandRowController)
