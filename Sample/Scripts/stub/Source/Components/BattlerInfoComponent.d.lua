---@meta Source.Components.BattlerInfoComponent

--- @brief Editable battle attributes for any battling entity.
---@class Source.Components.BattlerInfoComponent: Engine.Component
---@field MAXHP integer
---@field ATK integer
---@field DEF integer
---@field EXP integer
---@field GOLD integer
---@field ANIMATION_KEY string
---@field special table<string, Source.Battler.AttributeValue> | nil
---@field new fun(values?: table<string, Source.Battler.AttributeValue>): Source.Components.BattlerInfoComponent
local BattlerInfoComponent = {}

---@param values table<string, Source.Battler.AttributeValue> | nil
function BattlerInfoComponent:init(values) end

return BattlerInfoComponent
