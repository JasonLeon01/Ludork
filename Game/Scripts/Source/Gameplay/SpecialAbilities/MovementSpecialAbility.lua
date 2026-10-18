local GlobalCore = require("GlobalCore")
local Special = require("Enums.GeneralData.Special")
local GameplayConstants = require("Source.Configs.GameplayConstants")

local GameplayAbility = GlobalCore.GameplayAbility
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult

---@class (partial) Source.Gameplay.SpecialAbilities.MovementSpecialAbility
local MovementSpecialAbility = {}

---@param specialID string
---@param magnitude any
function MovementSpecialAbility:init(specialID, magnitude)
    GameplayAbility.init(self, {})
    self.id = GameplayConstants.SPECIAL_PREFIX .. specialID
    self.triggerTags = {
        GameplayConstants.MOVEMENT_QUERY_HAZARD_EVENT, GameplayConstants.MOVEMENT_QUERY_HAZARD_BOUNDS_EVENT
    }
    self._specialID = specialID
    self._magnitude = magnitude
end

function MovementSpecialAbility:activate(_abilitySystem, eventData)
    local radius = 1
    if self._specialID == Special.Domain then
        assert(math.type(self._magnitude) == "integer", "Domain special magnitude must be an integer")
        radius = math.max(1, self._magnitude)
    end
    if eventData.eventTag == GameplayConstants.MOVEMENT_QUERY_HAZARD_BOUNDS_EVENT then
        return assert(GameplayAbilityResult.Success("MovementHazardBounds", { radius = radius }))
    end
    local active = false
    if self._specialID == Special.Domain then
        active = eventData.payload.distance <= radius
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
