local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GeneralEnum = require("Source.Configs.GeneralEnum")
local Constants = require("Source.Gameplay.SpecialAbilities.Constants")

local GameplayAbility = GlobalCore.GameplayAbility
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult
local Special = GeneralEnum.Special

local function replaceNumericAttributes(expression, prefix, abilitySystem)
    for _, attribute in ipairs(table.orderedStringKeys(abilitySystem:getNumericAttributeBases())) do
        expression = string.replace(
            expression, "{" .. prefix .. attribute .. "}", tostring(abilitySystem:getNumericAttribute(attribute))
        )
    end
    return expression
end

---@class (partial) Source.Gameplay.SpecialAbilities.FixDmgAbility
local FixDmgAbility = {}

---@param value number | string
function FixDmgAbility:init(value)
    GameplayAbility.init(self, {})
    self.id = "Special." .. Special.FixDmg
    self.triggerTags = { Constants.BATTLE_RULES_EVENT }
    self._value = value
end

function FixDmgAbility:activate(_abilitySystem, eventData)
    local damage = self._value
    if Class.isInstance(damage, "string") then
        damage = replaceNumericAttributes(damage, "m", eventData.payload.playerAbilitySystem)
        damage = replaceNumericAttributes(damage, "e", eventData.payload.enemyAbilitySystem)
        damage = Engine.Eval(damage, {})
    end
    assert(math.isFinite(damage), "FixDmg special value must resolve to a finite number")
    damage = math.floor(damage)
    assert(damage >= 0, "FixDmg special value must resolve to a non-negative number")
    eventData.payload.fixedDamage = damage
    return assert(GameplayAbilityResult.Success("BattleRulesResolved", eventData.payload))
end

return class(FixDmgAbility, GameplayAbility)
