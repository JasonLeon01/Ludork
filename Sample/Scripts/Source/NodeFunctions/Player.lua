local Engine = require("Engine")
local Context = require("Source.NodeFunctions.Context")
local Effects = require("Source.Gameplay.Effects")

local Player = {}

function Player.GetPlayer()
    return Context.RequireGameInstance():getPlayer()
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
    if player.attributes:getAttributeSchema(attrName) ~= nil then
        return player.attributes[attrName]
    end
    return player[attrName]
end

function Player.SetPlayerAttr(attrName, value)
    local player = Player.GetPlayer()
    if player == nil then
        return
    end
    local schema = player.attributes:getAttributeSchema(attrName)
    if schema ~= nil then
        if schema.type == "int" or schema.type == "float" then
            player:getAbilitySystemComponent():setNumericAttributeBase(attrName, value)
        else
            player.attributes[attrName] = value
        end
    else
        player[attrName] = value
    end
end

function Player.GetPlayerAttrRef(attrName)
    local player = Player.GetPlayer()
    if player == nil then
        return nil
    end
    local NodeUtils = require("Source.NodeFunctions.Utils")

    if player.attributes:getAttributeSchema(attrName) ~= nil then
        return NodeUtils.GetAttrRef(player.attributes, attrName)
    end
    return NodeUtils.GetAttrRef(player, attrName)
end

function Player.HealPlayer(amount)
    amount = amount == nil and 1 or amount
    local player = Player.GetPlayer()
    if player ~= nil then
        Effects.ApplyInstantModifier(player, "Blueprint.Heal", "HP", "Add", amount)
    end
end

function Player.DamagePlayer(amount)
    amount = amount == nil and 1 or amount
    local player = Player.GetPlayer()
    if player ~= nil then
        Effects.ApplyInstantModifier(player, "Blueprint.Damage", "HP", "Add", -amount)
    end
end

function Player.AddHP(amount)
    amount = amount == nil and 1 or amount
    local player = Player.GetPlayer()
    if player ~= nil then
        Effects.ApplyInstantModifier(player, "Blueprint.AddHP", "HP", "Add", amount)
    end
end

function Player.AddGold(amount)
    amount = amount == nil and 1 or amount
    local player = Player.GetPlayer()
    if player ~= nil then
        Effects.ApplyInstantModifier(player, "Blueprint.AddGold", "GOLD", "Add", amount)
    end
end

function Player.AddATK(amount)
    amount = amount == nil and 1 or amount
    local player = Player.GetPlayer()
    if player ~= nil then
        Effects.ApplyInstantModifier(player, "Blueprint.AddATK", "ATK", "Add", amount)
    end
end

function Player.AddDEF(amount)
    amount = amount == nil and 1 or amount
    local player = Player.GetPlayer()
    if player ~= nil then
        Effects.ApplyInstantModifier(player, "Blueprint.AddDEF", "DEF", "Add", amount)
    end
end

function Player.AddEXP(amount)
    amount = amount == nil and 1 or amount
    local player = Player.GetPlayer()
    if player ~= nil then
        Effects.ApplyInstantModifier(player, "Blueprint.AddEXP", "EXP", "Add", amount)
    end
end

function Player.MeetPlayer(actors)
    local player = Player.GetPlayer()
    if player == nil then
        return nil
    end
    if table.contains(actors, player) then
        return player
    end
    return nil
end

return Player
