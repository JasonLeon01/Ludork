local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UI.Ui")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

---@class Source.UI.Parts.Shared.CommandRow
local CommandRowUI = {}

function CommandRowUI:init(model)
    assert((model.text ~= nil) ~= (model.localeKey ~= nil), "Command row requires exactly one of text or localeKey")
    super(CommandRowUI, self).init(model)
end

function CommandRowUI:bind()
    if self.model.callback ~= nil then
        ---@cast self.root Engine.Canvas
        self.root:addConfirmCallback(self.model.callback)
    end
end

function CommandRowUI:refresh()
    local text = self.model.localeKey ~= nil and LOC(self.model.localeKey) or self.model.text
    ---@cast text string
    self:setText("Label", text)
end

return Ui.Define("Parts/Shared/CommandRow", CommandRowUI)
