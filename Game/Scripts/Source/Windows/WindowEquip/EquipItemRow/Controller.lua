local Ui = require("Internal.UIBase.Ui")
local IconTexture = require("Internal.UIBase.IconTexture")
local View = require("Internal.UI.Parts.WindowEquip.EquipItemRow")

---@class (partial) Source.Windows.WindowEquip.EquipItemRow.Controller
local EquipItemRowController = {}

function EquipItemRowController:refresh()
    IconTexture.Apply(self, "Icon", self.model.iconTexture)
    local showCount = self.model.count > 1
    self:setText("Count", showCount and tostring(self.model.count) or "")
    self:setProperty("Count", "visible", showCount)
end

return Ui.Define(View, EquipItemRowController)
