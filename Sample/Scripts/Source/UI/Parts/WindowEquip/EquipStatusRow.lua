local Ui = require("Source.UI.Ui")

local EquipStatusRowUI = {}

function EquipStatusRowUI:refresh()
    self:setText("Label", self.model.label)
    self:setText("Delta", self.model.delta > 0 and "+" .. tostring(self.model.delta) or tostring(self.model.delta))
    self:setProperty("Delta", "colour", self.model.delta > 0 and { 0, 255, 0, 255 } or { 255, 0, 0, 255 })
end

return Ui.Define("Parts/WindowEquip/EquipStatusRow", EquipStatusRowUI)
