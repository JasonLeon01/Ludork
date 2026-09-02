local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local Data = require("Source.Data")
local Battler = require("Source.Battler")
local GameplayEffectSpec = GlobalCore.GameplayEffectSpec
local GameplayEventData = GlobalCore.GameplayEventData
local Effects = require("Source.Gameplay.Effects")
local GeneralDataGraphAbility = require("Source.Gameplay.GeneralDataGraphAbility")

local Character = Engine.Character
local Input = Engine.Input

local LEVEL_HP_GAIN = 400
local LEVEL_ATK_GAIN = 2
local LEVEL_DEF_GAIN = 2

---@param _old   integer | Class.MissingValue
---@param _new   integer | Class.MissingValue
---@param change { attribute: string, source: string, oldBase: number, newBase: number }
---@param player Source.Player.Player
local function onHPChange(_old, _new, change, player)
    if player == nil then
        return
    end
    if change.source ~= "Base" then
        return
    end
    local base = player:getAbilitySystemComponent():getNumericAttributeBase("HP")
    if base < 0 then
        player:getAbilitySystemComponent():setNumericAttributeBase("HP", 0)
    elseif base > player.attributes.MAXHP then
        player:getAbilitySystemComponent():setNumericAttributeBase("HP", player.attributes.MAXHP)
    end
end

---@param _old   integer | Class.MissingValue
---@param _new   integer | Class.MissingValue
---@param change { attribute: string, source: string, oldBase: number, newBase: number }
---@param player Source.Player.Player
local function onMAXHPChange(_old, _new, change, player)
    if player == nil then
        return
    end
    if change.source ~= "Base" then
        return
    end
    local abilitySystem = player:getAbilitySystemComponent()
    if abilitySystem:getNumericAttributeBase("HP") > player.attributes.MAXHP then
        abilitySystem:setNumericAttributeBase("HP", player.attributes.MAXHP)
    end
    if not player._loading then
        local oldBase = change.oldBase == Class.MISSING and 0 or change.oldBase
        local newBase = change.newBase
        ---@cast oldBase integer
        ---@cast newBase integer
        local delta = newBase - oldBase
        ---@cast delta integer
        if delta > 0 then
            abilitySystem:setNumericAttributeBase("HP", abilitySystem:getNumericAttributeBase("HP") + delta)
        end
    end
end

local function constrainHP(value, abilitySystem, resolvedValues)
    local maxHP = resolvedValues.MAXHP or abilitySystem:getNumericAttribute("MAXHP")
    return math.max(0, math.min(value, maxHP))
end

---@return GlobalCore.GameplayEventData
local function createPlayerEvent(player, eventTag, payload)
    return GameplayEventData.new(player, player, eventTag, payload or {})
end

---@type function
local getHeldKeyboardMoveOffset
---@class Source.Player.Player
local Player = {}

Player.ID = "FILL_IT_BY_YOURSELF"
Player.tickable = true
Player.collisionEnabled = true
Player.animatable = true
Player.speed = 96.0

