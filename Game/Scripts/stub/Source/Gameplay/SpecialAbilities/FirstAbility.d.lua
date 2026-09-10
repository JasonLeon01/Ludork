---@meta Source.Gameplay.SpecialAbilities.FirstAbility

---@class Source.Gameplay.SpecialAbilities.FirstAbility: GlobalCore.GameplayAbility
---@field new fun(): Source.Gameplay.SpecialAbilities.FirstAbility
local FirstAbility = {}

function FirstAbility:init() end

---@param abilitySystem GlobalCore.AbilitySystemComponent
---@param eventData     GlobalCore.GameplayEventData
---@return GlobalCore.GameplayAbilityResult
function FirstAbility:activate(abilitySystem, eventData) end

return FirstAbility
