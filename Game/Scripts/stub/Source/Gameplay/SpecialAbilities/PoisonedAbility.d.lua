---@meta Source.Gameplay.SpecialAbilities.PoisonedAbility

---@class Source.Gameplay.SpecialAbilities.PoisonedAbility: GlobalCore.GameplayAbility
---@field new                       fun(): Source.Gameplay.SpecialAbilities.PoisonedAbility
---@field private _DAMAGE_PER_STACK integer
local PoisonedAbility = {}

function PoisonedAbility:init() end

---@param abilitySystem GlobalCore.AbilitySystemComponent
---@param eventData     GlobalCore.GameplayEventData
---@return GlobalCore.GameplayAbilityResult
function PoisonedAbility:activate(abilitySystem, eventData) end

return PoisonedAbility
