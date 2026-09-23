local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Parts.WindowItem.ItemRow")

local _UNUSABLE_ICON_ALPHA = 160

local ItemRowController = {}

function ItemRowController:bind()
    self._iconColour = self.ui.controls["Icon"]:getColour():copy()
end

function ItemRowController:refresh()
    if self.model.iconTexture == nil then
        self:setProperty("Icon", "visible", false)
    else
        self.ui.controls["Icon"]:setTexture(self.model.iconTexture, true)
        self:setProperty("Icon", "visible", true)
        local colour = self._iconColour:copy()
        if not self.model.usable then
            colour.a = _UNUSABLE_ICON_ALPHA
        end
        self:setProperty("Icon", "colour", colour)
    end
    self:setText("Count", self.model.cost and tostring(self.model.count) or "")
    self:setProperty("Count", "visible", self.model.cost)
end

return Ui.Define(View, ItemRowController)
