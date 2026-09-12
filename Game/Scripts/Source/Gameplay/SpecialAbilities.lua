local GlobalCore = require("GlobalCore")
local GeneralEnum = require("Source.Configs.GeneralEnum")
local CompeteAbility = require("Source.Gameplay.SpecialAbilities.CompeteAbility")
local HardAbility = require("Source.Gameplay.SpecialAbilities.HardAbility")
local MagicAbility = require("Source.Gameplay.SpecialAbilities.MagicAbility")
local MultiHitAbility = require("Source.Gameplay.SpecialAbilities.MultiHitAbility")
local PoisonedAbility = require("Source.Gameplay.SpecialAbilities.PoisonedAbility")
local MovementSpecialAbility = require("Source.Gameplay.SpecialAbilities.MovementSpecialAbility")
local PassiveTagAbility = require("Source.Gameplay.SpecialAbilities.PassiveTagAbility")
local VampireAbility = require("Source.Gameplay.SpecialAbilities.VampireAbility")
local FirstAbility = require("Source.Gameplay.SpecialAbilities.FirstAbility")
local FixDmgAbility = require("Source.Gameplay.SpecialAbilities.FixDmgAbility")
local GameplayConstants = require("Source.Configs.GameplayConstants")

local GameplayEffect = GlobalCore.GameplayEffect
local Special = GeneralEnum.Special

local SpecialAbilities = {}

local abilityTypes = {
    [Special.Compete] = function ()
        return CompeteAbility.new()
    end,
    [Special.Hard] = function ()
        return HardAbility.new()
    end,
    [Special.Magic] = function ()
        return MagicAbility.new()
    end,
    [Special.MultiHit] = function (magnitude)
        assert(math.type(magnitude) == "integer", "MultiHit special magnitude must be an integer")
        return MultiHitAbility.new(magnitude)
    end,
    [Special.Vampire] = function (magnitude)
        assert(math.isFinite(magnitude) and magnitude >= 0, "Vampire special magnitude must be a non-negative number")
        return VampireAbility.new(magnitude)
    end,
    [Special.First] = function ()
        return FirstAbility.new()
    end,
    [Special.FixDmg] = function (magnitude)
        assert(
            math.isFinite(magnitude) and magnitude >= 0 or Class.isInstance(magnitude, "string") and bool(magnitude),
            "FixDmg special magnitude must be a finite non-negative number or a non-empty string"
        )
        return FixDmgAbility.new(magnitude)
    end,
    [Special.Domain] = function (magnitude)
        return MovementSpecialAbility.new(Special.Domain, magnitude)
    end,
    [Special.Flank] = function (magnitude)
        return MovementSpecialAbility.new(Special.Flank, magnitude)
    end,
    [Special.Blockade] = function (magnitude)
        return MovementSpecialAbility.new(Special.Blockade, magnitude)
    end
}

---@type table<string, boolean>
local movementSpecialIDs = { [Special.Domain] = true, [Special.Flank] = true, [Special.Blockade] = true }

function SpecialAbilities.CreateEffect(specialID, magnitude)
    local createAbility = abilityTypes[specialID]
    local ability = createAbility ~= nil and createAbility(magnitude) or PassiveTagAbility.new(specialID)
    local grantedTags = { GameplayConstants.SPECIAL_PREFIX .. specialID }
    if movementSpecialIDs[specialID] then
        grantedTags[#grantedTags + 1] = GameplayConstants.MOVEMENT_HAZARD_TAG
    end
    return GameplayEffect.new({
        id = GameplayConstants.SPECIAL_PREFIX .. specialID,
        durationPolicy = "Infinite",
        stackingPolicy = "None",
        grantedTags = grantedTags,
        grantedAbilities = { ability },
        data = { magnitude = deepcopy(magnitude), specialID = specialID }
    })
end

function SpecialAbilities.CreatePoisonedAbility()
    return PoisonedAbility.new()
end

function SpecialAbilities.GetMagnitude(abilitySystem, specialID)
    for _, activeEffect in ipairs(abilitySystem:getActiveGameplayEffects()) do
        local spec = assert(activeEffect.spec)
        local effect = assert(spec.effect)
        if effect.id == GameplayConstants.SPECIAL_PREFIX .. specialID then
            return effect.data.magnitude
        end
    end
    return nil
end

return SpecialAbilities
