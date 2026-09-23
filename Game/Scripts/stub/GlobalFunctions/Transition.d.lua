---@meta GlobalFunctions.Transition

local Transition = {}

---@brief Freeze the current frame and wait until it is ready for a transition.
---
--- - @return A condition callable that becomes True when the frame has been captured.
---@return GlobalFunctions.FrozenCondition
function Transition.FreezeTransitionBackground() end

---@brief Request a screen transition and wait until it finishes.
---
--- - @param transitionName Optional transition texture filename.
--- - @param transitionTime Transition duration in seconds.
--- - @return A condition callable that becomes True when the transition is finished.
---@param transitionName string
---@param transitionTime number
---@return GlobalFunctions.TransitionCondition
function Transition.RequestTransition(transitionName, transitionTime) end

return Transition
