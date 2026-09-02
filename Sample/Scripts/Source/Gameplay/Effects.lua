local GlobalCore = require("GlobalCore")
local GameplayEffect = GlobalCore.GameplayEffect
local GameplayEffectSpec = GlobalCore.GameplayEffectSpec
local GeneralDataGraphAbility = require("Source.Gameplay.GeneralDataGraphAbility")
local SpecialAbilities = require("Source.Gameplay.SpecialAbilities")
local Data = require("Source.Data")

local Effects = {}

function Effects.CreateInstantModifierSpec(effectID, attribute, operation, magnitude, eventData)
    local effect = GameplayEffect.new({
        id = effectID,
        durationPolicy = "Instant",
        stackingPolicy = "None",
        modifiers = {
            {
                attribute = attribute,
                operation = operation,
                magnitude = magnitude
            }
        }
    })
    return GameplayEffectSpec.new(effect, eventData)
end

function Effects.ApplyInstantModifier(target, effectID, attribute, operation, magnitude, eventData)
    return target
        :getAbilitySystemComponent()
        :applyGameplayEffectSpec(Effects.CreateInstantModifierSpec(effectID, attribute, operation, magnitude, eventData))
end

function Effects.CreateEquipmentEffect(equipID, slot, attrPlus)
    local modifiers = {}
    for attribute, magnitude in pairs(attrPlus) do
        modifiers[#modifiers + 1] = { attribute = attribute, operation = "Add", magnitude = magnitude }
    end
    table.sort(modifiers, function (left, right)
        return left.attribute < right.attribute
    end)
    return GameplayEffect.new({
        id = "Equipment." .. slot .. "." .. equipID,
        durationPolicy = "Infinite",
        stackingPolicy = "None",
        modifiers = modifiers,
        grantedTags = { "Equipment.Slot." .. slot },
        data = { equipID = equipID, slot = slot }
    })
end

function Effects.CreateStateEffect(stateID)
    local stateData = Data.GetGeneralStateData(stateID)
    local modifiers = {}
    local grantedAbilities = {
        GeneralDataGraphAbility.new("State", stateID, "onWalk", { "Event.Movement.Step" }),
        GeneralDataGraphAbility.new("State", stateID, "onHookTriggered", { "Event.State.Trigger." .. stateID })
    }
    if stateID == "Weak" then
        modifiers = {
            { attribute = "ATK", operation = "Add", magnitude = -2, minimum = 0 },
            { attribute = "DEF", operation = "Add", magnitude = -2, minimum = 0 }
        }
    elseif stateID == "Poisoned" then
        grantedAbilities[#grantedAbilities + 1] = SpecialAbilities.CreatePoisonedAbility()
    end
    return GameplayEffect.new({
        id = "State." .. stateID,
        durationPolicy = "Infinite",
        stackingPolicy = bool(stateData.stackable) and "Aggregate" or "None",
        modifiers = modifiers,
        grantedTags = { "State." .. stateID },
        grantedAbilities = grantedAbilities,
        data = { stateID = stateID }
    })
end

function Effects.ApplyState(target, stateID, stacks, eventData)
    return target
        :getAbilitySystemComponent()
        :applyGameplayEffectSpec(Effects.CreateStateSpec(stateID, stacks, eventData))
end

function Effects.CreateStateSpec(stateID, stacks, eventData)
    stacks = stacks or 1
    assert(math.type(stacks) == "integer" and stacks > 0, "State stacks must be positive")
    local effect = Effects.CreateStateEffect(stateID)
    return GameplayEffectSpec.new(effect, eventData, stacks, "State." .. stateID)
end

function Effects.RemoveState(target, stateID)
    local abilitySystem = target:getAbilitySystemComponent()
    local handle = Effects.FindActiveEffectHandle(abilitySystem, "State." .. stateID)
    if handle == nil then
        return false
    end
    return abilitySystem:removeActiveGameplayEffect(handle)
end

function Effects.ReduceState(target, stateID, stacks)
    stacks = stacks or 1
    local abilitySystem = target:getAbilitySystemComponent()
    local handle = Effects.FindActiveEffectHandle(abilitySystem, "State." .. stateID)
    if handle == nil then
        return false
    end
    return abilitySystem:removeActiveGameplayEffect(handle, stacks)
end

function Effects.ClearStates(target)
    local abilitySystem = target:getAbilitySystemComponent()
    local handles = {}
    for _, activeEffect in ipairs(abilitySystem:getActiveGameplayEffects()) do
        local spec = assert(activeEffect.spec)
        local effect = assert(spec.effect)
        if string.startsWith(effect.id, "State.") then
            handles[#handles + 1] = activeEffect.handle
        end
    end
    for _, handle in ipairs(handles) do
        abilitySystem:removeActiveGameplayEffect(handle)
    end
end

function Effects.GetStateStacks(target)
    local result = {}
    for _, activeEffect in ipairs(target:getAbilitySystemComponent():getActiveGameplayEffects()) do
        local spec = assert(activeEffect.spec)
        local effect = assert(spec.effect)
        local stateID = effect.data.stateID
        if stateID ~= nil then
            result[stateID] = activeEffect.stacks
        end
    end
    return result
end

function Effects.GetStateIDs(target)
    return table.orderedStringKeys(Effects.GetStateStacks(target))
end

function Effects.FindActiveEffectHandle(abilitySystem, effectID)
    for _, activeEffect in ipairs(abilitySystem:getActiveGameplayEffects()) do
        local spec = assert(activeEffect.spec)
        local effect = assert(spec.effect)
        if effect.id == effectID then
            return activeEffect.handle
        end
    end
    return nil
end

return Effects
