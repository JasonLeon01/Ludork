---@meta Global.Gameplay.GameplayAbilitySpec

---@class Global.Gameplay.GameplayAbilitySpec
---@field ability    Global.Gameplay.GameplayAbility
---@field sourceKey  any
---@field grantOrder integer
---@field new        fun(ability: Global.Gameplay.GameplayAbility, sourceKey?: any, grantOrder?: integer): Global.Gameplay.GameplayAbilitySpec
local GameplayAbilitySpec = {}

---@param ability     Global.Gameplay.GameplayAbility
---@param sourceKey?  any
---@param grantOrder? integer
function GameplayAbilitySpec:init(ability, sourceKey, grantOrder) end

return GameplayAbilitySpec
