---@meta Source.NodeFunctions.FrozenCondition

---@class Source.NodeFunctions.FrozenCondition
---@operator call: boolean
local FrozenCondition = {}

---@return Source.NodeFunctions.FrozenCondition
function FrozenCondition.new() end

---@return boolean
function FrozenCondition:poll() end

---@return boolean
function FrozenCondition:isFinished() end

return FrozenCondition
