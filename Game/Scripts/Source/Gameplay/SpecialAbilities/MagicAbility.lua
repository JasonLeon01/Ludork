local GlobalCore = require("GlobalCore")
local GeneralEnum = require("Source.Configs.GeneralEnum")
local GameplayConstants = require("Source.Configs.GameplayConstants")

local GameplayAbility = GlobalCore.GameplayAbility
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult
local Special = GeneralEnum.Special

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
