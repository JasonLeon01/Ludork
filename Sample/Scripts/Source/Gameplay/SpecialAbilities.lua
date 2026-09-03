local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameplayAbility = GlobalCore.GameplayAbility
local GameplayAbilityResult = GlobalCore.GameplayAbilityResult
local GameplayEffect = GlobalCore.GameplayEffect
---@type { Special: Source.Configs.GeneralEnum.Special }
local GeneralEnum = require("Source.Configs.GeneralEnum")

local Special = GeneralEnum.Special

local SpecialAbilities = {}

SpecialAbilities.MOVEMENT_HAZARD_TAG = "Gameplay.Movement.Hazard"
SpecialAbilities.BATTLE_RULES_EVENT = "Event.Combat.ResolveBattleRules"

local function isFiniteNumber(value)
    return Class.isInstance(value, "number") and value == value and value ~= math.huge and value ~= -math.huge
end

local function replaceNumericAttributes(expression, prefix, abilitySystem)
    for _, attribute in ipairs(table.orderedStringKeys(abilitySystem:getNumericAttributeBases())) do
        expression = string.replace(
            expression, "{" .. prefix .. attribute .. "}", tostring(abilitySystem:getNumericAttribute(attribute))
        )
    end
    return expression
end

---@class Source.Gameplay.SpecialAbilities.CompeteAbility: GlobalCore.GameplayAbility
local CompeteAbility = {}

function CompeteAbility:init()
    GameplayAbility.init(self, {})
    self.id = "Special." .. Special.Compete
    self.triggerTags = { "Event.Combat.ResolveAttack" }
end

---@diagnostic disable-next-line: unused, Gameplay Ability override intentionally ignores its receiver
function CompeteAbility:activate(_abilitySystem, eventData)
    local opponentAbilitySystem = eventData.payload.opponentAbilitySystem
    if opponentAbilitySystem ~= nil then
        eventData.payload.value = math.max(eventData.payload.value, opponentAbilitySystem:getNumericAttribute("ATK"))
    end
    return assert(GameplayAbilityResult.Success("AttackResolved", eventData.payload))
end

---@class Source.Gameplay.SpecialAbilities.HardAbility: GlobalCore.GameplayAbility
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

---@class Source.Gameplay.SpecialAbilities.MagicAbility: GlobalCore.GameplayAbility
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

---@class Source.Gameplay.SpecialAbilities.MultiHitAbility: GlobalCore.GameplayAbility
---@field _magnitude integer
local MultiHitAbility = {}

---@param magnitude integer
function MultiHitAbility:init(magnitude)
    GameplayAbility.init(self, {})
    self.id = "Special." .. Special.MultiHit
    self.triggerTags = { "Event.Combat.ResolveHitCount" }
    local clampedMagnitude = math.max(1, magnitude)
    ---@cast clampedMagnitude integer
    self._magnitude = clampedMagnitude
end

function MultiHitAbility:activate(_abilitySystem, eventData)
    eventData.payload.value = self._magnitude
    return assert(GameplayAbilityResult.Success("HitCountResolved", eventData.payload))
end

---@class Source.Gameplay.SpecialAbilities.PoisonedAbility: GlobalCore.GameplayAbility
local PoisonedAbility = {}

function PoisonedAbility:init()
    GameplayAbility.init(self, {})
    self.id = "State.Poisoned.Combat"
    self.triggerTags = { "Event.Combat.ResolveIncomingDamage" }
end

---@diagnostic disable-next-line: unused, Gameplay Ability override intentionally ignores its receiver
function PoisonedAbility:activate(abilitySystem, eventData)
    local stacks = abilitySystem:getActiveEffectStacks("State.Poisoned")
    eventData.payload.value = eventData.payload.value + 10 * stacks
    return assert(GameplayAbilityResult.Success("IncomingDamageResolved", eventData.payload))
end

---@class Source.Gameplay.SpecialAbilities.MovementSpecialAbility: GlobalCore.GameplayAbility
---@field _specialID string
---@field _magnitude any
local MovementSpecialAbility = {}

