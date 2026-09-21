local GlobalCore = require("GlobalCore")
local TransitionCondition = require("Source.NodeFunctions.TransitionCondition")
local FrozenCondition = require("Source.NodeFunctions.FrozenCondition")

local NativeTransition = GlobalCore.Transition
local Transition = {}

function Transition.FreezeTransitionBackground()
    NativeTransition.freezeTransitionBackground()
    return FrozenCondition.new()
end

function Transition.RequestTransition(transitionName, transitionTime)
    transitionName = transitionName == nil and "" or transitionName
    transitionTime = transitionTime == nil and 1.0 or transitionTime
    NativeTransition.requestTransition(transitionName, transitionTime)
    return TransitionCondition.new()
end

return Transition
