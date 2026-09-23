local GlobalCore
local function loadGlobalCore()
    if GlobalCore == nil then
        GlobalCore = require("GlobalCore")
    end
    return GlobalCore
end

local TransitionCondition
local function loadTransitionCondition()
    if TransitionCondition == nil then
        TransitionCondition = require("GlobalFunctions.TransitionCondition")
    end
    return TransitionCondition
end

local FrozenCondition
local function loadFrozenCondition()
    if FrozenCondition == nil then
        FrozenCondition = require("GlobalFunctions.FrozenCondition")
    end
    return FrozenCondition
end

local Transition = {}

function Transition.FreezeTransitionBackground()
    loadGlobalCore().Transition.freezeTransitionBackground()
    return loadFrozenCondition().new()
end

function Transition.RequestTransition(transitionName, transitionTime)
    transitionName = transitionName == nil and "" or transitionName
    transitionTime = transitionTime == nil and 1.0 or transitionTime
    loadGlobalCore().Transition.requestTransition(transitionName, transitionTime)
    return loadTransitionCondition().new()
end

return Transition
