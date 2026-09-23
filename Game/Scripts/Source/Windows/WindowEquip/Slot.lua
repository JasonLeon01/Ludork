local GlobalCore = require("GlobalCore")
local Data = require("Source.Data")
local GameSystem = require("Source.System")
local LocaleCore = require("Source.Locale.Core")
local IconTexture = require("Internal.UIBase.IconTexture")
local EquipSlotRowController = require("Source.Windows.WindowEquip.EquipSlotRow.Controller")
local Ui = require("Internal.UIBase.Ui")
local View = require("Internal.UI.Parts.WindowEquip.WindowEquipSlot")
local WindowSelectable = require("Internal.UIBase.WindowSelectable")

local AudioManager = GlobalCore.AudioManager
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local _SLOT_ORDER = { "weapon", "shield", "accessory" }

---@class Source.Windows.WindowEquipSlot.Controller
local Controller = {}

Controller.windowOptions = { returnButton = true, hidden = true, list = "SlotList", scroll = "SlotScrollBox" }

function Controller:init(player, windowEquipSelect, windowEquipStatus, onClose)
    self._player = player
    self._windowEquipSelect = windowEquipSelect
    self._windowEquipStatus = windowEquipStatus
    self._onCloseCallback = onClose
    self._slotKeys = {}
    self._lastSlotIndex = nil
    self._rows = self:createCollection(self.ui.controls["SlotList"], EquipSlotRowController)
end

function Controller:ready()
    self:refreshSlots()
end

function Controller:getSlotCellData(slotKey)
    local equipID = self._player:getEquipInfo(slotKey)
    if not bool(equipID) then
        return nil, LOC("EQUIP_UNEQUIPPED")
    end
    local equipInfo = Data.GetGeneralEquipData(equipID)
    local name = equipInfo.name or ""
    local label = bool(name) and LOC(name) or equipID
    return IconTexture.Load(equipInfo.icon or ""), label
end

function Controller:refreshSlots()
    local savedSlotKey = self:getCurrentSlotKey()
    local classData = Data.GetGeneralClassData(self._player.attributes.CLASS)
    local classSlots = classData.slot
    if classSlots == nil then
        classSlots = {}
    end
    self._slotKeys = table.orderedStringKeys(classSlots, _SLOT_ORDER)
    self._rows:clear()
    for _, slotKey in ipairs(self._slotKeys) do
        local iconTexture, label = self:getSlotCellData(slotKey)
        local rowUI = self._rows:add({
            label = label,
            iconTexture = iconTexture
        })
        local child = rowUI.ui.root
        child:addConfirmCallback(self:bindCallback(Controller.focusSelectWindow))
    end
    self._rows:layout()
    self.host:setListView(self.ui.controls["SlotList"])
    if savedSlotKey ~= nil then
        local restoredIndex = nil
        local index = table.index(self._slotKeys, savedSlotKey)
        if index ~= nil then
            restoredIndex = index - 1
        end
        self.host.index = restoredIndex
    else
        self.host.index = bool(self._slotKeys) and 0 or nil
    end
    if self.host.index == nil and bool(self._slotKeys) then
        self.host.index = 0
    end
    self._lastSlotIndex = self.host.index
    self.host:detachSelectionRect()
    self:redrawIfVisible()
end

function Controller:refreshLocale()
    if not self.host:getVisible() then
        return
    end
    self:refreshSlots()
    local slotKey = self:getCurrentSlotKey()
    if slotKey == nil then
        return
    end
    if self._windowEquipSelect ~= nil and self._windowEquipSelect:getActive() then
        self._windowEquipSelect:updateStatus()
        return
    end
    if self._windowEquipStatus ~= nil then
        self._windowEquipStatus:refreshForSlot(slotKey)
    end
end

function Controller:redrawIfVisible()
    if not self.host:getVisible() then
        return
    end
    local wasActive = self.host:getActive()
    local returnButtonSuppressed = self.host:isReturnButtonSuppressed()
    if not wasActive then
        self.host:setReturnButtonSuppressed(true)
        self.host:setActive(true)
    end
    self.host:update(0.0)
    self.host:render()
    if not wasActive then
        self.host:setActive(false)
        self.host:setReturnButtonSuppressed(returnButtonSuppressed)
    end
end

function Controller:getCurrentSlotKey()
    if self.host.index == nil or self.host.index < 0 or self.host.index >= #self._slotKeys then
        return nil
    end
    return self._slotKeys[self.host.index + 1]
end

function Controller:notifySlotChanged()
    local slotKey = self:getCurrentSlotKey()
    if slotKey == nil then
        return
    end
    if self._windowEquipStatus ~= nil and self._windowEquipStatus:getVisible() then
        self._windowEquipStatus:refreshForSlot(slotKey)
    end
    if self._windowEquipSelect ~= nil and self._windowEquipSelect:getVisible() then
        self._windowEquipSelect:refreshForSlot(slotKey)
    end
end

function Controller:onTick(deltaTime)
    WindowSelectable.onTick(self.host, deltaTime)
    if self._lastSlotIndex == self.host.index then
        return
    end
    self._lastSlotIndex = self.host.index
    self:notifySlotChanged()
end

function Controller:focusSelectWindow()
    if self._windowEquipSelect == nil then
        return
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    local slotKey = self:getCurrentSlotKey()
    if slotKey ~= nil then
        self._windowEquipSelect:refreshForSlot(slotKey)
    end
    if self._windowEquipStatus ~= nil and slotKey ~= nil then
        self._windowEquipStatus:setVisible(true)
        self._windowEquipStatus:refreshForSlot(slotKey)
    end
    self.host:setActive(false)
    self._windowEquipSelect:setVisible(true)
    self._windowEquipSelect:setActive(true)
    self._windowEquipSelect:updateStatus()
    self._windowEquipSelect:requestKeyboardFocusAtCursor()
end

function Controller:open()
    self:refreshSlots()
    self.host:resetSelection()
    self._lastSlotIndex = self.host.index
    self.host:setVisible(true)
    self.host:setActive(true)
    local slotKey = self:getCurrentSlotKey()
    if slotKey == nil then
        self:closeChildWindows()
        return
    end
    if self._windowEquipStatus ~= nil then
        self._windowEquipStatus:openForSlot(slotKey)
    end
    if self._windowEquipSelect ~= nil then
        self._windowEquipSelect:refreshForSlot(slotKey)
        self._windowEquipSelect:open()
    end
end

function Controller:closeChildWindows()
    if self._windowEquipStatus ~= nil then
        self._windowEquipStatus:close()
    end
    if self._windowEquipSelect ~= nil then
        self._windowEquipSelect:close()
    end
end

function Controller:close()
    self.host:setVisible(false)
    self.host:setActive(false)
    self:closeChildWindows()
end

function Controller:onReturn()
    AudioManager.playSound(GameSystem.GetCancelSE())
    if self._onCloseCallback ~= nil then
        self._onCloseCallback()
    else
        self:close()
    end
end

function Controller:setPlayer(player)
    self._player = player
end

function Controller:setEquipStatusWindow(windowEquipStatus)
    self._windowEquipStatus = windowEquipStatus
end

function Controller:setEquipSelectWindow(windowEquipSelect)
    self._windowEquipSelect = windowEquipSelect
end

function Controller:setOnCloseCallback(callback)
    self._onCloseCallback = callback
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
