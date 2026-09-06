local GlobalCore = require("GlobalCore")
local GeneralEnum = require("Source.Configs.GeneralEnum")

local GameplayAbility = GlobalCore.GameplayAbility
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult
local Special = GeneralEnum.Special

---@class (partial) Source.Gameplay.SpecialAbilities.MagicAbility
local MagicAbility = {}

function MagicAbility:init()
    GameplayAbility.init(self, {})
    self.id = "Special." .. Special.Magic
    self.triggerTags = { "Event.Combat.ResolveDamage" }
end

---@diagnostic disable-next-line: unused, Gameplay Ability override intentionally ignores its receiver
function MagicAbility:activate(_abilitySystem, eventData)
    eventData.payload.defenderDEF = 0
    return assert(GameplayAbilityResult.Success("DamageResolved", eventData.payload))
end

return class(MagicAbility, GameplayAbility)
