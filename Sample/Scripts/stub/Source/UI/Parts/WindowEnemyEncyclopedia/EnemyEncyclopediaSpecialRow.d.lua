---@meta Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow

---@class Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow: Source.UI.UiController
---@field model { width: integer, name: string, description: string }
---@field new fun(model: { width: integer, name: string, description: string }): Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow
local EnemyEncyclopediaSpecialRowUI = {}

function EnemyEncyclopediaSpecialRowUI:init(model) end

function EnemyEncyclopediaSpecialRowUI:refresh() end

---@return Engine.Canvas
function EnemyEncyclopediaSpecialRowUI:prepare() end

---@return integer
function EnemyEncyclopediaSpecialRowUI:getHeight() end

---@return Engine.FunctionalPlainText
function EnemyEncyclopediaSpecialRowUI:getNameText() end

---@return Engine.FunctionalPlainText
function EnemyEncyclopediaSpecialRowUI:getDescriptionText() end

return EnemyEncyclopediaSpecialRowUI
