---@meta Source.Components.PlayerInfoComponent

--- @brief Editable player identity and battle attributes.
---@class Source.Components.PlayerInfoComponent: Source.Components.BattlerInfoComponent
---@field HP integer
---@field name string
---@field desc string
---@field LEVEL integer
---@field CLASS string
---@field new fun(values?: table<string, Source.Battler.AttributeValue>): Source.Components.PlayerInfoComponent
local PlayerInfoComponent = {}

return PlayerInfoComponent
