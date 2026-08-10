---@meta Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair

---@class Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair: Source.UI.UiController
---@field model { label: string, value: string }
---@field new fun(model: { label: string, value: string }): Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair
local EnemyEncyclopediaInfoPairUI = {}

function EnemyEncyclopediaInfoPairUI:refresh() end

---@param logicalSize sf.Vector2u
---@return Engine.Canvas
function EnemyEncyclopediaInfoPairUI:prepare(logicalSize) end

---@return Engine.FunctionalPlainText
function EnemyEncyclopediaInfoPairUI:getLabel() end

---@return Engine.FunctionalPlainText
function EnemyEncyclopediaInfoPairUI:getValue() end

return EnemyEncyclopediaInfoPairUI
