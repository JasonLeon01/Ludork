---@meta Mixins.Doors.ConditionDoor
---@class Mixins.Doors.ConditionDoor: Source.DoorBase.DoorBase
---@field openConditionName string
---@field openConditionVal integer
---@field _conditionDoorPending boolean
local ConditionDoor = {}

function ConditionDoor:onCreate() end

function ConditionDoor:onDestroy() end

---@param deltaTime number
function ConditionDoor:onTick(deltaTime) end

return ConditionDoor
