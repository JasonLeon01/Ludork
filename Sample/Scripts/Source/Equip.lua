local Engine = require("Engine")
local EquipInfo = require("Source.Infos.EquipInfo")
local Pickup = require("Source.Pickup")

local Actor = Engine.Actor

local Equip = {}

Equip.ID = "FILL_IT_BY_YOURSELF"
Equip.getSE = ""

function Equip:init(texture, rect, tag)
    local Data = require("Source.Data")

    ---@cast self Source.Equip
    Actor.init(self, texture, rect, tag)
    self:initInfo(Data)
end

function Equip:onCollision(other)
    ---@cast self Source.Equip
    local parentCollision = super(Equip, self).onCollision
    Pickup.handleInventoryCollision(self, other, parentCollision, function (player)
        player:addEquip(self.ID)
    end)
end

return class(Equip, Actor, EquipInfo)
