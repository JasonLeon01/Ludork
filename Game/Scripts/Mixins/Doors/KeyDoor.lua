local Player = require("Source.MapActors.Player")

---@class (partial) Mixins.Doors.KeyDoor
local KeyDoor = {}

KeyDoor.needKeyID = ""
KeyDoor.needKeyCount = 1

function KeyDoor:onCollision(other)
    local gameMap = self:getMap()
    if gameMap == nil then
        return
    end
    ---@cast gameMap GameMap
    local player = Player.MeetPlayer(other, gameMap:getPlayer())
    if player == nil then
        return
    end
    if self.opening then
        return
    end
    if player:getItemCount(self.needKeyID) < self.needKeyCount then
        return
    end
    super().onCollision(other)
    player:removeItem(self.needKeyID, self.needKeyCount)
end

return KeyDoor
