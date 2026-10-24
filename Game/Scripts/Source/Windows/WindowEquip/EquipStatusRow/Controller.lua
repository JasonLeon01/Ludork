local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Parts.WindowEquip.EquipStatusRow")

local EquipStatusRowController = {}

function EquipStatusRowController:refresh()
    self:setText("Label", self.model.label)
    self:setText("Delta", self.model.delta > 0 and "+" .. tostring(self.model.delta) or tostring(self.model.delta))
    self:setProperty("Delta", "colour", self.model.delta > 0 and sf.Color.Green or sf.Color.Red)
end

return Ui.Define(View, EquipStatusRowController)
