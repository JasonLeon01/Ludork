---@meta Source.Gameplay.SpecialAbilities.MagicAbility

---@class Source.Gameplay.SpecialAbilities.MagicAbility: GlobalCore.GameplayAbility
---@field new fun(): Source.Gameplay.SpecialAbilities.MagicAbility
local MagicAbility = {}

function MagicAbility:init() end

---@param abilitySystem GlobalCore.AbilitySystemComponent
---@param eventData     GlobalCore.GameplayEventData
---@return GlobalCore.GameplayAbilityResult
function MagicAbility:activate(abilitySystem, eventData) end

return MagicAbility
