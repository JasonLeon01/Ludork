local GlobalFunctions = require("GlobalFunctions")
local Battler = require("Source.Battler")
local Context = require("Source.NodeFunctions.Context")

local ComponentsFunctions = GlobalFunctions.Components
local State = {}

function State.GetStateOwner()
    local owner = Context._getGraphOwner(State.GetStateOwner)
    ---@cast owner Source.Battler.Battler | nil
    return owner
end

function State.GetEventArg(name, default)
    local value = Context._getRefLocal(State.GetEventArg)["__" .. name .. "__"]
    return value == nil and default or value
end

function State.GetBattlerAttr(battler, attrName, default)
    if battler == nil then
        return default
    end
    local value = ComponentsFunctions.getComponentFieldValue(battler, attrName, Class.MISSING)
    if value ~= Class.MISSING then
        return value
    end
    value = battler[attrName]
    return value == nil and default or value
end

function State.SetBattlerAttr(battler, attrName, value)
    if battler == nil then
        return
    end
    if not ComponentsFunctions.setComponentFieldValue(battler, attrName, value) then
        battler[attrName] = value
    end
end

function State.DamageBattler(battler, amount)
    amount = amount == nil and 1 or amount
    if battler == nil then
        return
    end
    local hp = ComponentsFunctions.getComponentFieldValue(battler, "HP", nil)
    if hp == nil then
        return
    end
    ComponentsFunctions.setComponentFieldValue(battler, "HP", hp - amount)
end

function State.HealBattler(battler, amount)
    amount = amount == nil and 1 or amount
    if battler == nil then
        return
    end
    local hp = ComponentsFunctions.getComponentFieldValue(battler, "HP", nil)
    if hp == nil then
        return
    end
    ComponentsFunctions.setComponentFieldValue(battler, "HP", hp + amount)
end

function State.BattlerHasState(battler, stateID)
    if not Class.isInstance(battler, Battler) then
        return false
    end
    return battler:hasState(stateID)
end

function State.AddStateTo(battler, stateID, stacks)
    stacks = stacks == nil and 1 or stacks
    if not Class.isInstance(battler, Battler) then
        return
    end
    battler:addState(stateID, stacks)
end

function State.RemoveStateFrom(battler, stateID)
    if not Class.isInstance(battler, Battler) then
        return
    end
    battler:removeState(stateID)
end

function State.ReduceStateFrom(battler, stateID, stacks)
    stacks = stacks == nil and 1 or stacks
    if not Class.isInstance(battler, Battler) then
        return
    end
    battler:reduceStateStacks(stateID, stacks)
end

function State.TriggerStateHook(battler, stateID)
    if not Class.isInstance(battler, Battler) then
        return
    end
    battler:triggerStateHook(stateID)
end

return State
