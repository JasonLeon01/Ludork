---@meta GlobalFunctions.TransitionCondition

---@class GlobalFunctions.TransitionCondition
---@field _started boolean
---@operator call: boolean
local TransitionCondition = {}

---@return GlobalFunctions.TransitionCondition
function TransitionCondition.new() end

function TransitionCondition:init() end

---@return boolean
function TransitionCondition:poll() end

---@return boolean
function TransitionCondition:isFinished() end

return TransitionCondition
