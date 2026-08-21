---@meta Mixins.Doors.Door
---@class (partial) Mixins.Doors.Door: Source.DoorBase.DoorBase
local Door = {}

---@param other Engine.Actor[]
function Door:onCollision(other) end

return Door
