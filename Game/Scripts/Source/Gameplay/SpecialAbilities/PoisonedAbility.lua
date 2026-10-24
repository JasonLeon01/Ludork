local GlobalCore = require("GlobalCore")
local GameplayConstants = require("Source.Configs.GameplayConstants")
local CombatResolutionCode = require("Enums.CombatResolutionCode")
local State = require("Enums.GeneralData.State")

local GameplayAbility = GlobalCore.GameplayAbility
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult

---@class (partial) Source.Gameplay.SpecialAbilities.PoisonedAbility
local PoisonedAbility = {
    _DAMAGE_PER_STACK = 10
}

function PoisonedAbility:init()
    GameplayAbility.init(self, {})
    self.id = GameplayConstants.STATE_PREFIX .. State.Poisoned .. ".Combat"
    self.triggerTags = { GameplayConstants.COMBAT_RESOLVE_INCOMING_DAMAGE_EVENT }
end

---@diagnostic disable-next-line: unused, Gameplay Ability override intentionally ignores its receiver
function PoisonedAbility:activate(abilitySystem, eventData)
    local stacks = abilitySystem:getActiveEffectStacks(GameplayConstants.STATE_PREFIX .. State.Poisoned)
    eventData.payload.value = eventData.payload.value + PoisonedAbility._DAMAGE_PER_STACK * stacks
    return assert(GameplayAbilityResult.Success(CombatResolutionCode.INCOMING_DAMAGE, eventData.payload))
end

return class(PoisonedAbility, GameplayAbility)
