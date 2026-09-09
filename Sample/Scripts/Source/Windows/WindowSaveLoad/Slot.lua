local Engine = require("Engine")
local EventKeys = require("Source.Configs.EventKeys")
local LocaleCore = require("Source.Locale.Core")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.Parts.WindowSaveLoad.WindowSaveSlot")
local WindowSaveSlotRowController = require("Source.Windows.WindowSaveLoad.WindowSaveSlotRow.Controller")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")

local Input = Engine.Input
---@type fun(value: string): string
local LOC = LocaleCore.ApplyStringLocaleFormat

local _SLOT_ROW_HEIGHT = 32
---@class Source.Windows.WindowSaveSlot.Controller
local Controller = {}

Controller.windowOptions = {
    returnButton = true,
    hidden = true,
    list = "SlotList",
    scroll = "SlotScrollBox",
    itemHeight = _SLOT_ROW_HEIGHT
}

Controller.MAX_SAVE_SLOTS = 100
Controller.refreshEvents = { EventKeys.LocaleChanged }

function Controller:init(owner)
    self._owner = owner
    self._rows = self:createCollection(self.ui.controls["SlotList"], WindowSaveSlotRowController)
end

function Controller:onTick(deltaTime)
    WindowSelectable.onTick(self.host, deltaTime)
    self._owner:notifySlotIndexMaybeChanged(self.host.index)
end

function Controller:onKeyDown(kwargs)
    if Input.isActionTriggered(Input.getCancelKeys(), false) then
        self:onReturn()
        Input.isActionTriggered(Input.getCancelKeys(), true)
        return
    end
    if self._owner:handleTabNavigationInput() then
        return
    end
    WindowSelectable.onKeyDown(self.host, kwargs)
end

function Controller:onReturn()
    self._owner:closeByCancel()
end

function Controller:dispose()
    self.host:setListView(nil)
    self._owner = nil
    super(Controller, self).dispose()
end

function Controller:confirmSlot(slot)
    self._owner:onSlotConfirm(slot)
end

function Controller:bind()
    for slotIndex = 0, self.MAX_SAVE_SLOTS - 1 do
        local slot = slotIndex
        local rowUI = self._rows:add({
            text = LOC("SAVEFILE"):pformat(slot + 1),
            callback = function (_obj, _kwargs)
                self:confirmSlot(slot)
            end
        })
        local root = rowUI.ui.root
        self.host:applyItem(root)
    end
    self._rows:layout()
end

function Controller:refresh()
    for slotIndex, rowUI in ipairs(self._rows.items) do
        rowUI.model.text = LOC("SAVEFILE"):pformat(slotIndex)
        rowUI:refresh()
    end
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
