local Engine = require("Engine")
local ItemInfo = require("Source.Infos.ItemInfo")
local Pickup = require("Source.Pickup")

local Actor = Engine.Actor

local Item = {}

Item.ID = ""
Item.count = 1
Item.getSE = ""

function Item:init(texture, rect, tag)
    local Data = require("Source.Data")

    ---@cast self Source.Item
    Actor.init(self, texture, rect, tag)
    self:initInfo(Data)
end

function Item:onCollision(other)
    ---@cast self Source.Item
    local parentCollision = super(Item, self).onCollision
    Pickup.HandleInventoryCollision(self, other, parentCollision, function (player)
        player:addItem(self.ID, self.count)
    end)
end

return class(Item, Actor, ItemInfo)
