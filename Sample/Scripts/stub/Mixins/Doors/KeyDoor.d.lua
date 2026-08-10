---@meta Mixins.Doors.KeyDoor
---@class Mixins.Doors.KeyDoor: Source.DoorBase.DoorBase
---@field needKeyID string
---@field needKeyCount integer
local KeyDoor = {}

---@param other Engine.Actor[]
function KeyDoor:onCollision(other) end

return KeyDoor
