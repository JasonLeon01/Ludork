local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local Save = require("Source.Save")
local Logging = require("Global.Utils.Logging")
local Ui = require("Source.UIBase.Ui")
local View = require("Source.UI.WindowSaveLoad")
local UiLayout = require("Source.UIBase.UiLayout")
local WindowTransition = require("Source.UIBase.WindowTransition")
local WindowSaveDetail = require("Source.Windows.WindowSaveLoad.Detail")
local WindowSaveSlot = require("Source.Windows.WindowSaveLoad.Slot")
local WindowSaveTabs = require("Source.Windows.WindowSaveLoad.Tabs")

local AudioManager = GlobalCore.AudioManager
local Canvas = Engine.Canvas

local CLOSE_REASON_CANCEL = "cancel"
local CLOSE_REASON_SAVED = "saved"
local CLOSE_REASON_LOADED = "loaded"

---@class Source.Windows.WindowSaveLoad.Controller
local Controller = {}

Controller.windowOptions = { hidden = true }

function Controller:init(loadOnly, getSaveSource, onClose, onLoaded)
    self._loadOnly = loadOnly == true
    self._getSaveSource = getSaveSource
    self._onCloseCallback = onClose
    self._onLoadedCallback = onLoaded
    self._mode = "load"
    self._transitionProfile = WindowTransition.DEFAULT
    self._tabWindow = nil
    if not self._loadOnly then
        self._tabWindow = self:createChild("TabsAsset", WindowSaveTabs, self.host)
    else
        self.ui.assets["TabsAsset"].root:setVisible(false)
    end
    self._slotWindow = self:createChild("SlotAsset", WindowSaveSlot, self.host)
    self._detailWindow = self:createChild("DetailAsset", WindowSaveDetail)
    if self._loadOnly then
        local tabHeight = self.ui.assets["TabsAsset"].root:getSize().y
        local size = self.ui.root:getSize()
        local bounds = UiLayout.GetCenteredRect(size.x, size.y - tabHeight)
        self.host:setPosition(sf.Vector2f.new(bounds.position.x, bounds.position.y))
        self._slotWindow:setPosition(self._slotWindow:getPosition() - sf.Vector2f.new(0, tabHeight))
        self._detailWindow:setPosition(self._detailWindow:getPosition() - sf.Vector2f.new(0, tabHeight))
    end
    self._lastSlotIndex = nil
    self._scanReader = Engine.SavePreviewReader.new()
    self._scanPending = false
    self._latestSlot = nil
    self._selectionTouched = false
    self._opening = false
    self._openClock = sf.Clock.new()
    self._openedBefore = false
    self._reportedOpen = false
end

function Controller:getTabWindow()
    return self._tabWindow
end

function Controller:getSlotWindow()
    return self._slotWindow
end

function Controller:getDetailWindow()
    return self._detailWindow
end

function Controller:getVisible()
    return self._transition:isBlocking()
end

function Controller:setVisible(visible)
    super(Canvas, self.host).setVisible(visible)
    if self._tabWindow ~= nil then
        self._tabWindow:setVisible(visible)
    end
    self._slotWindow:setVisible(visible)
    self._detailWindow:setVisible(visible)
end

function Controller:open(transitionProfile, initialMode, dockPosition)
    self._openClock:restart()
    self._reportedOpen = false
    self._opening = true
    self._selectionTouched = false
    self._latestSlot = nil
    self._transitionProfile = transitionProfile or WindowTransition.DEFAULT
    ---@type "load" | "save"
    local mode = "load"
    if not self._loadOnly and initialMode == "save" then
        mode = "save"
    end
    self._mode = mode
    if self._tabWindow ~= nil then
        self.ui.assets["TabsAsset"].controls["Tabs"]:setSelectedIndex(mode == "save" and 1 or 0)
    end
    self._slotWindow:resetSelection()
    local paths = {}
    for slot = 1, WindowSaveSlot.MAX_SAVE_SLOTS do
        paths[slot] = Save.GetSavePath(slot)
    end
    self._scanReader:requestScan(paths)
    self._scanPending = true
    self._lastSlotIndex = nil
    if not self._loadOnly then
        local size = self.ui.root:getSize()
        if self._transitionProfile == WindowTransition.MENU then
            self.host:setPosition(assert(dockPosition, "Menu windows require a dock position"))
        else
            self.host:setPosition(UiLayout.GetCenteredPosition(size.x, size.y))
        end
    end
    if self._tabWindow ~= nil then
        self._tabWindow:setActive(false)
    end
    self._slotWindow:setActive(false)
    self._detailWindow:setActive(false)
    local fadeIn = WindowTransition.GetAnimationNames(self._transitionProfile)
    self._transition:show(fadeIn, function ()
        self.host:setActive(true)
        if self._tabWindow ~= nil then
            self._tabWindow:setActive(true)
        end
        self._slotWindow:setActive(true)
        self._slotWindow:requestKeyboardFocusAtCursor()
    end)
    self._lastSlotIndex = self._slotWindow.index
    self._detailWindow:setSlot(self._lastSlotIndex or 0)
    self._detailWindow:setPreviewEnabled(true)
