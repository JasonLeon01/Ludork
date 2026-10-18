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
--- QueryHazardBounds returns MovementHazardBounds with a conservative non-negative
--- integer radius around the enemy anchor. QueryHazard resolves the exact cell.
function MovementSpecialAbility:activate(abilitySystem, eventData) end

return MovementSpecialAbility