---@param specialID string
---@param magnitude any
function MovementSpecialAbility:init(specialID, magnitude)
    GameplayAbility.init(self, {})
    self.id = "Special." .. specialID
    self.triggerTags = { "Event.Movement.QueryHazard" }
    self._specialID = specialID
    self._magnitude = magnitude
end

function MovementSpecialAbility:activate(_abilitySystem, eventData)
    local active = false
    if self._specialID == Special.Domain then
        assert(math.type(self._magnitude) == "integer", "Domain special magnitude must be an integer")
        active = eventData.payload.distance < math.max(1, self._magnitude)
    elseif self._specialID == Special.Blockade then
        active = eventData.payload.distance == 1
    elseif self._specialID == Special.Flank then
        active = true
    end
    return assert(GameplayAbilityResult.Success("MovementHazard", {
            active = active,
            special = self._specialID,
            magnitude = self._magnitude,
            damage = active and eventData.payload.damagePerRound or 0
        }))
end

---@class Source.Gameplay.SpecialAbilities.PassiveTagAbility: GlobalCore.GameplayAbility
local PassiveTagAbility = {}

---@param specialID string
function PassiveTagAbility:init(specialID)
    GameplayAbility.init(self, {})
    self.id = "Special." .. specialID
end

---@class Source.Gameplay.SpecialAbilities.VampireAbility: GlobalCore.GameplayAbility
---@field _magnitude number
local VampireAbility = {}

---@param magnitude number
function VampireAbility:init(magnitude)
    GameplayAbility.init(self, {})
    self.id = "Special." .. Special.Vampire
    self.triggerTags = { SpecialAbilities.BATTLE_RULES_EVENT }
    self._magnitude = magnitude
end

function VampireAbility:activate(_abilitySystem, eventData)
    eventData.payload.vampireHealing = math.floor(eventData.payload.counterDamage * self._magnitude)
    return assert(GameplayAbilityResult.Success("BattleRulesResolved", eventData.payload))
end

---@class Source.Gameplay.SpecialAbilities.FirstAbility: GlobalCore.GameplayAbility
local FirstAbility = {}

function FirstAbility:init()
    GameplayAbility.init(self, {})
    self.id = "Special." .. Special.First
    self.triggerTags = { SpecialAbilities.BATTLE_RULES_EVENT }
end

---@diagnostic disable-next-line: unused, Gameplay Ability override intentionally ignores its receiver
function FirstAbility:activate(_abilitySystem, eventData)
    eventData.payload.firstStrike = true
    return assert(GameplayAbilityResult.Success("BattleRulesResolved", eventData.payload))
end

---@class Source.Gameplay.SpecialAbilities.FixDmgAbility: GlobalCore.GameplayAbility
---@field _value number | string
local FixDmgAbility = {}

---@param value number | string
function FixDmgAbility:init(value)
    GameplayAbility.init(self, {})
    self.id = "Special." .. Special.FixDmg
    self.triggerTags = { SpecialAbilities.BATTLE_RULES_EVENT }
    self._value = value
end

function FixDmgAbility:activate(_abilitySystem, eventData)
    local damage = self._value
    if Class.isInstance(damage, "string") then
        damage = replaceNumericAttributes(damage, "m", eventData.payload.playerAbilitySystem)
        damage = replaceNumericAttributes(damage, "e", eventData.payload.enemyAbilitySystem)
        damage = Engine.Eval(damage, {})
    end
    assert(isFiniteNumber(damage), "FixDmg special value must resolve to a finite number")
    damage = math.floor(damage)
    assert(damage >= 0, "FixDmg special value must resolve to a non-negative number")
    eventData.payload.fixedDamage = damage
    return assert(GameplayAbilityResult.Success("BattleRulesResolved", eventData.payload))
end

