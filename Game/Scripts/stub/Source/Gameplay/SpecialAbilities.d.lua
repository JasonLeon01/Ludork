---@meta Source.Gameplay.SpecialAbilities

local SpecialAbilities = {}

---@type string
SpecialAbilities.MOVEMENT_HAZARD_TAG = "Gameplay.Movement.Hazard"

---@type string
SpecialAbilities.BATTLE_RULES_EVENT = "Event.Combat.ResolveBattleRules"

---@param specialID string
---@param magnitude any
---@return GlobalCore.GameplayEffect
function SpecialAbilities.CreateEffect(specialID, magnitude) end

---@return GlobalCore.GameplayAbility
function SpecialAbilities.CreatePoisonedAbility() end

---@param abilitySystem GlobalCore.AbilitySystemComponent
---@param specialID     string
---@return any
function SpecialAbilities.GetMagnitude(abilitySystem, specialID) end

return SpecialAbilities