end

function Controller:onTick(_)
    if not self._opening then
        return
    end
    if self._scanPending and self._scanReader:pollScan() then
        self._scanPending = false
        local error = self._scanReader:getScanError()
        if error ~= "" then
            Logging.warning("Save slot scan failed: %s", error)
        elseif not self._selectionTouched then
            self._latestSlot = self._scanReader:getLatestSlot()
            self:_applyLatestSlot()
        end
    end
    if not self._reportedOpen and self._slotWindow:isReady() then
        Logging.info(
            "Save window %s open: %.2f ms until slot list ready", self._openedBefore and "repeat" or "first",
            self._openClock:getElapsedTime():asMicroseconds() / 1000
        )
        self._reportedOpen = true
        self._openedBefore = true
    end
end

function Controller:onSlotsReady()
    self._lastSlotIndex = self._slotWindow.index
    self:_applyLatestSlot()
    self._detailWindow:setSlot(self._slotWindow.index)
    if self._opening and self._transition:isOpen() then
        self._slotWindow:requestKeyboardFocusAtCursor()
    end
end

function Controller:_applyLatestSlot()
    if self._slotWindow:isReady() and self._slotWindow.index ~= self._lastSlotIndex then
        self._selectionTouched = true
    end
    if self._selectionTouched or not self._slotWindow:isReady() or self._latestSlot == nil or self._latestSlot <= 0 then
        return
    end
    local latestSlotIndex = self._latestSlot - 1
    ---@cast latestSlotIndex integer
    self._slotWindow:selectIndex(latestSlotIndex)
    self._lastSlotIndex = latestSlotIndex
    self._detailWindow:setSlot(latestSlotIndex)
end

function Controller:close(onHidden)
    self._opening = false
    self._scanPending = false
    self._scanReader:cancel()
    self._detailWindow:setPreviewEnabled(false)
    if self._tabWindow ~= nil then
        self._tabWindow:setActive(false)
    end
    self._slotWindow:setActive(false)
    self._detailWindow:setActive(false)
    local _, fadeOut = WindowTransition.GetAnimationNames(self._transitionProfile)
    self._transition:hide(fadeOut, onHidden)
end

function Controller:closeByCancel()
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:_closeWithReason(CLOSE_REASON_CANCEL)
end

function Controller:handleTabNavigationInput()
    if self._tabWindow == nil then
        return false
    end
    return self._tabWindow:handleNavigationInput()
end

---@param index integer
function Controller:onTabSelected(index)
    if self._loadOnly then
        return
    end
    local mode = index == 0 and "load" or "save"
    if mode == self._mode then
        return
    end
    self._mode = mode
    self._detailWindow:refresh()
end

function Controller:notifySlotIndexMaybeChanged(index)
    if index == self._lastSlotIndex then
        return
    end
    if self._opening and self._slotWindow:isReady() then
        self._selectionTouched = true
    end
    self._lastSlotIndex = index
    self._detailWindow:setSlot(index)
end

function Controller:onSlotConfirm(slot)
    local slotNumber = slot + 1
    if self._mode == "save" then
        self:_handleSave(slotNumber)
    else
        self:_handleLoad(slotNumber)
    end
end

---@param slotNumber integer
function Controller:_handleSave(slotNumber)
    if self._getSaveSource == nil then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    local instance = self._getSaveSource()
    if instance == nil then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    Save.SaveSlot(slotNumber, instance, GameSystem.GetSavedScreenImage())
    AudioManager.playSound(GameSystem.GetSaveSE())
    self._detailWindow:refresh()
    self:_closeWithReason(CLOSE_REASON_SAVED)
end

---@param slotNumber integer
function Controller:_handleLoad(slotNumber)
    local filePath = Save.GetSavePath(slotNumber)
    if not os.path.isfile(filePath) then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    local instance = Save.LoadGame(filePath)
    if instance == nil then
        AudioManager.playSound(GameSystem.GetBuzzerSE())
        return
    end
    AudioManager.playSound(GameSystem.GetLoadSE())
    self:_closeWithReason(CLOSE_REASON_LOADED, function ()
        if self._onLoadedCallback ~= nil then
            self._onLoadedCallback(instance)
        end
    end)
end

---@param reason   string
---@param onHidden function | nil
function Controller:_closeWithReason(reason, onHidden)
    self:close(function ()
        if self._onCloseCallback ~= nil then
            self._onCloseCallback(reason)
        end
        if onHidden ~= nil then
            onHidden()
        end
    end)
end

function Controller:dispose()
    self._scanReader:cancel()
    self._opening = false
    self._transition:hideImmediate()
    self._getSaveSource = nil
    self._onCloseCallback = nil
    self._onLoadedCallback = nil
    super(Controller, self).dispose()
end

return Ui.DefineWindow(View, Controller, Canvas)
