local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowEquip.EquipSlotRow")

local EquipSlotRowController = {}

function EquipSlotRowController:refresh()
    if self.model.iconTexture == nil then
        self:setProperty("Icon", "visible", false)
    else
        self.ui.controls["Icon"]:setTexture(self.model.iconTexture, true)
        self:setProperty("Icon", "visible", true)
    end
    self:setText("Label", self.model.label)
end

return Ui.Define(View, EquipSlotRowController)
