local GlobalCore = require("GlobalCore")
local GeneralEnum = require("Source.Configs.GeneralEnum")
local GameplayConstants = require("Source.Configs.GameplayConstants")

local GameplayAbility = GlobalCore.GameplayAbility
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult
local Special = GeneralEnum.Special

---@class (partial) Source.Gameplay.SpecialAbilities.FirstAbility
local FirstAbility = {}

function FirstAbility:init()
    GameplayAbility.init(self, {})
    self.id = GameplayConstants.SPECIAL_PREFIX .. Special.First
    self.triggerTags = { GameplayConstants.BATTLE_RULES_EVENT }
end

---@diagnostic disable-next-line: unused, Gameplay Ability override intentionally ignores its receiver
function FirstAbility:activate(_abilitySystem, eventData)
    eventData.payload.firstStrike = true
    return assert(GameplayAbilityResult.Success("BattleRulesResolved", eventData.payload))
end

return class(FirstAbility, GameplayAbility)
