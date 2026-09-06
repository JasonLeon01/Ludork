local GlobalCore = require("GlobalCore")
local GeneralEnum = require("Source.Configs.GeneralEnum")
local Constants = require("Source.Gameplay.SpecialAbilities.Constants")

local GameplayAbility = GlobalCore.GameplayAbility
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult
local Special = GeneralEnum.Special

---@class (partial) Source.Gameplay.SpecialAbilities.VampireAbility
local VampireAbility = {}

---@param magnitude number
function VampireAbility:init(magnitude)
    GameplayAbility.init(self, {})
    self.id = "Special." .. Special.Vampire
    self.triggerTags = { Constants.BATTLE_RULES_EVENT }
    self._magnitude = magnitude
end

function VampireAbility:activate(_abilitySystem, eventData)
    eventData.payload.vampireHealing = math.floor(eventData.payload.counterDamage * self._magnitude)
    return assert(GameplayAbilityResult.Success("BattleRulesResolved", eventData.payload))
end

return class(VampireAbility, GameplayAbility)
