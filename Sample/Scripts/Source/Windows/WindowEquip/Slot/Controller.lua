local GlobalCore = require("GlobalCore")
local Data = require("Source.Data")
local GameSystem = require("Source.System")
local LocaleCore = require("Source.Locale.Core")
local IconTexture = require("Source.UI.IconTexture")
local EquipSlotRowUI = require("Source.UI.Parts.WindowEquip.EquipSlotRow")
local ListViewController = require("Source.UI.Helpers.ListView")

local AudioManager = GlobalCore.AudioManager
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local _SLOT_ORDER = { "weapon", "shield", "accessory" }

---@class (partial) Source.Windows.WindowEquip.Slot.Controller
local WindowEquipSlotController = {}

WindowEquipSlotController.ROW_HEIGHT = 32

function WindowEquipSlotController:init(model)
    self._logicalSize = nil
    self._rowUIs = {}
    super
        (WindowEquipSlotController, self)
        .init(model, sf.Vector2u.new(1, 1), WindowEquipSlotController.ROW_HEIGHT, true, 1)
end

function WindowEquipSlotController:attach(listView)
    self:_refreshLogicalSize()
    if listView ~= nil then
        self.root = listView
        self.root:clearChildren()
    end
    local root = self:prepare(self._logicalSize)
    self.model:setListView(root)
end

function WindowEquipSlotController:getSlotCellData(slotKey)
    local equipID = self.model._player:getEquipInfo(slotKey)
    if not bool(equipID) then
        return nil, LOC("EQUIP_UNEQUIPPED")
    end
    local equipInfo = Data.GetGeneralEquipData(equipID)
    local name = equipInfo.name or ""
    local label = bool(name) and LOC(name) or equipID
    return IconTexture.Load(equipInfo.icon or ""), label
end

