local GlobalCore = require("GlobalCore")

local Transition = GlobalCore.Transition
local FrozenCondition = {}

---@diagnostic disable-next-line: unused
function FrozenCondition:poll()
    return not Transition.isTransitionBackgroundFreezePending() and Transition.isTransitionBackgroundFrozen()
end

function FrozenCondition:isFinished()
    return self:poll()
end

local FinalFrozenCondition = class(FrozenCondition)
FinalFrozenCondition.__call = FrozenCondition.poll

return FinalFrozenCondition
