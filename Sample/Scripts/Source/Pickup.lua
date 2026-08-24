local GlobalFunctions = require("GlobalFunctions")
local PlayerFunctions = require("Source.NodeFunctions.Player")
local System = require("Source.System")

local ManagerFunctions = GlobalFunctions.Manager

local Pickup = {}

local function playPickupSound(actor)
    local getSE = actor.getSE
    if not bool(getSE) then
        getSE = System.GetGetSE()
    end
    ManagerFunctions.playSE(getSE)
end

local function showNewItemMessage(actor, inst, scene)
    if inst:getCachedNewItem(actor.ID) then
        return
    end
    inst:setCachedNewItem(actor.ID)
    scene:showMessage("", "ITEM_NEW", nil, {
        name = actor.name,
        desc = actor.desc
    })
end

function Pickup.HandleCollision(actor, other, parentCollision, applyPickup)
    if actor:isDestroyed() then
        return
    end
    local player = PlayerFunctions.MeetPlayer(other)
    if player == nil then
        return
    end
    local scene = actor:getMap():getScene()
    local inst = scene.inst
    playPickupSound(actor)
    applyPickup(player, inst, scene)
    parentCollision(other)
    scene:recordDestroyedActor(actor)
    actor:destroy()
end

function Pickup.HandleInventoryCollision(actor, other, parentCollision, applyPickup)
    Pickup.HandleCollision(actor, other, parentCollision, function (player, inst, scene)
        applyPickup(player, inst, scene)
        showNewItemMessage(actor, inst, scene)
    end)
end

return Pickup
