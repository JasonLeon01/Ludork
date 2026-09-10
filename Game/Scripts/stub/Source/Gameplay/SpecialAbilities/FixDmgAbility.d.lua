---@meta Source.Gameplay.SpecialAbilities.FixDmgAbility

---@class Source.Gameplay.SpecialAbilities.FixDmgAbility: GlobalCore.GameplayAbility
---@field new    fun(value: number | string): Source.Gameplay.SpecialAbilities.FixDmgAbility
---@field _value number | string
local FixDmgAbility = {}

---@param value number | string
function FixDmgAbility:init(value) end

---@param abilitySystem GlobalCore.AbilitySystemComponent
---@param eventData     GlobalCore.GameplayEventData
---@return GlobalCore.GameplayAbilityResult
function FixDmgAbility:activate(abilitySystem, eventData) end

return FixDmgAbility
