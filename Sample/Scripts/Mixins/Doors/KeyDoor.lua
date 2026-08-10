local PlayerFunctions = require("Source.NodeFunctions.Player")

local KeyDoor = {}

KeyDoor.needKeyID = ""
KeyDoor.needKeyCount = 1

---@param self Mixins.Doors.KeyDoor
function KeyDoor.onCollision(self, other)
    if PlayerFunctions.MeetPlayer(other) == nil then
        return
    end
    if self.opening then
        return
    end
    if PlayerFunctions.GetItemCount(self.needKeyID) < self.needKeyCount then
        return
    end
    super().onCollision(other)
    PlayerFunctions.RemoveItem(self.needKeyID, self.needKeyCount)
end

return KeyDoor
