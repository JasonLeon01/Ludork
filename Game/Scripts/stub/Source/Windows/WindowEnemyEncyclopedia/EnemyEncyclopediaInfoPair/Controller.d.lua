---@meta Source.Windows.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair.Controller

---@class Source.Windows.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair.Controller: Source.UIBase.UiController
---@field ui    Source.UI.Parts.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair
---@field model { label: string, value: string }
---@field new   fun(model: { label: string, value: string }): Source.Windows.WindowEnemyEncyclopedia.EnemyEncyclopediaInfoPair.Controller
local EnemyEncyclopediaInfoPairController = {}

function EnemyEncyclopediaInfoPairController:refresh() end

---@param logicalSize sf.Vector2u
---@return Engine.Canvas
function EnemyEncyclopediaInfoPairController:prepare(logicalSize) end

return EnemyEncyclopediaInfoPairController