---@type Class.ClassType<Source.Gameplay.SpecialAbilities.CompeteAbility>
local FinalCompeteAbility = class(CompeteAbility, GameplayAbility)
---@type Class.ClassType<Source.Gameplay.SpecialAbilities.HardAbility>
local FinalHardAbility = class(HardAbility, GameplayAbility)
---@type Class.ClassType<Source.Gameplay.SpecialAbilities.MagicAbility>
local FinalMagicAbility = class(MagicAbility, GameplayAbility)
---@type Class.ClassType<Source.Gameplay.SpecialAbilities.MultiHitAbility>
local FinalMultiHitAbility = class(MultiHitAbility, GameplayAbility)
---@type Class.ClassType<Source.Gameplay.SpecialAbilities.VampireAbility>
local FinalVampireAbility = class(VampireAbility, GameplayAbility)
---@type Class.ClassType<Source.Gameplay.SpecialAbilities.FirstAbility>
local FinalFirstAbility = class(FirstAbility, GameplayAbility)
---@type Class.ClassType<Source.Gameplay.SpecialAbilities.FixDmgAbility>
local FinalFixDmgAbility = class(FixDmgAbility, GameplayAbility)
---@type Class.ClassType<Source.Gameplay.SpecialAbilities.PoisonedAbility>
local FinalPoisonedAbility = class(PoisonedAbility, GameplayAbility)
---@type Class.ClassType<Source.Gameplay.SpecialAbilities.MovementSpecialAbility>
local FinalMovementSpecialAbility = class(MovementSpecialAbility, GameplayAbility)
---@type Class.ClassType<Source.Gameplay.SpecialAbilities.PassiveTagAbility>
local FinalPassiveTagAbility = class(PassiveTagAbility, GameplayAbility)

local abilityTypes = {
    [Special.Compete] = function ()
        return FinalCompeteAbility.new()
    end,
    [Special.Hard] = function ()
        return FinalHardAbility.new()
    end,
    [Special.Magic] = function ()
        return FinalMagicAbility.new()
    end,
    [Special.MultiHit] = function (magnitude)
        assert(math.type(magnitude) == "integer", "MultiHit special magnitude must be an integer")
        return FinalMultiHitAbility.new(magnitude)
    end,
    [Special.Vampire] = function (magnitude)
        assert(isFiniteNumber(magnitude) and magnitude >= 0, "Vampire special magnitude must be a non-negative number")
        return FinalVampireAbility.new(magnitude)
    end,
    [Special.First] = function ()
        return FinalFirstAbility.new()
    end,
    [Special.FixDmg] = function (magnitude)
        assert(
            isFiniteNumber(magnitude) and magnitude >= 0 or Class.isInstance(magnitude, "string") and bool(magnitude),
            "FixDmg special magnitude must be a finite non-negative number or a non-empty string"
        )
        return FinalFixDmgAbility.new(magnitude)
    end,
    [Special.Domain] = function (magnitude)
        return FinalMovementSpecialAbility.new(Special.Domain, magnitude)
    end,
    [Special.Flank] = function (magnitude)
        return FinalMovementSpecialAbility.new(Special.Flank, magnitude)
    end,
    [Special.Blockade] = function (magnitude)
        return FinalMovementSpecialAbility.new(Special.Blockade, magnitude)
    end
}

---@type table<string, boolean>
local movementSpecialIDs = { [Special.Domain] = true, [Special.Flank] = true, [Special.Blockade] = true }

function SpecialAbilities.CreateEffect(specialID, magnitude)
    local createAbility = abilityTypes[specialID]
    local ability = createAbility ~= nil and createAbility(magnitude) or FinalPassiveTagAbility.new(specialID)
    local grantedTags = { "Special." .. specialID }
    if movementSpecialIDs[specialID] then
        grantedTags[#grantedTags + 1] = SpecialAbilities.MOVEMENT_HAZARD_TAG
    end
    return GameplayEffect.new({
        id = "Special." .. specialID,
        durationPolicy = "Infinite",
        stackingPolicy = "None",
        grantedTags = grantedTags,
        grantedAbilities = { ability },
        data = { magnitude = deepcopy(magnitude), specialID = specialID }
    })
end

function SpecialAbilities.CreatePoisonedAbility()
    return FinalPoisonedAbility.new()
end

function SpecialAbilities.GetMagnitude(abilitySystem, specialID)
    for _, activeEffect in ipairs(abilitySystem:getActiveGameplayEffects()) do
        local spec = assert(activeEffect.spec)
        local effect = assert(spec.effect)
        if effect.id == "Special." .. specialID then
            return effect.data.magnitude
        end
    end
    return nil
end

return SpecialAbilities
