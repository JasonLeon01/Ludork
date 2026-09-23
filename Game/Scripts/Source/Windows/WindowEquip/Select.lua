local GlobalCore = require("GlobalCore")
local Data = require("Source.Data")
local GameSystem = require("Source.System")
local IconTexture = require("Internal.UIBase.IconTexture")
local EquipItemRowController = require("Source.Windows.WindowEquip.EquipItemRow.Controller")
local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Parts.WindowEquip.WindowEquipSelect")
local WindowSelectable = require("Internal.UIBase.WindowSelectable")

local AudioManager = GlobalCore.AudioManager

---@class Source.Windows.WindowEquipSelect.Controller
local Controller = {}

Controller.windowOptions = { returnButton = true, hidden = true, list = "SelectList", scroll = "SelectScrollBox" }

Controller.UNEQUIP = {}

function Controller:init(player, windowEquipSlot, windowEquipStatus, onEquip)
    self._player = player
    self._windowEquipSlot = windowEquipSlot
    self._windowEquipStatus = windowEquipStatus
    self._onEquipCallback = onEquip
    self._slotKey = ""
    self._equipList = {}
    self._equipCounts = {}
    self._lastStatusIndex = nil
    self._rows = self:createCollection(self.ui.controls["SelectList"], EquipItemRowController)
end

function Controller:refreshForSlot(slotKey)
    self._slotKey = slotKey
    self._rows:clear()
    local equipData = Data.GetAllGeneralEquipData()
    local playerEquips = self._player:getEquips()
    local currentEquipped = self._player:getEquipInfo(slotKey)
    local orderedEquips = {}
    if bool(currentEquipped) then
        orderedEquips[#orderedEquips + 1] = self.UNEQUIP
    end
    self._equipCounts = {}
    for _, equipID in ipairs(table.orderedStringKeys(equipData)) do
        local equip = equipData[equipID] or {}
        if playerEquips[equipID] ~= nil and equip.slot == slotKey then
            orderedEquips[#orderedEquips + 1] = equipID
            self._equipCounts[equipID] = playerEquips[equipID]
        end
    end
    self._equipList = orderedEquips
    for _, entry in ipairs(orderedEquips) do
        local iconTexture = nil
        local count = 0
        if entry ~= self.UNEQUIP then
            local member = equipData[entry] or {}
            iconTexture = IconTexture.Load(member.icon or "")
            count = self._equipCounts[entry] or 1
        end
        local rowUI = self._rows:add({
            iconTexture = iconTexture,
            count = count
        })
        local cell = rowUI.ui.root
        cell:addConfirmCallback(self:bindCallback(Controller.onConfirmAction))
    end
    self._rows:layout()
    self.host:setListView(self.ui.controls["SelectList"])
    self.host:resetSelection()
    self._lastStatusIndex = nil
    if self.host:getActive() then
        self:updateStatus()
    end
end

function Controller:onTick(deltaTime)
    WindowSelectable.onTick(self.host, deltaTime)
    if self._lastStatusIndex == self.host.index then
        return
    end
    self._lastStatusIndex = self.host.index
    if self.host:getActive() then
        self:updateStatus()
    end
end

function Controller:updateStatus()
    if self._windowEquipStatus == nil then
        return
    end
    if self.host.index == nil or self.host.index < 0 or self.host.index >= #self._equipList then
        self._windowEquipStatus:refreshForEquip(self._slotKey, nil)
        return
    end
    local showUnequip = self._equipList[self.host.index + 1] == self.UNEQUIP
    if showUnequip then
        self._windowEquipStatus:refreshForEquip(self._slotKey, nil, true)
    else
        self._windowEquipStatus:refreshForEquip(self._slotKey, self._equipList[self.host.index + 1], false)
    end
end

function Controller:returnToSlotWindow(playSE)
    if playSE == nil then
        playSE = true
    end
    if playSE then
        AudioManager.playSound(GameSystem.GetCancelSE())
    end
    self.host:setActive(false)
    self.host:setVisible(true)
    if self._windowEquipSlot ~= nil then
        self._windowEquipSlot:setActive(true)
        self._windowEquipSlot:requestKeyboardFocus()
    end
    if self._windowEquipStatus ~= nil and bool(self._slotKey) then
        self._windowEquipStatus:refreshForSlot(self._slotKey)
    end
end

function Controller:onReturn()
    self:returnToSlotWindow()
end

function Controller:open()
    self.host:setVisible(true)
    self.host:setActive(false)
end

function Controller:close()
    self.host:setVisible(false)
    self.host:setActive(false)
end

function Controller:onConfirmAction()
    if self.host.index == nil or self.host.index < 0 or self.host.index >= #self._equipList then
        return
    end
    local equipID = assert(self._equipList[self.host.index + 1])
    local currentEquipped = self._player:getEquipInfo(self._slotKey)
    AudioManager.playSound(GameSystem.GetEquipSE())
    if equipID == self.UNEQUIP or equipID == currentEquipped then
        if bool(currentEquipped) then
            self._player:unequip(self._slotKey)
        end
    else
        ---@cast equipID string
        self._player:equip(equipID)
    end
    if self._windowEquipSlot ~= nil then
        self._windowEquipSlot:refreshSlots()
    end
    self:refreshForSlot(self._slotKey)
    if self._onEquipCallback ~= nil then
        self._onEquipCallback()
    end
end

function Controller:setPlayer(player)
    self._player = player
end

function Controller:setEquipStatusWindow(windowEquipStatus)
    self._windowEquipStatus = windowEquipStatus
end

function Controller:setEquipSlotWindow(windowEquipSlot)
    self._windowEquipSlot = windowEquipSlot
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
