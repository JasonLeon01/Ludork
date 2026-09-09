local Engine = require("Engine")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair")

local TextLayout = Engine.TextLayout

local _LABEL_WIDTH = 96
local _VALUE_WIDTH = 104

local EnemyEncyclopediaInfoPairController = {}

function EnemyEncyclopediaInfoPairController:refresh()
    local label = self.ui.controls["Label"]
    local value = self.ui.controls["Value"]
    ---@cast label Engine.PlainText
    ---@cast value Engine.PlainText
    self:setText("Label", TextLayout.fitPlainText(self.model.label, _LABEL_WIDTH, label))
    self:setText("Value", TextLayout.fitPlainText(self.model.value, _VALUE_WIDTH, value))
end

function EnemyEncyclopediaInfoPairController:prepare(logicalSize)
    return super(EnemyEncyclopediaInfoPairController, self).prepare(logicalSize)
end

return Ui.Define(View, EnemyEncyclopediaInfoPairController)
