local Data = require("Source.Data")
local UiControlFactory = require("Source.UI.UiControlFactory")

---@class Source.UI.Helpers.CommandRow
local CommandRowController = {}

function CommandRowController:init(model)
    self.model = model
    self.root, self._label = UiControlFactory.createFunctionalTextRow(
        sf.Vector2u.new(1, 1), Data.getPlainTextConfig("UI/Default")
    )
    self._bound = false
end

function CommandRowController:bind()
    if self.model.callback ~= nil then
        self.root:addConfirmCallback(self.model.callback)
    end
end

function CommandRowController:refresh()
    self._label:setString(self.model.text)
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
