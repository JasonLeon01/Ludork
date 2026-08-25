local GlobalFunctions = require("GlobalFunctions")
local Data = require("Source.Data")
local GameSystem = require("Source.System")
local IconTexture = require("Source.UI.IconTexture")
local EquipItemRowUI = require("Source.UI.Parts.WindowEquip.EquipItemRow")
local ListViewController = require("Source.UI.Helpers.ListView")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local ManagerFunctions = GlobalFunctions.Manager

local _EQUIP_CELL_SIZE = 32
local _EQUIP_ORDER = { "Sword_A", "Shield_A" }
local WindowEquipSelectController = {}

WindowEquipSelectController.UNEQUIP = {}

function WindowEquipSelectController:init(model)
    self._logicalSize = nil
    self._rowUIs = {}
    super(WindowEquipSelectController, self).init(model, sf.Vector2u.new(1, 1), _EQUIP_CELL_SIZE, true, 1)
end

function WindowEquipSelectController:attach()
    self:_updateLayout()
    local listView = self:prepare(self._logicalSize)
    self.model:setListView(listView)
end

---@diagnostic disable-next-line: unused
function WindowEquipSelectController:resizeCanvas(target, width, height)
    local logicalSize = sf.Vector2u.new(width, height)
    ---@cast logicalSize sf.Vector2u
    target:resize(logicalSize)
    target:setView(target:getDefaultView())
end

