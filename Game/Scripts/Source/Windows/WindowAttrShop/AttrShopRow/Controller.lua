local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowAttrShop.AttrShopRow")

local _DISABLED_COLOUR = sf.Color.new(160, 160, 160, 255)
local _ENABLED_COLOUR = sf.Color.new(255, 255, 255, 255)

---@class Source.Windows.WindowAttrShop.AttrShopRow.Controller
local AttrShopRowController = {}

function AttrShopRowController:bind()
    local label = self.ui.controls["Label"]
    ---@cast label Engine.PlainText
    self._label = label
end

function AttrShopRowController:refresh()
    self:setText("Label", self.model.text)
    self._label:setColour(self.model.available and _ENABLED_COLOUR or _DISABLED_COLOUR)
end

return Ui.Define(View, AttrShopRowController)
