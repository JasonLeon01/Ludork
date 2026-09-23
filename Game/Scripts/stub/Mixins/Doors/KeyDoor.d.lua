---@meta Mixins.Doors.KeyDoor
---@class (partial) Mixins.Doors.KeyDoor: Source.MapActors.DoorBase.DoorBase
---@field needKeyID    string
---@field needKeyCount integer
local KeyDoor = {}

---@param other Engine.Actor[]
function KeyDoor:onCollision(other) end

return KeyDoor
