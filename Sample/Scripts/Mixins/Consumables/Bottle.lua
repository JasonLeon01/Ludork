local Pickup = require("Source.Pickup")
local Effects = require("Source.Gameplay.Effects")

---@class (partial) Mixins.Consumables.Bottle
local Bottle = {}

Bottle.HP_plus = 0
Bottle.getSE = ""

function Bottle:onCollision(other)
    local parentCollision = super().onCollision
    Pickup.HandleCollision(self, other, parentCollision, function (player)
        Effects.ApplyInstantModifier(player, "Consumable.Bottle.Heal", "HP", "Add", self.HP_plus)
    end)
end

return Bottle
