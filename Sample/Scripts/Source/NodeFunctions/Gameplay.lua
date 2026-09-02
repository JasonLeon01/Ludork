local GlobalCore = require("GlobalCore")
local GameplayEffectSpec = GlobalCore.GameplayEffectSpec
local GameplayEventData = GlobalCore.GameplayEventData
local Context = require("Source.NodeFunctions.Context")
local Effects = require("Source.Gameplay.Effects")

local Gameplay = {}

local function requireGameplayContext(fn)
    local context = Context.RequireGraphParent(fn)
    assert(Class.isInstance(context, GameplayEventData), "Gameplay node requires GameplayEventData")
    return context
end

function Gameplay.GetContext()
    return requireGameplayContext(Gameplay.GetContext)
end

function Gameplay.GetSource()
    return requireGameplayContext(Gameplay.GetSource).instigator
end

function Gameplay.GetTarget()
    return requireGameplayContext(Gameplay.GetTarget).target
end

function Gameplay.GetEventTag()
    return requireGameplayContext(Gameplay.GetEventTag).eventTag
end

function Gameplay.GetPayload()
    return requireGameplayContext(Gameplay.GetPayload).payload
end

function Gameplay.HasTag(target, tag)
    return target:getAbilitySystemComponent():hasMatchingGameplayTag(tag)
end

function Gameplay.GetNumericAttribute(target, attribute)
    return target:getAbilitySystemComponent():getNumericAttribute(attribute)
end

function Gameplay.SetNumericAttributeBase(target, attribute, value)
    target:getAbilitySystemComponent():setNumericAttributeBase(attribute, value)
end

function Gameplay.ApplyAttributeDelta(target, attribute, magnitude)
    return Effects.ApplyInstantModifier(
        target, "Blueprint.AttributeDelta", attribute, "Add", magnitude,
        requireGameplayContext(Gameplay.ApplyAttributeDelta)
    )
end

function Gameplay.ApplyState(target, stateID, stacks)
    return Effects.ApplyState(target, stateID, stacks or 1, requireGameplayContext(Gameplay.ApplyState))
end

function Gameplay.RemoveState(target, stateID)
    return Effects.RemoveState(target, stateID)
end

function Gameplay.ReduceState(target, stateID, stacks)
    return Effects.ReduceState(target, stateID, stacks or 1)
end

function Gameplay.RemovePlayerState(stateID)
    local player = assert(requireGameplayContext(Gameplay.RemovePlayerState).target, "Gameplay event requires a target")
    return Effects.RemoveState(player, stateID)
end

function Gameplay.ReducePlayerState(stateID, stacks)
    local player = assert(requireGameplayContext(Gameplay.ReducePlayerState).target, "Gameplay event requires a target")
    return Effects.ReduceState(player, stateID, stacks or 1)
end

function Gameplay.SendEvent(target, eventTag, payload)
    local sourceContext = requireGameplayContext(Gameplay.SendEvent)
    return target:getAbilitySystemComponent():handleGameplayEvent(
        GameplayEventData.new(sourceContext.instigator, target, eventTag, payload or {})
    )
end

function Gameplay.ApplyEffect(target, effect, stacks, sourceKey)
    return target:getAbilitySystemComponent():applyGameplayEffectSpec(GameplayEffectSpec.new(
        effect, requireGameplayContext(Gameplay.ApplyEffect), stacks or 1, sourceKey
    ))
end

return Gameplay
