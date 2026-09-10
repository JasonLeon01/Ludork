local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowEquip.EquipStatusRow")

local EquipStatusRowController = {}

function EquipStatusRowController:refresh()
    self:setText("Label", self.model.label)
    self:setText("Delta", self.model.delta > 0 and "+" .. tostring(self.model.delta) or tostring(self.model.delta))
    self:setProperty(
        "Delta", "colour", self.model.delta > 0 and sf.Color.new(0, 255, 0, 255) or sf.Color.new(255, 0, 0, 255)
    )
end

return Ui.Define(View, EquipStatusRowController)
