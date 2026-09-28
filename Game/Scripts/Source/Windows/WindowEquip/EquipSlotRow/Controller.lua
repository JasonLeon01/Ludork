local Ui = require("Internal.UIBase.Ui")
local IconTexture = require("Internal.UIBase.IconTexture")
local View = require("Internal.UI.Parts.WindowEquip.EquipSlotRow")

---@class (partial) Source.Windows.WindowEquip.EquipSlotRow.Controller
local EquipSlotRowController = {}

function EquipSlotRowController:refresh()
    IconTexture.Apply(self, "Icon", self.model.iconTexture)
    self:setText("Label", self.model.label)
end

return Ui.Define(View, EquipSlotRowController)
