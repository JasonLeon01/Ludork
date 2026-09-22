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

---@class Source.Windows.WindowSaveSlot.Controller
local Controller = {}

Controller.windowOptions = { returnButton = true, hidden = true, list = "SlotList", scroll = "SlotScrollBox" }

Controller.MAX_SAVE_SLOTS = 100
Controller.refreshEvents = { EventKeys.LocaleChanged }

function Controller:init(owner)
    self._owner = owner
    self._rows = self:createCollection(self.ui.controls["SlotList"], WindowSaveSlotRowController)
    self._buildClock = sf.Clock.new()
end

function Controller:onTick(deltaTime)
    if self.host:getVisible() and not self:isReady() then
        if Input.isActionTriggered(Input.getCancelKeys(), true) then
            self:onReturn()
            return
        end
        self:_buildRows()
    end
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
    if not self:isReady() then
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
    if self:isReady() then
        self._owner:onSlotConfirm(slot)
    end
end

function Controller:bind()
    self.host:setSelectionInputPaused(true)
end

function Controller:isReady()
    return #self._rows.items == self.MAX_SAVE_SLOTS
end

function Controller:_buildRows()
    self._buildClock:restart()
    repeat
        local slot = #self._rows.items
        self._rows:add({
            text = LOC("SAVEFILE"):pformat(slot + 1),
            callback = function (_obj, _kwargs)
                self:confirmSlot(slot)
            end
        })
    until self:isReady() or self._buildClock:getElapsedTime():asMicroseconds() >= 2000
    self._rows:layout()
    if self:isReady() then
        self.host:resetSelection()
        self.host:setSelectionInputPaused(false)
        self._owner:onSlotsReady()
    end
end

function Controller:refresh()
    for slotIndex, rowUI in ipairs(self._rows.items) do
        rowUI.model.text = LOC("SAVEFILE"):pformat(slotIndex)
        rowUI:prepare()
    end
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
