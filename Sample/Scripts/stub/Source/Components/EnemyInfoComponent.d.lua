---@meta Source.Components.EnemyInfoComponent

--- @brief Editable enemy battle attributes.
---@class Source.Components.EnemyInfoComponent: Source.Components.BattlerInfoComponent
---@field name string
---@field desc string
---@field special Source.Data.EnemySpecialValues
---@field drops table<string, sf.Vector2i> Item Blueprint class paths mapped to their drop offsets.
---@field new fun(values?: table<string, Source.Battler.AttributeValue>): Source.Components.EnemyInfoComponent
local EnemyInfoComponent = {}

return EnemyInfoComponent
