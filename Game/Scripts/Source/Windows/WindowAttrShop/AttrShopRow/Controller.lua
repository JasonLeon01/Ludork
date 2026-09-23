local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Parts.WindowAttrShop.AttrShopRow")

local _DISABLED_COLOUR = sf.Color.new(160, 160, 160, 255)

---@class Source.Windows.WindowAttrShop.AttrShopRow.Controller
local AttrShopRowController = {}

function AttrShopRowController:bind()
    self._labelColour = self.ui.controls["Label"]:getColour():copy()
end

function AttrShopRowController:refresh()
    self:setText("Label", self.model.text)
    self.ui.controls["Label"]:setColour(self.model.available and self._labelColour or _DISABLED_COLOUR)
end

return Ui.Define(View, AttrShopRowController)
