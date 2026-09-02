---@meta Source.Gameplay.SpecialAbilities

local SpecialAbilities = {}

---@type string
SpecialAbilities.MOVEMENT_HAZARD_TAG = "Gameplay.Movement.Hazard"

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
