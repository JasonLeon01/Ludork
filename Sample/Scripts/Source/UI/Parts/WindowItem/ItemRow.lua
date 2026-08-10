local Ui = require("Source.UI.Ui")

local _UNUSABLE_ICON_ALPHA = 160

local ItemRowUI = {}

function ItemRowUI:refresh()
    if self.model.iconTexture == nil then
        self:setProperty("Icon", "visible", false)
    else
        local icon = self:requireControl("Icon")
        icon:setTexture(self.model.iconTexture, true)
        self:setProperty("Icon", "visible", true)
        self:setProperty("Icon", "colour", {
            255,
            255,
            255,
            self.model.usable and 255 or _UNUSABLE_ICON_ALPHA
        })
    end
    self:setText("Count", self.model.cost and tostring(self.model.count) or "")
    self:setProperty("Count", "visible", self.model.cost)
end

return Ui.define("Parts/WindowItem/ItemRow", ItemRowUI)
