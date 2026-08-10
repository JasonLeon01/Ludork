local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local Context = require("Source.NodeFunctions.Context")

local ComponentsFunctions = GlobalFunctions.Components
local Player = {}

function Player.GetPlayer()
    return Context.requireGameInstance():getPlayer()
end

function Player.GetPlayerFrontPosition()
    local player = Player.GetPlayer()
    if player == nil then
        return nil
    end
    local position = player:getMapPosition()
    local direction = player.direction
    local x = position.x
    local y = position.y
    if direction == Engine.Direction.UP then
        y = y - 1
    elseif direction == Engine.Direction.LEFT then
        x = x - 1
    elseif direction == Engine.Direction.RIGHT then
        x = x + 1
    else
        y = y + 1
    end
    ---@cast x integer
    ---@cast y integer
    return sf.Vector2i.new(x, y)
end

function Player.AddItem(itemID, count)
    count = count == nil and 1 or count
    local player = Player.GetPlayer()
    if player ~= nil then
        player:addItem(itemID, count)
    end
end

function Player.RemoveItem(itemID, count)
    count = count == nil and 1 or count
    local player = Player.GetPlayer()
    if player ~= nil and player:removeItem(itemID, count) then
        return 0
    end
    return 1
end

function Player.HasItem(itemID)
    local player = Player.GetPlayer()
    return bool(player) and player:hasItem(itemID)
end

function Player.GetItemCount(itemID)
    local player = Player.GetPlayer()
    return player ~= nil and player:getItemCount(itemID) or 0
end

function Player.AddEquip(equipID, count)
    count = count == nil and 1 or count
    local player = Player.GetPlayer()
    if player ~= nil then
        player:addEquip(equipID, count)
    end
end

function Player.RemoveEquip(equipID, count)
    count = count == nil and 1 or count
    local player = Player.GetPlayer()
    if player ~= nil and player:removeEquip(equipID, count) then
        return 0
    end
    return 1
end

function Player.HasEquip(equipID)
    local player = Player.GetPlayer()
    return bool(player) and player:hasEquip(equipID)
end

function Player.EquipItem(equipID)
    local player = Player.GetPlayer()
    if player ~= nil then
        player:equip(equipID)
    end
end

function Player.UnequipSlot(slotID)
    local player = Player.GetPlayer()
    if player ~= nil then
        player:unequip(slotID)
    end
end

function Player.GetEquipInSlot(slotID)
    local player = Player.GetPlayer()
    return player ~= nil and player:getEquipInfo(slotID) or ""
end

function Player.GetPlayerAttr(attrName)
    local player = Player.GetPlayer()
    if player == nil then
        return nil
    end
    local value = ComponentsFunctions.getComponentFieldValue(player, attrName, Class.MISSING)
    if value ~= Class.MISSING then
        return value
    end
    return player[attrName]
end

function Player.SetPlayerAttr(attrName, value)
    local player = Player.GetPlayer()
    if player ~= nil and not ComponentsFunctions.setComponentFieldValue(player, attrName, value) then
        player[attrName] = value
    end
end

function Player.GetPlayerAttrRef(attrName)
    local player = Player.GetPlayer()
    if player == nil then
        return nil
    end
    local NodeUtils = require("Source.NodeFunctions.Utils")

    return NodeUtils.GetAttrRef(player, attrName)
end

function Player.HealPlayer(amount)
    amount = amount == nil and 1 or amount
    local player = Player.GetPlayer()
    if player ~= nil then
        player.infoComp.HP = player.infoComp.HP + Engine.ToInteger(amount)
    end
end

function Player.DamagePlayer(amount)
    amount = amount == nil and 1 or amount
    local player = Player.GetPlayer()
    if player ~= nil then
        local hp = player.infoComp.HP - Engine.ToInteger(amount)
        ---@cast hp integer
        player.infoComp.HP = hp
    end
end

function Player.RemovePlayerState(stateID)
    local player = Player.GetPlayer()
    if player ~= nil then
        player:removeState(stateID)
    end
end

function Player.ReducePlayerState(stateID, stacks)
    stacks = stacks == nil and 1 or stacks
    local player = Player.GetPlayer()
    if player ~= nil then
        player:reduceStateStacks(stateID, stacks)
    end
end

function Player.AddHP(amount)
    amount = amount == nil and 1 or amount
    local player = Player.GetPlayer()
    if player ~= nil then
        player.infoComp.HP = player.infoComp.HP + Engine.ToInteger(amount)
    end
end

function Player.AddGold(amount)
    amount = amount == nil and 1 or amount
    local player = Player.GetPlayer()
    if player ~= nil then
        player.infoComp.GOLD = player.infoComp.GOLD + Engine.ToInteger(amount)
    end
end

function Player.AddATK(amount)
    amount = amount == nil and 1 or amount
    local player = Player.GetPlayer()
    if player ~= nil then
        player.infoComp.ATK = player.infoComp.ATK + Engine.ToInteger(amount)
    end
end

function Player.AddDEF(amount)
    amount = amount == nil and 1 or amount
    local player = Player.GetPlayer()
    if player ~= nil then
        player.infoComp.DEF = player.infoComp.DEF + Engine.ToInteger(amount)
    end
end

function Player.AddEXP(amount)
    amount = amount == nil and 1 or amount
    local player = Player.GetPlayer()
    if player ~= nil then
        player.infoComp.EXP = player.infoComp.EXP + Engine.ToInteger(amount)
    end
end

function Player.MeetPlayer(actors)
    local player = Player.GetPlayer()
    if player == nil then
        return nil
    end
    for _, actor in ipairs(actors) do
        if actor == player then
            return player
        end
    end
    return nil
end

return Player
