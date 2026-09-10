---@class (partial) Mixins.Doors.Door
local Door = {}

function Door:onCollision(_other)
    ---@diagnostic disable-next-line: unnecessary-if
    if self.opening then
        return
    end
    self:openDoor()
    self.opening = true
end

return Door
