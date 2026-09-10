local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local Save = require("Source.Save")
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

Controller.windowOptions = { position = sf.Vector2f.new(192, 0), hidden = true }

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

function Controller:open(transitionProfile)
    self._transitionProfile = transitionProfile or WindowTransition.DEFAULT
    self._mode = "load"
    if self._tabWindow ~= nil then
        self.ui.assets["TabsAsset"].controls["Tabs"]:setSelectedIndex(0)
    end
    self._slotWindow:resetSelection()
    local latestSlot = Save.FindLatestSlot(WindowSaveSlot.MAX_SAVE_SLOTS)
    if latestSlot ~= nil then
        local latestSlotIndex = latestSlot - 1
        ---@cast latestSlotIndex integer
        self._slotWindow:selectIndex(latestSlotIndex)
    end
    self._lastSlotIndex = nil
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
    self:notifySlotIndexMaybeChanged(self._slotWindow.index)
end

function Controller:close(onHidden)
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
    local filePath = Save.GetSavePath(slotNumber)
    local screenImage = GameSystem.GetSavedScreenImage()
    if screenImage ~= nil then
        local encoded = screenImage:saveToMemory("png")
        assert(bool(encoded), "Failed to encode save screenshot as PNG")
        instance:setScreenshot(encoded)
    else
        instance:setScreenshot(nil)
    end
    Save.SaveGame(filePath, instance)
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
    self._transition:hideImmediate()
    self._getSaveSource = nil
    self._onCloseCallback = nil
    self._onLoadedCallback = nil
    super(Controller, self).dispose()
end

return Ui.DefineWindow(View, Controller, Canvas)
