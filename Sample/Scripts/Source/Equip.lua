local Engine = require("Engine")
local Data = require("Source.Data")
local Pickup = require("Source.Pickup")

local Actor = Engine.Actor

local Equip = {}

Equip.ID = "FILL_IT_BY_YOURSELF"
Equip.getSE = ""

function Equip:init(texture, rect, tag)
    ---@cast self Source.Equip
    Actor.init(self, texture, rect, tag)
    self.attributes = Data.CreateGeneralAttributeSet("Equip", self.ID)
end

function Equip:onCollision(other)
    ---@cast self Source.Equip
    local parentCollision = super(Equip, self).onCollision
    Pickup.HandleInventoryCollision(self, other, parentCollision, function (player)
        player:addEquip(self.ID)
    end)
end

return class(Equip, Actor)
