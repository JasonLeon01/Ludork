local Ui = require("Source.UI.Ui")

local _DISABLED_COLOUR = sf.Color.new(160, 160, 160, 255)
local _ENABLED_COLOUR = sf.Color.new(255, 255, 255, 255)

---@class Source.UI.Parts.WindowAttrShop.AttrShopRow
local AttrShopRowUI = {}

function AttrShopRowUI:bind()
    local label = self:requireControl("Label")
    ---@cast label Engine.PlainText
    self._label = label
end

function AttrShopRowUI:refresh()
    self:setText("Label", self.model.text)
    self._label:setColour(self.model.available and _ENABLED_COLOUR or _DISABLED_COLOUR)
end

return Ui.Define("Parts/WindowAttrShop/AttrShopRow", AttrShopRowUI)
