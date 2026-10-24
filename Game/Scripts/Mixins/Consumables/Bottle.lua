local GlobalCore = require("GlobalCore")
local Pickup = require("Source.Utils.Pickup")
local Effects = require("Source.Gameplay.Effects")

---@class (partial) Mixins.Consumables.Bottle
local Bottle = {}

Bottle.HP_plus = 0
Bottle.getSE = ""

function Bottle:onCollision(other)
    local parentCollision = super().onCollision
    Pickup.HandleCollision(self, other, parentCollision, function (player)
        Effects.ApplyInstantModifier(
            player, "Consumable.Bottle.Heal", "HP", GlobalCore.GameplayModifier.OPERATION_ADD, self.HP_plus
        )
    end)
end

return Bottle
