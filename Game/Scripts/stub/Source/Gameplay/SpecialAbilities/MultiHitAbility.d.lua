---@meta Source.Gameplay.SpecialAbilities.MultiHitAbility

---@class Source.Gameplay.SpecialAbilities.MultiHitAbility: GlobalCore.GameplayAbility
---@field new        fun(magnitude: integer): Source.Gameplay.SpecialAbilities.MultiHitAbility
---@field _magnitude integer
local MultiHitAbility = {}

---@param magnitude integer
function MultiHitAbility:init(magnitude) end

---@param abilitySystem GlobalCore.AbilitySystemComponent
---@param eventData     GlobalCore.GameplayEventData
---@return GlobalCore.GameplayAbilityResult
function MultiHitAbility:activate(abilitySystem, eventData) end

return MultiHitAbility
