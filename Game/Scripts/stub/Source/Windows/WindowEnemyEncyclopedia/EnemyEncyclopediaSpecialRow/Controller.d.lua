---@meta Source.Windows.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow.Controller

---@class Source.Windows.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow.Controller: Source.UIBase.UiController
---@field ui    Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow
---@field model { width: integer, name: string, description: string }
---@field new   fun(model: { width: integer, name: string, description: string }): Source.Windows.WindowEnemyEncyclopedia.EnemyEncyclopediaSpecialRow.Controller
local EnemyEncyclopediaSpecialRowController = {}

function EnemyEncyclopediaSpecialRowController:init(model) end

function EnemyEncyclopediaSpecialRowController:refresh() end

---@return Engine.Canvas
function EnemyEncyclopediaSpecialRowController:prepare() end

---@return integer
function EnemyEncyclopediaSpecialRowController:getHeight() end

return EnemyEncyclopediaSpecialRowController
