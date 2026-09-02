local Engine = require("Engine")
local WindowEquipUI = require("Source.UI.WindowEquip")
local WindowEquipSelect = require("Source.Windows.WindowEquip.Select")
local WindowEquipSlot = require("Source.Windows.WindowEquip.Slot")
local WindowEquipStatus = require("Source.Windows.WindowEquip.Status")

local Canvas = Engine.Canvas

---@class Source.Windows.WindowEquip
local WindowEquip = {}

function WindowEquip:init(player)
    super(WindowEquip, self).init(Engine.ToIntRect(192, 0, 448, 352))
    self._ui = WindowEquipUI.new(self)
    self._ui:attach()
    self._slotWindow = WindowEquipSlot.new(
        Engine.ToIntRect(0, 0, 192, 160), player, nil, nil, nil, self._ui:getSlotAsset()
    )
    self._selectWindow = WindowEquipSelect.new(
        Engine.ToIntRect(0, 160, 192, 192), player, self._slotWindow, nil, nil, self._ui:getSelectAsset()
    )
    self._statusWindow = WindowEquipStatus.new(
        Engine.ToIntRect(192, 0, 256, 352), player, self._ui:getStatusPaneAsset()
    )
    self._slotWindow:setEquipSelectWindow(self._selectWindow)
    self._slotWindow:setEquipStatusWindow(self._statusWindow)
    self._selectWindow:setEquipStatusWindow(self._statusWindow)
    self:addChild(self._slotWindow)
    self:addChild(self._selectWindow)
    self:addChild(self._statusWindow)
    self:close()
end

function WindowEquip:setPlayer(player)
    self._slotWindow:setPlayer(player)
    self._selectWindow:setPlayer(player)
    self._statusWindow:setPlayer(player)
end

function WindowEquip:setOnCloseCallback(callback)
    self._slotWindow._onCloseCallback = callback
end

function WindowEquip:open()
    self:setVisible(true)
    self._selectWindow:open()
    self._slotWindow:open()
end

function WindowEquip:close()
    self._slotWindow:close()
    self:setVisible(false)
end

function WindowEquip:refreshLocale()
    self._slotWindow:refreshLocale()
end

function WindowEquip:getVisible()
    return self._slotWindow:getVisible() or self._selectWindow:getVisible() or self._statusWindow:getVisible()
end

function WindowEquip:requestSlotFocus()
    self._slotWindow:requestKeyboardFocusAtCursor()
end

function WindowEquip:getSlotFocusTarget()
    return self._slotWindow
end

function WindowEquip:getFocusControls()
    return self._slotWindow, self._selectWindow
end

function WindowEquip:returnSelectToSlot()
    if not self._selectWindow:getVisible() then
        return false
    end
    if not (self._selectWindow:getActive() or self._selectWindow:getFocused()) then
        return false
    end
    self._selectWindow:returnToSlotWindow()
    return true
end

return class(WindowEquip, Canvas)