function WindowEquipSlotController:refreshSlots()
    local savedSlotKey = self:getCurrentSlotKey()
    local classData = Data.GetGeneralClassData(self.model._player.attributes.CLASS)
    local classSlots = classData.slot
    if classSlots == nil then
        classSlots = {}
    end
    self.model._slotKeys = table.orderedStringKeys(classSlots, _SLOT_ORDER)
    self:_refreshLogicalSize()
    self._columns = 1
    self.root:setColumns(self._columns)
    self.root:clearChildren()
    self._rowUIs = {}
    local cellWidth = self.model:_getRectWidth()
    for _, slotKey in ipairs(self.model._slotKeys) do
        local iconTexture, label = self:getSlotCellData(slotKey)
        local rowUI = EquipSlotRowUI.new({
            label = label,
            iconTexture = iconTexture
        })
        local child = rowUI:prepare(sf.Vector2u.new(cellWidth, WindowEquipSlotController.ROW_HEIGHT))
        child:addConfirmCallback(function ()
            self:focusSelectWindow()
        end)
        self._rowUIs[#self._rowUIs + 1] = rowUI
        self.model:_applyItem(child)
        self.root:addChild(child)
    end
    self:prepare(self._logicalSize)
    self.model:setListView(self.root)
    if savedSlotKey ~= nil then
        local restoredIndex = nil
        local index = table.index(self.model._slotKeys, savedSlotKey)
        if index ~= nil then
            restoredIndex = index - 1
        end
        self.model.index = restoredIndex
    else
        self.model.index = bool(self.model._slotKeys) and 0 or nil
    end
    if self.model.index == nil and bool(self.model._slotKeys) then
        self.model.index = 0
    end
    self.model._lastSlotIndex = self.model.index
    self.model:_detachSelectionRect()
    self:redrawIfVisible()
end

function WindowEquipSlotController:refreshLocale()
    if not self.model:getVisible() then
        return
    end
    self:refreshSlots()
    local slotKey = self:getCurrentSlotKey()
    if slotKey == nil then
        return
    end
    if self.model._windowEquipSelect ~= nil and self.model._windowEquipSelect:getActive() then
        self.model._windowEquipSelect:updateStatus()
        return
    end
    if self.model._windowEquipStatus ~= nil then
        self.model._windowEquipStatus:refreshForSlot(slotKey)
    end
end

function WindowEquipSlotController:redrawIfVisible()
    if not self.model:getVisible() then
        return
    end
    local wasActive = self.model:getActive()
    local returnButtonSuppressed = self.model._returnButtonSuppressed == true
    if not wasActive then
        self.model:_setReturnButtonSuppressed(true)
        self.model:setActive(true)
    end
    self.model:update(0.0)
    self.model:render()
    if not wasActive then
        self.model:setActive(false)
        self.model:_setReturnButtonSuppressed(returnButtonSuppressed)
    end
end

function WindowEquipSlotController:getCurrentSlotKey()
    if self.model.index == nil or self.model.index < 0 or self.model.index >= #self.model._slotKeys then
        return nil
    end
    return self.model._slotKeys[self.model.index + 1]
end

function WindowEquipSlotController:notifySlotChanged()
    local slotKey = self:getCurrentSlotKey()
    if slotKey == nil then
        return
    end
    if self.model._windowEquipStatus ~= nil and self.model._windowEquipStatus:getVisible() then
        self.model._windowEquipStatus:refreshForSlot(slotKey)
    end
    if self.model._windowEquipSelect ~= nil and self.model._windowEquipSelect:getVisible() then
        self.model._windowEquipSelect:refreshForSlot(slotKey)
    end
end

function WindowEquipSlotController:tick()
    if self.model._lastSlotIndex == self.model.index then
        return
    end
    self.model._lastSlotIndex = self.model.index
    self:notifySlotChanged()
end

function WindowEquipSlotController:focusSelectWindow()
    if self.model._windowEquipSelect == nil then
        return
    end
    AudioManager.playSound(GameSystem.GetDecisionSE())
    local slotKey = self:getCurrentSlotKey()
    if slotKey ~= nil then
        self.model._windowEquipSelect:refreshForSlot(slotKey)
    end
    if self.model._windowEquipStatus ~= nil and slotKey ~= nil then
        self.model._windowEquipStatus:setVisible(true)
        self.model._windowEquipStatus:refreshForSlot(slotKey)
    end
    self.model:setActive(false)
    self.model._windowEquipSelect:setVisible(true)
    self.model._windowEquipSelect:setActive(true)
    self.model._windowEquipSelect:updateStatus()
    self.model._windowEquipSelect:requestKeyboardFocusAtCursor()
end

function WindowEquipSlotController:open()
    self:refreshSlots()
    self.model:resetSelection()
    self.model._lastSlotIndex = self.model.index
    self.model:setVisible(true)
    self.model:setActive(true)
    local slotKey = self:getCurrentSlotKey()
    if slotKey == nil then
        self:closeChildWindows()
        return
    end
    if self.model._windowEquipStatus ~= nil then
        self.model._windowEquipStatus:openForSlot(slotKey)
    end
    if self.model._windowEquipSelect ~= nil then
        self.model._windowEquipSelect:refreshForSlot(slotKey)
        self.model._windowEquipSelect:open()
    end
end

function WindowEquipSlotController:closeChildWindows()
    if self.model._windowEquipStatus ~= nil then
        self.model._windowEquipStatus:close()
    end
    if self.model._windowEquipSelect ~= nil then
        self.model._windowEquipSelect:close()
    end
end

function WindowEquipSlotController:close()
    self.model:setVisible(false)
    self.model:setActive(false)
    self:closeChildWindows()
end

function WindowEquipSlotController:closeByCancel()
    AudioManager.playSound(GameSystem.GetCancelSE())
    if self.model._onCloseCallback ~= nil then
        self.model._onCloseCallback()
    else
        self:close()
    end
end

function WindowEquipSlotController:_refreshLogicalSize()
    local contentSize = self.model.content:getSize()
    local logicalSize = sf.Vector2u.new(math.max(1, math.floor(contentSize.x)), math.max(1, math.floor(contentSize.y)))
    ---@cast logicalSize sf.Vector2u
    self._logicalSize = logicalSize
end

return class(WindowEquipSlotController, ListViewController)
