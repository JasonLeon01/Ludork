---@meta GlobalFunctions.FrozenCondition

---@class GlobalFunctions.FrozenCondition
---@operator call: boolean
local FrozenCondition = {}

---@return GlobalFunctions.FrozenCondition
function FrozenCondition.new() end

---@return boolean
function FrozenCondition:poll() end

---@return boolean
function FrozenCondition:isFinished() end

return FrozenCondition
