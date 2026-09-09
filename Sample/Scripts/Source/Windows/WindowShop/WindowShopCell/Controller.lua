local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowShop.WindowShopCell")

local _SHOP_DISABLED_ALPHA = 120
local _SHOP_DISABLED_TEXT_COLOUR = sf.Color.new(160, 160, 160, 255)

local WindowShopCellController = {}

function WindowShopCellController:bind()
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
        self:setProperty(
            "Icon", "colour", sf.Color.new(255, 255, 255, self.model.available and 255 or _SHOP_DISABLED_ALPHA)
        )
    end
    if self.model.showValue then
        self:setText("ValueText", tostring(self.model.value or 0))
        self:setProperty("ValueText", "visible", true)
        self:setProperty(
            "ValueText", "colour",
            self.model.available and sf.Color.new(255, 255, 255, 255) or _SHOP_DISABLED_TEXT_COLOUR
        )
    else
        self:setText("ValueText", "")
        self:setProperty("ValueText", "visible", false)
    end
end

return Ui.Define(View, WindowShopCellController)
