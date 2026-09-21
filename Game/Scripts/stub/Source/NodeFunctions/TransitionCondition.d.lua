---@meta Source.NodeFunctions.TransitionCondition

---@class Source.NodeFunctions.TransitionCondition
---@field _started boolean
---@operator call: boolean
local TransitionCondition = {}

---@return Source.NodeFunctions.TransitionCondition
function TransitionCondition.new() end

function TransitionCondition:init() end

---@return boolean
function TransitionCondition:poll() end

---@return boolean
function TransitionCondition:isFinished() end

return TransitionCondition
