---@meta Mixins.Doors.Door
---@class Mixins.Doors.Door: Source.DoorBase.DoorBase
local Door = {}

---@param other Engine.Actor[]
function Door:onCollision(other) end

return Door
