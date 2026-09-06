---@meta Source.Gameplay.SpecialAbilities.MovementSpecialAbility

---@class Source.Gameplay.SpecialAbilities.MovementSpecialAbility: GlobalCore.GameplayAbility
---@field new        fun(specialID: string, magnitude: any): Source.Gameplay.SpecialAbilities.MovementSpecialAbility
---@field _specialID string
---@field _magnitude any
local MovementSpecialAbility = {}

---@param specialID string
---@param magnitude any
function MovementSpecialAbility:init(specialID, magnitude) end

---@param abilitySystem GlobalCore.AbilitySystemComponent
---@param eventData     GlobalCore.GameplayEventData
---@return GlobalCore.GameplayAbilityResult
function MovementSpecialAbility:activate(abilitySystem, eventData) end

return MovementSpecialAbility
