local GlobalFunctions = require("GlobalFunctions")
local Pickup = require("Source.Pickup")

local ComponentsFunctions = GlobalFunctions.Components

local Gem = {}

Gem.ATTR_key = ""
Gem.plus = 0
Gem.getSE = ""

---@param self Mixins.Consumables.Gem
function Gem.onCollision(self, other)
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
