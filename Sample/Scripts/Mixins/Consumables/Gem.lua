local GlobalFunctions = require("GlobalFunctions")
local Pickup = require("Source.Pickup")

local ComponentsFunctions = GlobalFunctions.Components

---@class (partial) Mixins.Consumables.Gem
local Gem = {}

Gem.ATTR_key = ""
Gem.plus = 0
Gem.getSE = ""

function Gem:onCollision(other)
    local parentCollision = super().onCollision
    Pickup.handleCollision(self, other, parentCollision, function (player)
        local originAttr = ComponentsFunctions.getComponentFieldValue(player, self.ATTR_key, nil)
        if originAttr == nil then
            ---@type table
            local dynamicPlayer = player
            dynamicPlayer[self.ATTR_key] = dynamicPlayer[self.ATTR_key] + self.plus
        else
            ComponentsFunctions.setComponentFieldValue(player, self.ATTR_key, originAttr + self.plus)
        end
    end)
end

return Gem
