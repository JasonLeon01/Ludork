local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local Data = require("Source.Data")
local Battler = require("Source.Battler")
local PlayerInfoComponent = require("Source.Components.PlayerInfoComponent")
local EquipInfo = require("Source.Infos.EquipInfo")
local PlayerInfo = require("Source.Infos.PlayerInfo")

local Character = Engine.Character
local Input = Engine.Input
local ComponentsFunctions = GlobalFunctions.Components

local LEVEL_HP_GAIN = 400
local LEVEL_ATK_GAIN = 2
local LEVEL_DEF_GAIN = 2

---@class Source.Player.Player
local Player = {}

Player.ID = "FILL_IT_BY_YOURSELF"
Player.tickable = true
Player.collisionEnabled = true
Player.animatable = true
Player.speed = 96.0
Player._componentTypes = { infoComp = PlayerInfoComponent }
Player.infoComp = PlayerInfoComponent.new()

function Player:init(texture, tag)
    Character.init(self, texture, tag or "")
    Battler.init(self)
    self._loading = true
    self:initInfo(Data)
    self:_syncInitialHP()
    self._loading = false

    Class.monitor(self.infoComp, "LEVEL", function (old, new)
        local oldValue = old == Class.MISSING and 0 or old
        ---@cast oldValue integer
        ---@cast new integer
        local delta = new - oldValue
        ---@cast delta integer
        self.infoComp.HP = self.infoComp.HP + delta * LEVEL_HP_GAIN
        self.infoComp.ATK = self.infoComp.ATK + delta * LEVEL_ATK_GAIN
        self.infoComp.DEF = self.infoComp.DEF + delta * LEVEL_DEF_GAIN
    end, {}
    )
    self._items = {}
    self._equips = {}
    self._equipInfo = {}
    self._classPath = ""
    self._forbiddenMoving = false
    self._wasMovingOnLastFixedTick = false
    self._movementSpecialPath = {}
    local classData = Data.getGeneralClassData(self.infoComp.CLASS)
    for _, equipID in pairs(classData.slot or {}) do
        if bool(equipID) then
            self:equip(equipID)
        end
    end
end

