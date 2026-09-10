local GlobalCore = require("GlobalCore")
local Player = require("Source.Player")
local System = require("Source.System")

local AudioManager = GlobalCore.AudioManager

local Pickup = {}

local function playPickupSound(actor)
    local getSE = actor.getSE
    if not bool(getSE) then
        getSE = System.GetGetSE()
    end
    AudioManager.playSound(getSE)
end

local function showNewItemMessage(actor, inst, scene)
    if inst:getCachedNewItem(actor.ID) then
        return
    end
    inst:setCachedNewItem(actor.ID)
    scene:showMessage("", "ITEM_NEW", nil, {
        name = actor.attributes.name,
        desc = actor.attributes.desc
    })
end

function Pickup.HandleCollision(actor, other, parentCollision, applyPickup)
    if actor:isDestroyed() or not actor:isVisibleInHierarchy() then
        return
    end
    local gameMap = actor:getMap()
    if gameMap == nil then
        return
    end
    local player = Player.MeetPlayer(other, gameMap:getPlayer())
    if player == nil then
        return
    end
    local scene = gameMap:getScene()
    local inst = scene:getGameInstance()
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
