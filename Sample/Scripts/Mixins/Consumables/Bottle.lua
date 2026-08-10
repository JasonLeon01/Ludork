local Pickup = require("Source.Pickup")

local Bottle = {}

Bottle.HP_plus = 0
Bottle.getSE = ""

---@param self Mixins.Consumables.Bottle
function Bottle.onCollision(self, other)
    local parentCollision = super().onCollision
    Pickup.handleCollision(self, other, parentCollision, function (player)
        player.infoComp.HP = player.infoComp.HP + self.HP_plus
    end)
end

return Bottle
