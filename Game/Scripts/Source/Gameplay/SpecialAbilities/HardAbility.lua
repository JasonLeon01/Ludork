local GlobalCore = require("GlobalCore")
local GeneralEnum = require("Source.Configs.GeneralEnum")

local GameplayAbility = GlobalCore.GameplayAbility
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult
local Special = GeneralEnum.Special

---@class (partial) Source.Gameplay.SpecialAbilities.HardAbility
local HardAbility = {}

function HardAbility:init()
    GameplayAbility.init(self, {})
    self.id = "Special." .. Special.Hard
    self.triggerTags = { "Event.Combat.ResolveDefense" }
end

---@diagnostic disable-next-line: unused, Gameplay Ability override intentionally ignores its receiver
function HardAbility:activate(_abilitySystem, eventData)
    eventData.payload.value = math.max(eventData.payload.value, eventData.payload.attackerATK - 1)
    return assert(GameplayAbilityResult.Success("DefenseResolved", eventData.payload))
end

return class(HardAbility, GameplayAbility)
