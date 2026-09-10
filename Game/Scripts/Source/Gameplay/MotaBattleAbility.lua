local GlobalCore = require("GlobalCore")
local GameplayAbility = GlobalCore.GameplayAbility
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult
local GameplayEventData = GlobalCore.GameplayEventData
local Effects = require("Source.Gameplay.Effects")
---@type { Special: Source.Configs.GeneralEnum.Special }
local GeneralEnum = require("Source.Configs.GeneralEnum")
local SpecialAbilities = require("Source.Gameplay.SpecialAbilities")

local Special = GeneralEnum.Special

---@class Source.Gameplay.MotaBattleAbility: GlobalCore.GameplayAbility
local MotaBattleAbility = {}

MotaBattleAbility.id = "Ability.Combat.MotaBattle"
MotaBattleAbility.BattleResult = { WIN = 1, CANNOT_DAMAGE = 2, LETHAL_COUNTER_DAMAGE = 3 }
MotaBattleAbility.CriticalResult = { VALUE = 1, NOT_NEEDED = 2, UNKNOWN = 3 }

local function dispatchValue(abilitySystem, eventTag, instigator, target, payload)
    abilitySystem:handleGameplayEvent(GameplayEventData.new(instigator, target, eventTag, payload))
    return payload.value
end

local function resolveAttack(attacker, defender)
    local abilitySystem = attacker:getAbilitySystemComponent()
    return math.max(
        0,
        dispatchValue(abilitySystem, "Event.Combat.ResolveAttack", attacker, defender, {
            value = abilitySystem:getNumericAttribute("ATK"),
            opponentAbilitySystem = defender:getAbilitySystemComponent()
        })
    )
end

local function resolveDefense(defender, attacker, attackerATK)
    local abilitySystem = defender:getAbilitySystemComponent()
    return math.max(
        0,
        dispatchValue(abilitySystem, "Event.Combat.ResolveDefense", defender, attacker, {
            value = abilitySystem:getNumericAttribute("DEF"),
            attackerATK = attackerATK
        })
    )
end

local function resolveHitCount(attacker, defender)
    return dispatchValue(attacker:getAbilitySystemComponent(), "Event.Combat.ResolveHitCount", attacker, defender, {
        value = 1
    })
end

function MotaBattleAbility:init()
    GameplayAbility.init(self, {})
    self.id = MotaBattleAbility.id
end

local function resolveBattleRules(enemy, player, counterDamage)
    local payload = {
        counterDamage = counterDamage,
        vampireHealing = 0,
        firstStrike = false,
        fixedDamage = 0,
        playerAbilitySystem = player:getAbilitySystemComponent(),
        enemyAbilitySystem = enemy:getAbilitySystemComponent()
    }
    enemy:getAbilitySystemComponent():handleGameplayEvent(GameplayEventData.new(
        enemy, player, SpecialAbilities.BATTLE_RULES_EVENT, payload
    ))
    return payload
end

local function calculateCounterRounds(enemyMAXHP, attackDamage, vampireHealing)
    if attackDamage >= enemyMAXHP then
        return 0
    end
    if attackDamage <= 0 or attackDamage <= vampireHealing then
        return nil
    end
    return math.max(0, math.ceil((enemyMAXHP - attackDamage) / (attackDamage - vampireHealing)))
end

function MotaBattleAbility.CalculateDamagePerRound(attacker, defender)
    local attackerATK = resolveAttack(attacker, defender)
    local defenderDEF = resolveDefense(defender, attacker, attackerATK)
    local payload = { attackerATK = attackerATK, defenderDEF = defenderDEF }
    attacker:getAbilitySystemComponent():handleGameplayEvent(GameplayEventData.new(
        attacker, defender, "Event.Combat.ResolveDamage", payload
    ))
    local hitCount = resolveHitCount(attacker, defender)
    local damage = math.max(0, payload.attackerATK - payload.defenderDEF) * hitCount
    local incomingPayload = { value = damage }
    defender:getAbilitySystemComponent():handleGameplayEvent(GameplayEventData.new(
        attacker, defender, "Event.Combat.ResolveIncomingDamage", incomingPayload
    ))
    return incomingPayload.value, {
            attackerATK = payload.attackerATK,
            defenderDEF = payload.defenderDEF,
            hitCount = hitCount
        }
end