function Player:_onArrivedAtMapCell()
    local position = self:getMapPosition()
    self._movementSpecialPath[#self._movementSpecialPath + 1] = copy(position)
end

function Player:consumeMovementSpecialPath()
    local path = self._movementSpecialPath
    self._movementSpecialPath = {}
    return path
end

function Player:getClassPath()
    return self._classPath
end

function Player:setClassPath(classPath)
    self._classPath = classPath
end

function Player:onFixedTick(_fixedDelta)
    if bool(self._movementSpecialPath) then
        local MovementSpecials = require("Source.MovementSpecials")

        MovementSpecials.notifyPlayerMovementFinished(self, self:consumeMovementSpecialPath())
    end
    if self._wasMovingOnLastFixedTick and not self:isMoving() then
        self:triggerStateWalk()
    end
    self._wasMovingOnLastFixedTick = self:isMoving()
end

---@return sf.Vector2i | nil
function Player:_getContinueMoveOffset()
    if self:isInRoute() then
        return nil
    end
    if self:getForbiddenMoving() or self:_isSceneInputBlocked() then
        return nil
    end
    return Player._getHeldKeyboardMoveOffset()
end

---@return sf.Vector2i | nil
function Player._getHeldKeyboardMoveOffset()
    if Input.isActionHeld(Input.getUpKeys()) then
        local offset = sf.Vector2i.new(0, -1)
        ---@cast offset sf.Vector2i
        return offset
    end
    if Input.isActionHeld(Input.getDownKeys()) then
        local offset = sf.Vector2i.new(0, 1)
        ---@cast offset sf.Vector2i
        return offset
    end
    if Input.isActionHeld(Input.getLeftKeys()) then
        local offset = sf.Vector2i.new(-1, 0)
        ---@cast offset sf.Vector2i
        return offset
    end
    if Input.isActionHeld(Input.getRightKeys()) then
        local offset = sf.Vector2i.new(1, 0)
        ---@cast offset sf.Vector2i
        return offset
    end
    return nil
end

function Player:asDict()
    local position = self:getMapPosition()
    return {
        playerClass = self._classPath,
        tag = self.tag,
        position = { position.x, position.y },
        attr = {
            LEVEL = self.infoComp.LEVEL,
            HP = self.infoComp.HP,
            MAXHP = self.infoComp.MAXHP,
            ATK = self.infoComp.ATK,
            DEF = self.infoComp.DEF,
            EXP = self.infoComp.EXP,
            GOLD = self.infoComp.GOLD
        },
        items = self._items,
        equips = self._equips,
        equipInfo = self._equipInfo,
        states = self:getStateStacks()
    }
end

function Player.InitPlayer(playerPath)
    local actor = assert(Data.genActorFromClassPath(playerPath, "PLAYER"), "Player blueprint class is missing")
    ---@cast actor Source.Player.Player
    actor:setClassPath(playerPath)
    actor:setAnimatable(true, true)
    actor:setCollisionEnabled(true)
    assert(actor:hasGraph(), "Player blueprint graph is missing")
    return actor
end

function Player.FromDict(data)
    local player = Player.InitPlayer(data.playerClass)
    player.tag = data.tag
    local positionX = data.position[1]
    local positionY = data.position[2]
    ---@cast positionX integer
    ---@cast positionY integer
    local position = sf.Vector2u.new(positionX, positionY)
    ---@cast position sf.Vector2u
    player:setMapPosition(position)
    for key, value in pairs(data.attr) do
        if not ComponentsFunctions.setComponentFieldValue(player, key, value) then
            player[key] = value
        end
    end
    player._items = data.items
    player._equips = data.equips
    player._equipInfo = data.equipInfo
    local states = data.states or {}
    if #states > 0 then
        ---@cast states string[]
        player:setStateIDs(states)
    else
        ---@cast states table<string, integer>
        player:setStateStacks(states)
    end
    return player
end

function Player:addItem(itemID, count)
    local itemCount = count == nil and 1 or count
    ---@cast itemCount integer
    self._items[itemID] = (self._items[itemID] or 0) + itemCount
end

function Player:removeItem(itemID, count)
    local itemCount = count == nil and 1 or count
    ---@cast itemCount integer
    if self._items[itemID] == nil or self._items[itemID] < itemCount then
        return false
    end
    self._items[itemID] = self._items[itemID] - itemCount
    if self._items[itemID] == 0 then
        self._items[itemID] = nil
    end
    return true
end

function Player:getItemCount(itemID)
    return self._items[itemID] or 0
end

function Player:hasItem(itemID)
    return self._items[itemID] ~= nil and self._items[itemID] > 0
end

function Player:addEquip(equipID, count)
    local equipCount = count == nil and 1 or count
    ---@cast equipCount integer
    self._equips[equipID] = (self._equips[equipID] or 0) + equipCount
end

function Player:removeEquip(equipID, count)
    local equipCount = count == nil and 1 or count
    ---@cast equipCount integer
    if self._equips[equipID] == nil or self._equips[equipID] < equipCount then
        return false
    end
    self._equips[equipID] = self._equips[equipID] - equipCount
    if self._equips[equipID] == 0 then
        self._equips[equipID] = nil
    end
    return true
end

function Player:equip(equipID)
    local equipData = Data.getGeneralEquipData(equipID)
    local classData = Data.getGeneralClassData(self.infoComp.CLASS)
    local slot = equipData.slot or ""
    if classData.slot[slot] == nil then
        error("Equip " .. tostring(equipID) .. " is not in the player's class")
    end
    local currentID = self._equipInfo[slot] or ""
    if bool(currentID) and currentID ~= equipID then
        self:unequip(slot)
    end
    self:_updateEquipInfo(slot, equipID)
    self:_applyEquipAttributeChanges(equipData.attrPlus, 1)
    local info = EquipInfo.new()
    info.ID = equipID
    info:initInfo(Data)
    info:triggerEvent("onEquip")
    self:removeEquip(equipID)
end

function Player:unequip(slotID)
    local equipID = self._equipInfo[slotID] or ""
    if not bool(equipID) then
        return
    end
    self:_updateEquipInfo(slotID, "")
    local equipData = Data.getGeneralEquipData(equipID)
    self:_applyEquipAttributeChanges(equipData.attrPlus, -1)
    local info = EquipInfo.new()
    info.ID = equipID
    info:initInfo(Data)
    info:triggerEvent("onUnequip")
    self:addEquip(equipID)
end

function Player:getEquipCount(equipID)
    return self._equips[equipID] or 0
end

function Player:hasEquip(equipID)
    return self._equips[equipID] ~= nil and self._equips[equipID] > 0
end

function Player:getEquipInfo(slotID)
    return self._equipInfo[slotID] or ""
end

function Player:getForbiddenMoving()
    return self._forbiddenMoving
end

function Player:setForbiddenMoving(value)
    self._forbiddenMoving = value
end

---@return boolean
function Player:_isSceneInputBlocked()
    local gameMap = self:getMap()
    if gameMap == nil then
        return false
    end
    ---@cast gameMap GameMap
    local scene = gameMap:getScene()
    return scene ~= nil and scene:isInputBlocked()
end

---@param slot    string
---@param equipID string
function Player:_updateEquipInfo(slot, equipID)
    local result = {}
    local classData = Data.getGeneralClassData(self.infoComp.CLASS)
    for slotName in pairs(classData.slot or {}) do
        result[slotName] = slotName == slot and equipID or self._equipInfo[slotName] or ""
    end
    self._equipInfo = result
end

---@param attrPlus   table<string, string|number>
---@param multiplier integer
function Player:_applyEquipAttributeChanges(attrPlus, multiplier)
    for attrKey, attrValue in pairs(attrPlus) do
        local delta = assert(tonumber(attrValue), "Invalid equip attribute value: " .. tostring(attrKey)) * multiplier
        if self.infoComp[attrKey] ~= nil then
            self.infoComp[attrKey] = self.infoComp[attrKey] + delta
        else
            local currentValue = assert(
                tonumber(self[attrKey]), "Equip attribute target is missing or not numeric: " .. tostring(attrKey)
            )
            self[attrKey] = currentValue + delta
        end
    end
end

return class(Player, Character, PlayerInfo, Battler)
