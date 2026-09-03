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
    self._transition = self._ui:createTransition(self)
    self._onCloseCallback = nil
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
    self._slotWindow._onCloseCallback = function ()
        self:close(self._onCloseCallback)
    end
    self._slotWindow:close()
    self._transition:hideImmediate()
end

function WindowEquip:setPlayer(player)
    self._slotWindow:setPlayer(player)
    self._selectWindow:setPlayer(player)
    self._statusWindow:setPlayer(player)
end

function WindowEquip:setOnCloseCallback(callback)
    self._onCloseCallback = callback
end

function WindowEquip:open()
    self._selectWindow:open()
    self._slotWindow:open()
    self._transition:show("FadeIn_Menu", function ()
        self:setActive(true)
        self._slotWindow:requestKeyboardFocusAtCursor()
    end)
end

function WindowEquip:close(onHidden)
    self:setActive(false)
    self._slotWindow:setActive(false)
    self._selectWindow:setActive(false)
    self._transition:hide("FadeOut_Menu", function ()
        self._slotWindow:close()
        if onHidden ~= nil then
            onHidden()
        end
    end)
end

function WindowEquip:hideImmediate()
    self._transition:hideImmediate()
    self._slotWindow:close()
end

function WindowEquip:refreshLocale()
    self._slotWindow:refreshLocale()
end

function WindowEquip:getVisible()
    return self._transition:isBlocking()
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
