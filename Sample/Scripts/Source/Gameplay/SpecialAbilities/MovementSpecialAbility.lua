local GlobalCore = require("GlobalCore")
local GeneralEnum = require("Source.Configs.GeneralEnum")

local GameplayAbility = GlobalCore.GameplayAbility
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult
local Special = GeneralEnum.Special

---@class (partial) Source.Gameplay.SpecialAbilities.MovementSpecialAbility
local MovementSpecialAbility = {}

---@param specialID string
---@param magnitude any
function MovementSpecialAbility:init(specialID, magnitude)
    GameplayAbility.init(self, {})
    self.id = "Special." .. specialID
    self.triggerTags = { "Event.Movement.QueryHazard" }
    self._specialID = specialID
    self._magnitude = magnitude
end

function MovementSpecialAbility:activate(_abilitySystem, eventData)
    local active = false
    if self._specialID == Special.Domain then
        assert(math.type(self._magnitude) == "integer", "Domain special magnitude must be an integer")
        active = eventData.payload.distance < math.max(1, self._magnitude)
    elseif self._specialID == Special.Blockade then
        active = eventData.payload.distance == 1
    elseif self._specialID == Special.Flank then
        active = true
    end
    return assert(GameplayAbilityResult.Success("MovementHazard", {
            active = active,
            special = self._specialID,
            magnitude = self._magnitude,
            damage = active and eventData.payload.damagePerRound or 0
        }))
end

return class(MovementSpecialAbility, GameplayAbility)
