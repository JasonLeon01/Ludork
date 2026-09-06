---@meta Source.Gameplay.SpecialAbilities.CompeteAbility

---@class Source.Gameplay.SpecialAbilities.CompeteAbility: GlobalCore.GameplayAbility
---@field new fun(): Source.Gameplay.SpecialAbilities.CompeteAbility
local CompeteAbility = {}

function CompeteAbility:init() end

---@param abilitySystem GlobalCore.AbilitySystemComponent
---@param eventData     GlobalCore.GameplayEventData
---@return GlobalCore.GameplayAbilityResult
function CompeteAbility:activate(abilitySystem, eventData) end

return CompeteAbility
