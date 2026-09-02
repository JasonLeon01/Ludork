local Engine = require("Engine")
local Ui = require("Source.UI.Ui")

local TextLayout = Engine.TextLayout

local _LABEL_WIDTH = 96
local _VALUE_WIDTH = 104

local EnemyEncyclopediaInfoPairUI = {}

function EnemyEncyclopediaInfoPairUI:refresh()
    local label = self:requireControl("Label")
    local value = self:requireControl("Value")
    ---@cast label Engine.PlainText
    ---@cast value Engine.PlainText
    self:setText("Label", TextLayout.fitPlainText(self.model.label, _LABEL_WIDTH, label))
    self:setText("Value", TextLayout.fitPlainText(self.model.value, _VALUE_WIDTH, value))
end

function EnemyEncyclopediaInfoPairUI:prepare(logicalSize)
    return super(EnemyEncyclopediaInfoPairUI, self).prepare(logicalSize)
end

function EnemyEncyclopediaInfoPairUI:getLabel()
    return self:requireControl("Label")
end

function EnemyEncyclopediaInfoPairUI:getValue()
    return self:requireControl("Value")
end

return Ui.Define("Parts/WindowEnemyEncyclopedia/EnemyEncyclopediaInfoPair", EnemyEncyclopediaInfoPairUI)
