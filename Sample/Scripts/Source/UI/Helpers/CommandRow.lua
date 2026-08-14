local Data = require("Source.Data")
local LocaleCore = require("Source.Locale.Core")
local UiControlFactory = require("Source.UI.UiControlFactory")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

---@class Source.UI.Helpers.CommandRow
local CommandRowController = {}

function CommandRowController:init(model)
    assert(
        (model.text ~= nil) ~= (model.localeKey ~= nil),
        "Command row requires exactly one of text or localeKey"
    )
    self.model = model
    local logicalSize = sf.Vector2u.new(1, 1)
    ---@cast logicalSize sf.Vector2u
    self.root, self._label = UiControlFactory.createFunctionalTextRow(
        logicalSize, Data.getPlainTextConfig("UI/Default")
    )
    self._bound = false
end

function CommandRowController:bind()
    if self.model.callback ~= nil then
        self.root:addConfirmCallback(self.model.callback)
    end
end

function CommandRowController:refresh()
    local text = self.model.text
    if self.model.localeKey ~= nil then
        text = LOC(self.model.localeKey)
    end
    self._label:setString(text)
    UiControlFactory.layoutCenteredTextRow(self.root, self._label, 0.0)
end

function CommandRowController:prepare(logicalSize)
    self.root:resize(logicalSize)
    if not self._bound then
        self:bind()
        self._bound = true
    end
    self:refresh()
    return self.root
end

return class(CommandRowController)
