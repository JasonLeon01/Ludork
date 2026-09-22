local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowShop.WindowShopCell")

local _SHOP_DISABLED_ALPHA = 120
local _SHOP_DISABLED_TEXT_COLOUR = sf.Color.new(160, 160, 160, 255)

local WindowShopCellController = {}

function WindowShopCellController:bind()
    self._iconColour = self.ui.controls["Icon"]:getColour():copy()
    self._valueColour = self.ui.controls["ValueText"]:getColour():copy()
    if self.model.callback ~= nil then
        self.root:addConfirmCallback(function (obj, kwargs)
            self.model.callback(obj, kwargs)
        end)
    end
end

function WindowShopCellController:refresh()
    if self.model.iconTexture == nil then
        self:setProperty("Icon", "visible", false)
    else
        self.ui.controls["Icon"]:setTexture(self.model.iconTexture, true)
        self:setProperty("Icon", "visible", true)
        local colour = self._iconColour:copy()
        if not self.model.available then
            colour.a = _SHOP_DISABLED_ALPHA
        end
        self:setProperty("Icon", "colour", colour)
    end
    if self.model.showValue then
        self:setText("ValueText", tostring(self.model.value or 0))
        self:setProperty("ValueText", "visible", true)
        self:setProperty(
            "ValueText", "colour", self.model.available and self._valueColour or _SHOP_DISABLED_TEXT_COLOUR
        )
    else
        self:setText("ValueText", "")
        self:setProperty("ValueText", "visible", false)
    end
end

return Ui.Define(View, WindowShopCellController)
