local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Parts.WindowEquip.EquipItemRow")

local EquipItemRowController = {}

function EquipItemRowController:refresh()
    if self.model.iconTexture == nil then
        self:setProperty("Icon", "visible", false)
    else
        self.ui.controls["Icon"]:setTexture(self.model.iconTexture, true)
        self:setProperty("Icon", "visible", true)
    end
    local showCount = self.model.count > 1
    self:setText("Count", showCount and tostring(self.model.count) or "")
    self:setProperty("Count", "visible", showCount)
end

return Ui.Define(View, EquipItemRowController)