---@diagnostic disable-next-line: unused, Gameplay Ability override intentionally ignores its receiver
function MotaBattleAbility:calculate(abilitySystem, eventData)
    local enemy = abilitySystem:getOwner()
    local player = assert(eventData.target, "Mota Battle requires a target")
    local attackDamage, playerAttack = MotaBattleAbility.CalculateDamagePerRound(player, enemy)
    local counterDamage, enemyAttack = MotaBattleAbility.CalculateDamagePerRound(enemy, player)
    local battleRules = resolveBattleRules(enemy, player, counterDamage)
    local counterRounds = calculateCounterRounds(enemy.attributes.MAXHP, attackDamage, battleRules.vampireHealing)
    local firstStrikeDamage = battleRules.firstStrike and counterDamage or 0
    local damage = counterRounds ~= nil
        and math.max(0, counterRounds * counterDamage + firstStrikeDamage + battleRules.fixedDamage)
        or 0
    local code = MotaBattleAbility.BattleResult.WIN
    if counterRounds == nil then
        code = MotaBattleAbility.BattleResult.CANNOT_DAMAGE
    elseif damage >= player.attributes.HP then
        code = MotaBattleAbility.BattleResult.LETHAL_COUNTER_DAMAGE
    end
    local resultData = {
        damage = damage,
        attackDamage = attackDamage,
        counterDamage = counterDamage,
        counterRounds = counterRounds,
        vampireHealing = battleRules.vampireHealing,
        firstStrikeDamage = firstStrikeDamage,
        fixedDamage = battleRules.fixedDamage,
        playerAttack = playerAttack,
        enemyAttack = enemyAttack,
        enemy = enemy,
        player = player,
        committed = false
    }
    local playerAbilitySystem = player:getAbilitySystemComponent()
    if damage > 0 then
        resultData.damageEffectSpec = Effects.CreateInstantModifierSpec(
            "Combat.CounterDamage", "HP", "Add", -damage, eventData
        )
        playerAbilitySystem:validateGameplayEffectSpec(resultData.damageEffectSpec)
    end
    if code ~= MotaBattleAbility.BattleResult.WIN then
        resultData.gameOverEffectSpec = Effects.CreateInstantModifierSpec(
            "Combat.GameOver", "HP", "Override", 0, eventData
        )
        playerAbilitySystem:validateGameplayEffectSpec(resultData.gameOverEffectSpec)
    end
    return assert(GameplayAbilityResult.Success(code, resultData))
end

function MotaBattleAbility:activate(abilitySystem, eventData)
    local result = self:calculate(abilitySystem, eventData)
    if eventData.payload.commit then
        MotaBattleAbility.CommitResult(result)
    end
    return result
end

function MotaBattleAbility.CommitResult(result)
    assert(Class.isInstance(result, GameplayAbilityResult), "Mota Battle commit requires a GameplayAbilityResult")
    assert(not result.data.committed, "Mota Battle result was already committed")
    if result.data.damageEffectSpec ~= nil then
        result.data.player:getAbilitySystemComponent():applyGameplayEffectSpec(result.data.damageEffectSpec)
    end
    result.data.committed = true
end

function MotaBattleAbility.CalculateCriticalValue(enemy, player)
    local attackDamage, playerAttack = MotaBattleAbility.CalculateDamagePerRound(player, enemy)
    local counterDamage = MotaBattleAbility.CalculateDamagePerRound(enemy, player)
    local battleRules = resolveBattleRules(enemy, player, counterDamage)
    if attackDamage >= enemy.attributes.MAXHP then
        return assert(GameplayAbilityResult.Success(MotaBattleAbility.CriticalResult.NOT_NEEDED))
    end
    if enemy:getAbilitySystemComponent():hasMatchingGameplayTag("Special." .. Special.Hard) then
        return assert(GameplayAbilityResult.Success(MotaBattleAbility.CriticalResult.UNKNOWN))
    end
    local hitCount = math.max(1, playerAttack.hitCount)
    local counterRounds = calculateCounterRounds(enemy.attributes.MAXHP, attackDamage, battleRules.vampireHealing)
    if counterRounds == nil then
        local requiredDamage = battleRules.vampireHealing + 1
        return assert(GameplayAbilityResult.Success(MotaBattleAbility.CriticalResult.VALUE, {
                value = math.ceil(requiredDamage / hitCount) + playerAttack.defenderDEF
            }))
    end
    local requiredDamage = math.ceil(
        (enemy.attributes.MAXHP + (counterRounds - 1) * battleRules.vampireHealing) / counterRounds
    )
    return assert(GameplayAbilityResult.Success(MotaBattleAbility.CriticalResult.VALUE, {
            value = math.ceil(requiredDamage / hitCount) + playerAttack.defenderDEF
        }))
end

return class(MotaBattleAbility, GameplayAbility)
