local Ui = require("Source.UI.Ui")

local _SHOP_DISABLED_ALPHA = 120
local _SHOP_DISABLED_TEXT_COLOUR = { 160, 160, 160, 255 }

local WindowShopCellUI = {}

function WindowShopCellUI:bind()
    self._icon = self:requireControl("Icon")
    if self.model.callback ~= nil then
        self.root:addConfirmCallback(function (obj, kwargs)
            self.model.callback(obj, kwargs)
        end)
    end
end

function WindowShopCellUI:refresh()
    if self.model.iconTexture == nil then
        self:setProperty("Icon", "visible", false)
    else
        self._icon:setTexture(self.model.iconTexture, true)
        self:setProperty("Icon", "visible", true)
        self:setProperty("Icon", "colour", {
            255,
            255,
            255,
            self.model.available and 255 or _SHOP_DISABLED_ALPHA
        })
    end
    if self.model.showValue then
        self:setText("ValueText", tostring(self.model.value or 0))
        self:setProperty("ValueText", "visible", true)
        self:setProperty(
            "ValueText", "colour", self.model.available and { 255, 255, 255, 255 } or _SHOP_DISABLED_TEXT_COLOUR
        )
    else
        self:setText("ValueText", "")
        self:setProperty("ValueText", "visible", false)
    end
end

return Ui.Define("Parts/WindowShop/WindowShopCell", WindowShopCellUI)
