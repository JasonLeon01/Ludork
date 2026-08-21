local Pickup = require("Source.Pickup")

---@class (partial) Mixins.Consumables.Bottle
local Bottle = {}

Bottle.HP_plus = 0
Bottle.getSE = ""

function Bottle:onCollision(other)
    local parentCollision = super().onCollision
    Pickup.handleCollision(self, other, parentCollision, function (player)
        player.infoComp.HP = player.infoComp.HP + self.HP_plus
    end)
end

return Bottle
