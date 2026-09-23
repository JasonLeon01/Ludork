local GlobalCore
local function loadGlobalCore()
    if GlobalCore == nil then
        GlobalCore = require("GlobalCore")
    end
    return GlobalCore
end

local FrozenCondition = {}

---@diagnostic disable-next-line: unused
function FrozenCondition:poll()
    return not loadGlobalCore().Transition.isTransitionBackgroundFreezePending() and loadGlobalCore().Transition.isTransitionBackgroundFrozen()
end

function FrozenCondition:isFinished()
    return self:poll()
end

local FinalFrozenCondition = class(FrozenCondition)
FinalFrozenCondition.__call = FrozenCondition.poll

return FinalFrozenCondition
