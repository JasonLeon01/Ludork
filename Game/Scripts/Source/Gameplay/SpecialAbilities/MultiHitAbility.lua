local GlobalCore = require("GlobalCore")
local GeneralEnum = require("Source.Configs.GeneralEnum")
local GameplayConstants = require("Source.Configs.GameplayConstants")

local GameplayAbility = GlobalCore.GameplayAbility
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult
local Special = GeneralEnum.Special

---@class (partial) Source.Gameplay.SpecialAbilities.MultiHitAbility
local MultiHitAbility = {}

---@param magnitude integer
function MultiHitAbility:init(magnitude)
    GameplayAbility.init(self, {})
    self.id = GameplayConstants.SPECIAL_PREFIX .. Special.MultiHit
    self.triggerTags = { GameplayConstants.COMBAT_RESOLVE_HIT_COUNT_EVENT }
    local clampedMagnitude = math.max(1, magnitude)
    ---@cast clampedMagnitude integer
    self._magnitude = clampedMagnitude
end

function MultiHitAbility:activate(_abilitySystem, eventData)
    eventData.payload.value = self._magnitude
    return assert(GameplayAbilityResult.Success("HitCountResolved", eventData.payload))
end

return class(MultiHitAbility, GameplayAbility)
