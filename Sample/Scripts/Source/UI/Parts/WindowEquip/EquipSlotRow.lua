local Ui = require("Source.UI.Ui")

local EquipSlotRowUI = {}

function EquipSlotRowUI:refresh()
    if self.model.iconTexture == nil then
        self:setProperty("Icon", "visible", false)
    else
        local icon = self:requireControl("Icon")
        icon:setTexture(self.model.iconTexture, true)
        self:setProperty("Icon", "visible", true)
    end
    self:setText("Label", self.model.label)
end

return Ui.define("Parts/WindowEquip/EquipSlotRow", EquipSlotRowUI)
