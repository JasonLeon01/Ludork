local TextLayout = require("Source.TextLayout")
local Ui = require("Source.UI.Ui")

local _LABEL_WIDTH = 96
local _VALUE_WIDTH = 104

local EnemyEncyclopediaInfoPairUI = {}

function EnemyEncyclopediaInfoPairUI:refresh()
    local label = self:requireControl("Label")
    local value = self:requireControl("Value")
    ---@cast label Engine.PlainText
    ---@cast value Engine.PlainText
    self:setText("Label", TextLayout.FitPlainText(self.model.label, _LABEL_WIDTH, label))
    self:setText("Value", TextLayout.FitPlainText(self.model.value, _VALUE_WIDTH, value))
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
