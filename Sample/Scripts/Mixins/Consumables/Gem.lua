local Pickup = require("Source.Pickup")
local Effects = require("Source.Gameplay.Effects")

---@class (partial) Mixins.Consumables.Gem
local Gem = {}

Gem.ATTR_key = ""
Gem.plus = 0
Gem.getSE = ""

function Gem:onCollision(other)
    local parentCollision = super().onCollision
    Pickup.HandleCollision(self, other, parentCollision, function (player)
        local schema = assert(
            player.attributes:getAttributeSchema(self.ATTR_key), "Gem attribute is not in the player AttributeSet"
        )
        assert(schema.type == "int" or schema.type == "float", "Gem attribute must be numeric")
        Effects.ApplyInstantModifier(player, "Consumable.Gem." .. self.ATTR_key, self.ATTR_key, "Add", self.plus)
    end)
end

return Gem
