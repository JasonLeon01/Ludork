local GameplayAbility = require("Global.Gameplay.GameplayAbility")
local GameplayAbilityResult = require("Global.Gameplay.GameplayAbilityResult")
local GameplayEffect = require("Global.Gameplay.GameplayEffect")
---@type { Special: Source.Configs.GeneralEnum.Special }
local GeneralEnum = require("Source.Configs.GeneralEnum")

local Special = GeneralEnum.Special

local SpecialAbilities = {}

SpecialAbilities.MOVEMENT_HAZARD_TAG = "Gameplay.Movement.Hazard"

---@class Source.Gameplay.SpecialAbilities.CompeteAbility: Global.Gameplay.GameplayAbility
local CompeteAbility = {}

function CompeteAbility:init()
    GameplayAbility.init(self, {
        id = "Special." .. Special.Compete,
        triggerTags = { "Event.Combat.ResolveAttack" }
    })
end

---@diagnostic disable-next-line: unused, Gameplay Ability override intentionally ignores its receiver
function CompeteAbility:activate(_abilitySystem, eventData)
    local opponentAbilitySystem = eventData.payload.opponentAbilitySystem
    if opponentAbilitySystem ~= nil then
        eventData.payload.value = math.max(eventData.payload.value, opponentAbilitySystem:getNumericAttribute("ATK"))
    end
    return GameplayAbilityResult.Success("AttackResolved", eventData.payload)
end

---@class Source.Gameplay.SpecialAbilities.HardAbility: Global.Gameplay.GameplayAbility
local HardAbility = {}

function HardAbility:init()
    GameplayAbility.init(self, {
        id = "Special." .. Special.Hard,
        triggerTags = { "Event.Combat.ResolveDefense" }
    })
end

---@diagnostic disable-next-line: unused, Gameplay Ability override intentionally ignores its receiver
function HardAbility:activate(_abilitySystem, eventData)
    eventData.payload.value = math.max(eventData.payload.value, eventData.payload.attackerATK - 1)
    return GameplayAbilityResult.Success("DefenseResolved", eventData.payload)
end

---@class Source.Gameplay.SpecialAbilities.MagicAbility: Global.Gameplay.GameplayAbility
local MagicAbility = {}

function MagicAbility:init()
    GameplayAbility.init(self, {
        id = "Special." .. Special.Magic,
        triggerTags = { "Event.Combat.ResolveDamage" }
    })
end

---@diagnostic disable-next-line: unused, Gameplay Ability override intentionally ignores its receiver
function MagicAbility:activate(_abilitySystem, eventData)
    eventData.payload.defenderDEF = 0
    return GameplayAbilityResult.Success("DamageResolved", eventData.payload)
end

---@class Source.Gameplay.SpecialAbilities.MultiHitAbility: Global.Gameplay.GameplayAbility
---@field _magnitude integer
local MultiHitAbility = {}

---@param magnitude integer
function MultiHitAbility:init(magnitude)
    GameplayAbility.init(self, {
        id = "Special." .. Special.MultiHit,
        triggerTags = { "Event.Combat.ResolveHitCount" }
    })
    local clampedMagnitude = math.max(1, magnitude)
    ---@cast clampedMagnitude integer
    self._magnitude = clampedMagnitude
end

function MultiHitAbility:activate(_abilitySystem, eventData)
    eventData.payload.value = self._magnitude
    return GameplayAbilityResult.Success("HitCountResolved", eventData.payload)
end

---@class Source.Gameplay.SpecialAbilities.PoisonedAbility: Global.Gameplay.GameplayAbility
local PoisonedAbility = {}

function PoisonedAbility:init()
    GameplayAbility.init(self, {
        id = "State.Poisoned.Combat",
        triggerTags = { "Event.Combat.ResolveIncomingDamage" }
    })
end

---@diagnostic disable-next-line: unused, Gameplay Ability override intentionally ignores its receiver
function PoisonedAbility:activate(abilitySystem, eventData)
    local stacks = abilitySystem:getActiveEffectStacks("State.Poisoned")
    eventData.payload.value = eventData.payload.value + 10 * stacks
    return GameplayAbilityResult.Success("IncomingDamageResolved", eventData.payload)
end

---@class Source.Gameplay.SpecialAbilities.MovementSpecialAbility: Global.Gameplay.GameplayAbility
---@field _specialID string
---@field _magnitude any
local MovementSpecialAbility = {}

---@param specialID string
---@param magnitude any
function MovementSpecialAbility:init(specialID, magnitude)
    GameplayAbility.init(self, {
        id = "Special." .. specialID,
        triggerTags = { "Event.Movement.QueryHazard" }
    })
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
    return GameplayAbilityResult.Success("MovementHazard", {
        active = active,
        special = self._specialID,
        magnitude = self._magnitude,
        damage = active and eventData.payload.damagePerRound or 0
    })
end

---@class Source.Gameplay.SpecialAbilities.PassiveTagAbility: Global.Gameplay.GameplayAbility
local PassiveTagAbility = {}

---@param specialID string
function PassiveTagAbility:init(specialID)
    GameplayAbility.init(self, { id = "Special." .. specialID })
end

---@type Class.ClassType<Source.Gameplay.SpecialAbilities.CompeteAbility>
local FinalCompeteAbility = class(CompeteAbility, GameplayAbility)
---@type Class.ClassType<Source.Gameplay.SpecialAbilities.HardAbility>
local FinalHardAbility = class(HardAbility, GameplayAbility)
---@type Class.ClassType<Source.Gameplay.SpecialAbilities.MagicAbility>
local FinalMagicAbility = class(MagicAbility, GameplayAbility)
---@type Class.ClassType<Source.Gameplay.SpecialAbilities.MultiHitAbility>
local FinalMultiHitAbility = class(MultiHitAbility, GameplayAbility)
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
        if activeEffect.spec.effect.id == "Special." .. specialID then
            return activeEffect.spec.effect.data.magnitude
        end
    end
    return nil
end

return SpecialAbilities
