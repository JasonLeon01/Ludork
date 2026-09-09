local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowItem.ItemRow")

local _UNUSABLE_ICON_ALPHA = 160

local ItemRowController = {}

function ItemRowController:refresh()
    if self.model.iconTexture == nil then
        self:setProperty("Icon", "visible", false)
    else
        self.ui.controls["Icon"]:setTexture(self.model.iconTexture, true)
        self:setProperty("Icon", "visible", true)
        self:setProperty(
            "Icon", "colour", sf.Color.new(255, 255, 255, self.model.usable and 255 or _UNUSABLE_ICON_ALPHA)
        )
    end
    self:setText("Count", self.model.cost and tostring(self.model.count) or "")
    self:setProperty("Count", "visible", self.model.cost)
end

return Ui.Define(View, ItemRowController)
