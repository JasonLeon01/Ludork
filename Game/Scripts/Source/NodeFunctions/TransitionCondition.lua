local GlobalCore = require("GlobalCore")

local Transition = GlobalCore.Transition
local TransitionCondition = {}

function TransitionCondition:init()
    ---@type boolean
    self._started = false
end

function TransitionCondition:poll()
    if Transition.isTransitionPending() or Transition.isInTransition() then
        self._started = true
        return false
    end
    return self._started
end

function TransitionCondition:isFinished()
    return self._started and not Transition.isTransitionPending() and not Transition.isInTransition()
end

local FinalTransitionCondition = class(TransitionCondition)
FinalTransitionCondition.__call = TransitionCondition.poll

return FinalTransitionCondition
