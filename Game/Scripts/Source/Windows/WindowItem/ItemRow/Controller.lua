local Ui = require("Internal.UIBase.Ui")
local IconTexture = require("Internal.UIBase.IconTexture")
local View = require("Internal.UI.Parts.WindowItem.ItemRow")

local _UNUSABLE_ICON_ALPHA = 160

---@class (partial) Source.Windows.WindowItem.ItemRow.Controller
local ItemRowController = {}

function ItemRowController:bind()
    self._iconColour = self.ui.controls["Icon"]:getColour():copy()
end

function ItemRowController:refresh()
    if IconTexture.Apply(self, "Icon", self.model.iconTexture) then
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
