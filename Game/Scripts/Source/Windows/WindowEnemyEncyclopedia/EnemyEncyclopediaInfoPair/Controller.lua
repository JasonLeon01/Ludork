local Engine = require("Engine")
local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair")

local TextLayout = Engine.TextLayout

local EnemyEncyclopediaInfoPairController = {}

function EnemyEncyclopediaInfoPairController:refresh()
    local label = self.ui.controls["Label"]
    local value = self.ui.controls["Value"]
    ---@cast label Engine.PlainText
    ---@cast value Engine.PlainText
    self:setText("Label", TextLayout.fitPlainText(self.model.label, self.ui.controls["LabelArea"]:getSize().x, label))
    self:setText("Value", TextLayout.fitPlainText(self.model.value, self.ui.controls["ValueArea"]:getSize().x, value))
end

return Ui.Define(View, EnemyEncyclopediaInfoPairController)
