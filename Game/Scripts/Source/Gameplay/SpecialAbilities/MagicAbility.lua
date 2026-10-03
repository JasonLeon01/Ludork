local GlobalCore = require("GlobalCore")
local Special = require("Enums.GeneralData.Special")
local GameplayConstants = require("Source.Configs.GameplayConstants")

local GameplayAbility = GlobalCore.GameplayAbility
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult

---@class (partial) Source.Gameplay.SpecialAbilities.MagicAbility
local MagicAbility = {}

function MagicAbility:init()
    GameplayAbility.init(self, {})
    self.id = GameplayConstants.SPECIAL_PREFIX .. Special.Magic
    self.triggerTags = { GameplayConstants.COMBAT_RESOLVE_DAMAGE_EVENT }
end

---@diagnostic disable-next-line: unused, Gameplay Ability override intentionally ignores its receiver
function MagicAbility:activate(_abilitySystem, eventData)
    eventData.payload.defenderDEF = 0
    return assert(GameplayAbilityResult.Success("DamageResolved", eventData.payload))
end

return class(MagicAbility, GameplayAbility)