function Player:init(texture, tag)
    Character.init(self, texture, tag or "")
    self._loading = true
    local attributes = Data.CreateGeneralAttributeSet("Player", self.ID)
    Battler.init(self, attributes)
    local abilitySystem = self:getAbilitySystemComponent()
    local listenerParams = setmetatable({ self }, { __mode = "v" })
    abilitySystem:setNumericAttributeConstraint("HP", constrainHP)
    abilitySystem:addAttributeChangeListener("HP", onHPChange, listenerParams)
    abilitySystem:addAttributeChangeListener("MAXHP", onMAXHPChange, listenerParams)
    abilitySystem:addAttributeChangeListener("LEVEL", function (_old, _new, change)
        if self._loading or change.source ~= "Base" then
            return
        end
        local oldBase = change.oldBase == Class.MISSING and 0 or change.oldBase
        local newBase = change.newBase
        ---@cast oldBase integer
        ---@cast newBase integer
        local delta = newBase - oldBase
        ---@cast delta integer
        local playerAbilitySystem = self:getAbilitySystemComponent()
        playerAbilitySystem:setNumericAttributeBase(
            "HP", playerAbilitySystem:getNumericAttributeBase("HP") + delta * LEVEL_HP_GAIN
        )
        playerAbilitySystem:setNumericAttributeBase(
            "ATK", playerAbilitySystem:getNumericAttributeBase("ATK") + delta * LEVEL_ATK_GAIN
        )
        playerAbilitySystem:setNumericAttributeBase(
            "DEF", playerAbilitySystem:getNumericAttributeBase("DEF") + delta * LEVEL_DEF_GAIN
        )
    end)
    self._items = {}
    self._equips = {}
    self._equipInfo = {}
    self._equipEffectHandles = {}
    self._classPath = ""
    self._forbiddenMoving = false
    self._wasMovingOnLastFixedTick = false
    self._movementSpecialPath = {}
    self._loading = false
end

function Player:_applyInitialEquipment()
    local classData = Data.GetGeneralClassData(self.attributes.CLASS)
    for _, slot in ipairs(table.orderedStringKeys(classData.slot or {})) do
        local equipID = classData.slot[slot]
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
    local path = {}
    path, self._movementSpecialPath = self._movementSpecialPath, path
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

        MovementSpecials.NotifyPlayerMovementFinished(self, self:consumeMovementSpecialPath())
    end
    if self._wasMovingOnLastFixedTick and not self:isMoving() then
        self:getAbilitySystemComponent():handleGameplayEvent(createPlayerEvent(self, "Event.Movement.Step"))
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
    return getHeldKeyboardMoveOffset()
end

---@return sf.Vector2i | nil
function getHeldKeyboardMoveOffset()
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
    local bases = self:getAbilitySystemComponent():getNumericAttributeBases()
    return {
        playerClass = self._classPath,
        tag = self.tag,
        position = { position.x, position.y },
        attr = {
            LEVEL = bases.LEVEL,
            HP = bases.HP,
            MAXHP = bases.MAXHP,
            ATK = bases.ATK,
            DEF = bases.DEF,
            EXP = bases.EXP,
            GOLD = bases.GOLD
        },
        items = self._items,
        equips = self._equips,
        equipInfo = self._equipInfo,
        states = Effects.GetStateStacks(self)
    }
end

function Player.InitPlayer(playerPath, applyInitialEquipment)
    local actor = assert(Data.GenActorFromClassPath(playerPath, "PLAYER"), "Player blueprint class is missing")
    ---@cast actor Source.Player.Player
    actor:setClassPath(playerPath)
    actor:setAnimatable(true, true)
    actor:setCollisionEnabled(true)
    assert(actor:hasGraph(), "Player blueprint graph is missing")
    if applyInitialEquipment ~= false then
        actor:_applyInitialEquipment()
    end
    return actor
end

function Player.FromDict(data)
    local player = Player.InitPlayer(data.playerClass, false)
    player.tag = data.tag
    local positionX = data.position[1]
    local positionY = data.position[2]
    ---@cast positionX integer
    ---@cast positionY integer
    local position = sf.Vector2u.new(positionX, positionY)
    ---@cast position sf.Vector2u
    player:setMapPosition(position)
    player._loading = true
    player:_clearEquipmentEffects()
    Effects.ClearStates(player)
    player:getAbilitySystemComponent():setNumericAttributeBases(data.attr)
    player._items = deepcopy(data.items)
    player._equips = deepcopy(data.equips)
    player._equipInfo = {}
    player._equipEffectHandles = {}
    for _, itemID in ipairs(table.orderedStringKeys(player._items)) do
        player:_syncItemAbility(itemID)
    end
    for _, slot in ipairs(table.orderedStringKeys(data.equipInfo)) do
        local equipID = data.equipInfo[slot]
        if bool(equipID) then
            player:_setEquippedItem(slot, equipID, false)
        end
    end
    for _, stateID in ipairs(table.orderedStringKeys(data.states)) do
        local stacks = data.states[stateID]
        Effects.ApplyState(player, stateID, stacks, createPlayerEvent(player, "Event.State.Restore"))
    end
    player._loading = false
    return player
