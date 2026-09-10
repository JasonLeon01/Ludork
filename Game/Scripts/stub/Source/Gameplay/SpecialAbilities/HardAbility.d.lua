---@meta Source.Gameplay.SpecialAbilities.HardAbility

---@class Source.Gameplay.SpecialAbilities.HardAbility: GlobalCore.GameplayAbility
---@field new fun(): Source.Gameplay.SpecialAbilities.HardAbility
local HardAbility = {}

function HardAbility:init() end

---@param abilitySystem GlobalCore.AbilitySystemComponent
---@param eventData     GlobalCore.GameplayEventData
---@return GlobalCore.GameplayAbilityResult
function HardAbility:activate(abilitySystem, eventData) end

return HardAbility
