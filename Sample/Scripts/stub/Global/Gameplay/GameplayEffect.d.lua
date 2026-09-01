---@meta Global.Gameplay.GameplayEffect

---@class Global.Gameplay.GameplayModifier
---@field attribute string
---@field operation 'Add' | 'Multiply' | 'Override'
---@field magnitude number | fun(spec: Global.Gameplay.GameplayEffectSpec, stacks: integer): number
---@field minimum?  number

---@class Global.Gameplay.GameplayEffect
---@field id               string
---@field durationPolicy   'Instant' | 'Infinite'
---@field stackingPolicy   'None' | 'Aggregate'
---@field modifiers        Global.Gameplay.GameplayModifier[]
---@field grantedTags      string[]
---@field grantedAbilities Global.Gameplay.GameplayAbility[]
---@field data             table<string, any>
---@field new              fun(values?: table<string, any>): Global.Gameplay.GameplayEffect
local GameplayEffect = {}

---@param values? table<string, any>
function GameplayEffect:init(values) end

return GameplayEffect
