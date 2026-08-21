local PlayerFunctions = require("Source.NodeFunctions.Player")

---@class (partial) Mixins.Doors.KeyDoor
local KeyDoor = {}

KeyDoor.needKeyID = ""
KeyDoor.needKeyCount = 1

function KeyDoor:onCollision(other)
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
