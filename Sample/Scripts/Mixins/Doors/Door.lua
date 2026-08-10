local Door = {}

---@param self Mixins.Doors.Door
function Door.onCollision(self, _other)
    if self.opening then
        return
    end
    self:openDoor()
    self.opening = true
end

return Door
