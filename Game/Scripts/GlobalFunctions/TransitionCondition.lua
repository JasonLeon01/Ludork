local GlobalCore
local function loadGlobalCore()
    if GlobalCore == nil then
        GlobalCore = require("GlobalCore")
    end
    return GlobalCore
end

local TransitionCondition = {}

function TransitionCondition:init()
    ---@type boolean
    self._started = false
end

function TransitionCondition:poll()
    if loadGlobalCore().Transition.isTransitionPending() or loadGlobalCore().Transition.isInTransition() then
        self._started = true
        return false
    end
    return self._started
end

function TransitionCondition:isFinished()
    return self._started and not loadGlobalCore().Transition.isTransitionPending() and not loadGlobalCore()
            .Transition
            .isInTransition()
end

local FinalTransitionCondition = class(TransitionCondition)
FinalTransitionCondition.__call = TransitionCondition.poll

return FinalTransitionCondition
