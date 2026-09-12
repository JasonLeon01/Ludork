local GlobalCore = require("GlobalCore")
local GeneralEnum = require("Source.Configs.GeneralEnum")
local GameplayConstants = require("Source.Configs.GameplayConstants")

local GameplayAbility = GlobalCore.GameplayAbility
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult
local Special = GeneralEnum.Special

---@class (partial) Source.Gameplay.SpecialAbilities.CompeteAbility
local CompeteAbility = {}

function CompeteAbility:init()
    GameplayAbility.init(self, {})
    self.id = GameplayConstants.SPECIAL_PREFIX .. Special.Compete
    self.triggerTags = { GameplayConstants.COMBAT_RESOLVE_ATTACK_EVENT }
end

---@diagnostic disable-next-line: unused, Gameplay Ability override intentionally ignores its receiver
function CompeteAbility:activate(_abilitySystem, eventData)
    local opponentAbilitySystem = eventData.payload.opponentAbilitySystem
    if opponentAbilitySystem ~= nil then
        eventData.payload.value = math.max(eventData.payload.value, opponentAbilitySystem:getNumericAttribute("ATK"))
    end
    return assert(GameplayAbilityResult.Success("AttackResolved", eventData.payload))
end

return class(CompeteAbility, GameplayAbility)
