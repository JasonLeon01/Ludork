local Engine = require("Engine")
local Ui = require("Internal.UIBase.Ui")
local UiLayout = require("Internal.UIBase.UiLayout")
local WindowTransition = require("Internal.UIBase.WindowTransition")
local View = require("Internal.UI.WindowEquip")
local WindowEquipSelect = require("Source.Windows.WindowEquip.Select")
local WindowEquipSlot = require("Source.Windows.WindowEquip.Slot")
local WindowEquipStatus = require("Source.Windows.WindowEquip.Status")

local Canvas = Engine.Canvas

---@class Source.Windows.WindowEquip.Controller
local Controller = {}

Controller.windowOptions = { hidden = true }

function Controller:init(player)
    self._onCloseCallback = nil
    self._transitionProfile = WindowTransition.DEFAULT
    self._slotWindow = self:createChild("SlotAsset", WindowEquipSlot, player)
    self._selectWindow = self:createChild("SelectAsset", WindowEquipSelect, player, self._slotWindow)
    self._statusWindow = self:createChild("StatusPaneAsset", WindowEquipStatus, player)
    self._slotWindow:setEquipSelectWindow(self._selectWindow)
    self._slotWindow:setEquipStatusWindow(self._statusWindow)
    self._selectWindow:setEquipStatusWindow(self._statusWindow)
    self._slotWindow:setOnCloseCallback(function ()
        self:close(self._onCloseCallback)
    end)
    self._slotWindow:close()
end

function Controller:setPlayer(player)
    self._slotWindow:setPlayer(player)
    self._selectWindow:setPlayer(player)
    self._statusWindow:setPlayer(player)
end

function Controller:setOnCloseCallback(callback)
    self._onCloseCallback = callback
end

function Controller:open(transitionProfile, dockPosition)
    self._transitionProfile = transitionProfile or WindowTransition.DEFAULT
    local size = self.ui.root:getSize()
    if self._transitionProfile == WindowTransition.MENU then
        self.host:setPosition(assert(dockPosition, "Menu windows require a dock position"))
    else
        self.host:setPosition(UiLayout.GetCenteredPosition(size.x, size.y))
    end
    self._selectWindow:open()
    self._slotWindow:open()
    local fadeIn = WindowTransition.GetAnimationNames(self._transitionProfile)
    self._transition:show(fadeIn, function ()
        self.host:setActive(true)
        self._slotWindow:requestKeyboardFocusAtCursor()
    end)
end

function Controller:close(onHidden)
    self.host:setActive(false)
    self._slotWindow:setActive(false)
    self._selectWindow:setActive(false)
    local _, fadeOut = WindowTransition.GetAnimationNames(self._transitionProfile)
    self._transition:hide(fadeOut, function ()
        self._slotWindow:close()
        if onHidden ~= nil then
            onHidden()
        end
    end)
end

function Controller:hideImmediate()
    self._transition:hideImmediate()
    self._slotWindow:close()
end

function Controller:refreshLocale()
    self._slotWindow:refreshLocale()
end

function Controller:getVisible()
    return self._transition:isBlocking()
end

function Controller:requestSlotFocus()
    self._slotWindow:requestKeyboardFocusAtCursor()
end

function Controller:getSlotFocusTarget()
    return self._slotWindow
end

function Controller:getFocusControls()
    return self._slotWindow, self._selectWindow
end

function Controller:returnSelectToSlot()
    if not self._selectWindow:getVisible() then
        return false
    end
    if not (self._selectWindow:getActive() or self._selectWindow:getFocused()) then
        return false
    end
    self._selectWindow:returnToSlotWindow()
    return true
end

function Controller:dispose()
    self:hideImmediate()
    super(Controller, self).dispose()
end

return Ui.DefineWindow(View, Controller, Canvas)
