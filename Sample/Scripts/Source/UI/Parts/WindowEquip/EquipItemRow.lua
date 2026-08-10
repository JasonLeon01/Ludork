local Ui = require("Source.UI.Ui")

local EquipItemRowUI = {}

function EquipItemRowUI:refresh()
    if self.model.iconTexture == nil then
        self:setProperty("Icon", "visible", false)
    else
        local icon = self:requireControl("Icon")
        icon:setTexture(self.model.iconTexture, true)
        self:setProperty("Icon", "visible", true)
    end
    local showCount = self.model.count > 1
    self:setText("Count", showCount and tostring(self.model.count) or "")
    self:setProperty("Count", "visible", showCount)
end

return Ui.define("Parts/WindowEquip/EquipItemRow", EquipItemRowUI)
