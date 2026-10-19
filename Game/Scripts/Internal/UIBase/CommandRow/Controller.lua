local LocaleCore = require("Source.Locale.Core")
local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Parts.Shared.CommandRow")

---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

---@class Internal.UIBase.CommandRow.Controller
local CommandRowController = {}

function CommandRowController:bind()
    assert(
        (self.model.text ~= nil) ~= (self.model.localeKey ~= nil),
        "Command row requires exactly one of text or localeKey"
    )
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

function CommandRowController:prepareForList(itemSize)
    self:prepare()
    local textWidth = self.ui.controls["Label"]:getLocalBounds().size.x
    local rowSize = sf.Vector2u.new(math.ceil(math.max(itemSize.x, textWidth)), math.floor(itemSize.y))
    ---@cast rowSize sf.Vector2u
    self:prepare(rowSize)
end

return Ui.Define(View, CommandRowController)
