local GlobalCore
local function loadGlobalCore()
    if GlobalCore == nil then
        GlobalCore = require("GlobalCore")
    end
    return GlobalCore
end

local Transition = {}

function Transition.FreezeTransitionBackground()
    return loadGlobalCore().Transition.freezeTransitionBackground()
end

function Transition.RequestTransition(transitionName, transitionTime)
    transitionName = transitionName == nil and "" or transitionName
    transitionTime = transitionTime == nil and 1.0 or transitionTime
    return loadGlobalCore().Transition.requestTransition(transitionName, transitionTime)
end

return Transition
