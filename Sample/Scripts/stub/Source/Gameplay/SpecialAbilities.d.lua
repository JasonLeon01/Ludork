---@meta Source.Gameplay.SpecialAbilities

local SpecialAbilities = {}

---@type string
SpecialAbilities.MOVEMENT_HAZARD_TAG = "Gameplay.Movement.Hazard"

---@param specialID string
---@param magnitude any
---@return Global.Gameplay.GameplayEffect
function SpecialAbilities.CreateEffect(specialID, magnitude) end

---@return Global.Gameplay.GameplayAbility
function SpecialAbilities.CreatePoisonedAbility() end

---@param abilitySystem Global.Gameplay.AbilitySystemComponent
---@param specialID     string
---@return any
function SpecialAbilities.GetMagnitude(abilitySystem, specialID) end

return SpecialAbilities
