local TextLayout = require("Source.TextLayout")
local Ui = require("Source.UI.Ui")

local _LABEL_TEXT_CONFIG = "UI/LeftText16"
local _VALUE_TEXT_CONFIG = "UI/RightText16"
local _LABEL_WIDTH = 96
local _VALUE_WIDTH = 104

local EnemyEncyclopediaInfoPairUI = {}

function EnemyEncyclopediaInfoPairUI:refresh()
    self:setText("Label", TextLayout.fitPlainText(self.model.label, _LABEL_WIDTH, _LABEL_TEXT_CONFIG))
    self:setText("Value", TextLayout.fitPlainText(self.model.value, _VALUE_WIDTH, _VALUE_TEXT_CONFIG))
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

return Ui.define("Parts/WindowEnemyEncyclopedia/EnemyEncyclopediaInfoPair", EnemyEncyclopediaInfoPairUI)