function WindowEquipSelectController:refreshForSlot(slotKey)
    self.model._slotKey = slotKey
    self:_updateLayout()
    self.root:clearChildren()
    self._rowUIs = {}
    local equipData = Data.GetAllGeneralEquipData()
    local playerEquips = self.model._player._equips or {}
    local currentEquipped = self.model._player:getEquipInfo(slotKey)
    local orderedEquips = {}
    if bool(currentEquipped) then
        orderedEquips[#orderedEquips + 1] = self.UNEQUIP
    end
    self.model._equipCounts = {}
    for _, equipID in ipairs(table.orderedStringKeys(equipData, _EQUIP_ORDER)) do
        local equip = equipData[equipID] or {}
        if playerEquips[equipID] ~= nil and equip.slot == slotKey then
            orderedEquips[#orderedEquips + 1] = equipID
            self.model._equipCounts[equipID] = playerEquips[equipID]
        end
    end
    self.model._equipList = orderedEquips
    for _, entry in ipairs(orderedEquips) do
        local iconTexture = nil
        local count = 0
        if entry ~= self.UNEQUIP then
            local member = equipData[entry] or {}
            iconTexture = IconTexture.Load(member.icon or "", "Characters/items")
            count = self.model._equipCounts[entry] or 1
        end
        local rowUI = EquipItemRowUI.new({
            iconTexture = iconTexture,
            count = count
        })
        local cell = rowUI:prepare(sf.Vector2u.new(_EQUIP_CELL_SIZE, _EQUIP_CELL_SIZE))
        cell:addConfirmCallback(function ()
            self:onConfirmAction()
        end)
        self._rowUIs[#self._rowUIs + 1] = rowUI
        self.model:_applyItem(cell)
        self.root:addChild(cell)
    end
    self:prepare(self._logicalSize)
    self.model:setListView(self.root)
    self.model:resetSelection()
    self.model._lastStatusIndex = nil
    if self.model:getActive() then
        self:updateStatus()
    end
end

function WindowEquipSelectController:tick()
    if self.model._lastStatusIndex == self.model.index then
        return
    end
    self.model._lastStatusIndex = self.model.index
    if self.model:getActive() then
        self:updateStatus()
    end
end

function WindowEquipSelectController:updateStatus()
    if self.model._windowEquipStatus == nil then
        return
    end
    if self.model.index == nil or self.model.index < 0 or self.model.index >= #self.model._equipList then
        self.model._windowEquipStatus:refreshForEquip(self.model._slotKey, nil)
        return
    end
    local showUnequip = self.model._equipList[self.model.index + 1] == self.UNEQUIP
    if showUnequip then
        self.model._windowEquipStatus:refreshForEquip(self.model._slotKey, nil, true)
    else
        self.model._windowEquipStatus:refreshForEquip(
            self.model._slotKey, self.model._equipList[self.model.index + 1], false
        )
    end
end

---@diagnostic disable-next-line: unused
function WindowEquipSelectController:getGridColumns(contentWidth)
    return math.max(1, math.floor((contentWidth - _EQUIP_CELL_SIZE) / _EQUIP_CELL_SIZE))
end

function WindowEquipSelectController:returnToSlotWindow(playSE)
    if playSE == nil then
        playSE = true
    end
    if playSE then
        ManagerFunctions.playSE(GameSystem.GetCancelSE())
    end
    self.model:setActive(false)
    self.model:setVisible(true)
    if self.model._windowEquipSlot ~= nil then
        self.model._windowEquipSlot:setActive(true)
        self.model._windowEquipSlot:requestKeyboardFocus()
    end
    if self.model._windowEquipStatus ~= nil and bool(self.model._slotKey) then
        self.model._windowEquipStatus:refreshForSlot(self.model._slotKey)
    end
end

function WindowEquipSelectController:closeByCancel()
    self:returnToSlotWindow()
end

function WindowEquipSelectController:open()
    self.model:setVisible(true)
    self.model:setActive(false)
end

function WindowEquipSelectController:close()
    self.model:setVisible(false)
    self.model:setActive(false)
end

function WindowEquipSelectController:onConfirmAction()
    if self.model.index == nil or self.model.index < 0 or self.model.index >= #self.model._equipList then
        return
    end
    local currentEquipped = self.model._player:getEquipInfo(self.model._slotKey)
    ManagerFunctions.playSE(GameSystem.GetEquipSE())
    if self.model._equipList[self.model.index + 1] == self.UNEQUIP
        or self.model._equipList[self.model.index + 1] == currentEquipped then
        if bool(currentEquipped) then
            self.model._player:unequip(self.model._slotKey)
        end
    else
        self.model._player:equip(self.model._equipList[self.model.index + 1])
    end
    if self.model._windowEquipSlot ~= nil then
        self.model._windowEquipSlot:refreshSlots()
    end
    self:refreshForSlot(self.model._slotKey)
    if self.model._onEquipCallback ~= nil then
        self.model._onEquipCallback()
    end
end

function WindowEquipSelectController:_updateLayout()
    local windowSize = self.model:getSize()
    local contentWidth = math.max(1, math.floor(windowSize.x - 32))
    local contentHeight = math.max(1, math.floor(windowSize.y - 32))
    self:resizeCanvas(self.model.content, contentWidth, contentHeight)
    self._logicalSize = sf.Vector2u.new(contentWidth, contentHeight)
    self._columns = self:getGridColumns(contentWidth)
    self.root:setColumns(self._columns)
end

local FinalWindowEquipSelectController = class(WindowEquipSelectController, ListViewController)

local WindowEquipSelect = {}

WindowEquipSelect.controllerClass = FinalWindowEquipSelectController

function WindowEquipSelect:init(rect, player, windowEquipSlot, windowEquipStatus, onEquip)
    super(WindowEquipSelect, self).init(rect, nil, _EQUIP_CELL_SIZE, _EQUIP_CELL_SIZE)
    self:setHasReturnBtn(true)
    self._player = player
    self._windowEquipSlot = windowEquipSlot
    self._windowEquipStatus = windowEquipStatus
    self._onEquipCallback = onEquip
    self._slotKey = ""
    self._equipList = {}
    self._equipCounts = {}
    self._lastStatusIndex = nil
    self._selectController = self.controllerClass.new(self)
    self._selectController:attach()
    self:setActive(false)
    self:setVisible(false)
end

function WindowEquipSelect:setEquipSlotWindow(windowEquipSlot)
    self._windowEquipSlot = windowEquipSlot
end

function WindowEquipSelect:setPlayer(player)
    self._player = player
end

function WindowEquipSelect:setEquipStatusWindow(windowEquipStatus)
    self._windowEquipStatus = windowEquipStatus
end

---@param target Engine.Canvas
---@param width  integer
---@param height integer
function WindowEquipSelect:_resizeCanvas(target, width, height)
    self._selectController:resizeCanvas(target, width, height)
end

function WindowEquipSelect:refreshForSlot(slotKey)
    self._selectController:refreshForSlot(slotKey)
end

function WindowEquipSelect:onTick(deltaTime)
    super(WindowEquipSelect, self).onTick(deltaTime)
    self._selectController:tick()
end

function WindowEquipSelect:updateStatus()
    self._selectController:updateStatus()
end

---@param contentWidth integer
---@return integer
function WindowEquipSelect:_getGridColumns(contentWidth)
    return self._selectController:getGridColumns(contentWidth)
end

function WindowEquipSelect:returnToSlotWindow(playSE)
    self._selectController:returnToSlotWindow(playSE)
end

function WindowEquipSelect:onReturn()
    self._selectController:closeByCancel()
end

function WindowEquipSelect:open()
    self._selectController:open()
end

function WindowEquipSelect:close()
    self._selectController:close()
end

function WindowEquipSelect:_onConfirmAction()
    self._selectController:onConfirmAction()
end

return class(WindowEquipSelect, WindowSelectable)
