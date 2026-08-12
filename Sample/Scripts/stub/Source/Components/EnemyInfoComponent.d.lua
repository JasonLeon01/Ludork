---@meta Source.Components.EnemyInfoComponent

--- @brief Editable enemy battle attributes.
---@class Source.Components.EnemyInfoComponent: Source.Components.BattlerInfoComponent
---@field name string
---@field desc string
---@field special table<string, Source.Battler.AttributeValue>
---@field drops string[]
---@field new fun(values?: table<string, Source.Battler.AttributeValue>): Source.Components.EnemyInfoComponent
local EnemyInfoComponent = {}

return EnemyInfoComponent