end

function Player:_syncItemAbility(itemID)
    local abilitySystem = self:getAbilitySystemComponent()
    local sourceKey = "Item." .. itemID
    abilitySystem:removeAbilitiesBySource(sourceKey)
    if self:getItemCount(itemID) > 0 then
        abilitySystem:giveAbility(GeneralDataGraphAbility.new("Item", itemID, "onUse"), sourceKey)
    end
end

function Player:addItem(itemID, count)
    local itemCount = count == nil and 1 or count
    ---@cast itemCount integer
    self._items[itemID] = (self._items[itemID] or 0) + itemCount
    self:_syncItemAbility(itemID)
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
    self:_syncItemAbility(itemID)
    return true
end

function Player:activateItem(itemID)
    return self
        :getAbilitySystemComponent()
        :tryActivateAbility("GeneralData.Item." .. itemID .. ".onUse", createPlayerEvent(
            self, "Event.Item.Use", { itemID = itemID }
        ))
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

function Player:_executeEquipGraph(equipID, graphEvent)
    local ability = GeneralDataGraphAbility.new("Equip", equipID, graphEvent)
    local eventTag = graphEvent == "onEquip" and "Event.Equipment.Equip" or "Event.Equipment.Unequip"
    return ability:activate(self:getAbilitySystemComponent(), createPlayerEvent(self, eventTag, { equipID = equipID }))
end

function Player:_setEquippedItem(slot, equipID, executeGraph)
    local equipData = Data.GetGeneralEquipData(equipID)
    local effect = Effects.CreateEquipmentEffect(equipID, slot, equipData.attrPlus)
    local eventData = createPlayerEvent(self, "Event.Equipment.Equip", { equipID = equipID, slot = slot })
    local handle = self
        :getAbilitySystemComponent()
        :applyGameplayEffectSpec(GameplayEffectSpec.new(effect, eventData, 1, "Equipment." .. slot))
    self._equipInfo[slot] = equipID
    self._equipEffectHandles[slot] = handle
    if executeGraph then
        self:_executeEquipGraph(equipID, "onEquip")
    end
end

function Player:equip(equipID)
    local equipData = Data.GetGeneralEquipData(equipID)
    local classData = Data.GetGeneralClassData(self.attributes.CLASS)
    local slot = equipData.slot or ""
    if classData.slot[slot] == nil then
        error("Equip " .. tostring(equipID) .. " is not in the player's class")
    end
    local currentID = self:getEquipInfo(slot)
    if currentID == equipID then
        return
    end
    if bool(currentID) then
        self:unequip(slot)
    end
    self:_setEquippedItem(slot, equipID, true)
    self:removeEquip(equipID)
end

function Player:unequip(slotID)
    local equipID = self:getEquipInfo(slotID)
    if not bool(equipID) then
        return
    end
    assert(self._equipEffectHandles[slotID] ~= nil, "Equipped item is missing its Gameplay Effect")
    self:getAbilitySystemComponent():removeActiveGameplayEffect(self._equipEffectHandles[slotID])
    self._equipInfo[slotID] = ""
    self._equipEffectHandles[slotID] = nil
    self:_executeEquipGraph(equipID, "onUnequip")
    self:addEquip(equipID)
end

function Player:_clearEquipmentEffects()
    for _, handle in pairs(self._equipEffectHandles) do
        self:getAbilitySystemComponent():removeActiveGameplayEffect(handle)
    end
    self._equipInfo = {}
    self._equipEffectHandles = {}
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

return class(Player, Character, Battler)
