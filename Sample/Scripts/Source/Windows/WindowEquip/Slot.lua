local WindowEquipSlotUI = require("Source.UI.Parts.WindowEquip.WindowEquipSlot")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local WindowEquipSlotController = require("Source.Windows.WindowEquip.Slot.Controller")

---@class (partial) Source.Windows.WindowEquipSlot
local WindowEquipSlot = {}

WindowEquipSlot.controllerClass = WindowEquipSlotController

function WindowEquipSlot:init(rect, player, windowEquipSelect, windowEquipStatus, onClose, instance)
    super(WindowEquipSlot, self).init(rect, nil, nil, WindowEquipSlotController.ROW_HEIGHT, nil, nil, nil, nil, instance
        ~= nil)
    self:setHasReturnBtn(true)
    self._onCloseCallback = onClose
    self._player = player
    self._windowEquipSelect = windowEquipSelect
    self._windowEquipStatus = windowEquipStatus
    self._slotKeys = {}
    self._lastSlotIndex = nil
    if instance ~= nil then
        local ui = WindowEquipSlotUI.new(self, instance)
        self._ui = ui
        ui:attach()
        self:setScrollBox(ui:getScrollBox())
        self:setListView(ui:getListView())
    end
    self._slotController = self.controllerClass.new(self)
    self._slotController:attach(self:getListView())
    self:refreshSlots()
    self:setActive(false)
    self:setVisible(false)
end

function WindowEquipSlot:setEquipSelectWindow(windowEquipSelect)
    self._windowEquipSelect = windowEquipSelect
end

function WindowEquipSlot:setPlayer(player)
    self._player = player
end

function WindowEquipSlot:setEquipStatusWindow(windowEquipStatus)
    self._windowEquipStatus = windowEquipStatus
end

---@param slotKey string
---@return sf.Texture | nil, string
function WindowEquipSlot:_getSlotCellData(slotKey)
    local texture, label = self._slotController:getSlotCellData(slotKey)
    return texture, label
end

function WindowEquipSlot:refreshSlots()
    self._slotController:refreshSlots()
end

function WindowEquipSlot:refreshLocale()
    self._slotController:refreshLocale()
end

-- Force redraw while visible even when inactive.
function WindowEquipSlot:_redrawIfVisible()
    self._slotController:redrawIfVisible()
end

---@return string | nil
function WindowEquipSlot:_getCurrentSlotKey()
    return self._slotController:getCurrentSlotKey()
end

function WindowEquipSlot:_notifySlotChanged()
    self._slotController:notifySlotChanged()
end

function WindowEquipSlot:onTick(deltaTime)
    super(WindowEquipSlot, self).onTick(deltaTime)
    self._slotController:tick()
end

function WindowEquipSlot:_focusSelectWindow()
    self._slotController:focusSelectWindow()
end

function WindowEquipSlot:open()
    self._slotController:open()
end

function WindowEquipSlot:_closeChildWindows()
    self._slotController:closeChildWindows()
end

function WindowEquipSlot:close()
    self._slotController:close()
end

function WindowEquipSlot:onReturn()
    self._slotController:closeByCancel()
end

return class(WindowEquipSlot, WindowSelectable)
