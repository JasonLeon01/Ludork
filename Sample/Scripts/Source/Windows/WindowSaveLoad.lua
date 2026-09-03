local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local GameSystem = require("Source.System")
local Save = require("Source.Save")
local WindowSaveLoadUI = require("Source.UI.WindowSaveLoad")
local WindowTransition = require("Source.UI.WindowTransition")
local WindowSaveDetail = require("Source.Windows.WindowSaveLoad.Detail")
local WindowSaveSlot = require("Source.Windows.WindowSaveLoad.Slot")
local WindowSaveTabs = require("Source.Windows.WindowSaveLoad.Tabs")

local ManagerFunctions = GlobalFunctions.Manager
local Canvas = Engine.Canvas

local _DEFAULT_TAB_RECT = Engine.ToIntRect(192, 0, 416, 64)
local _DEFAULT_SLOT_RECT = Engine.ToIntRect(192, 64, 160, 256)
local _DEFAULT_DETAIL_RECT = Engine.ToIntRect(352, 64, 256, 256)

local CLOSE_REASON_CANCEL = "cancel"
local CLOSE_REASON_SAVED = "saved"
local CLOSE_REASON_LOADED = "loaded"

---@class Source.Windows.WindowSaveLoad
local WindowSaveLoad = {}

function WindowSaveLoad:init(tabRect, slotRect, detailRect, loadOnly, getSaveSource, onClose, onLoaded)
    self._loadOnly = loadOnly == true
    tabRect = tabRect or _DEFAULT_TAB_RECT
    slotRect = slotRect or _DEFAULT_SLOT_RECT
    detailRect = detailRect or _DEFAULT_DETAIL_RECT
    local topLeft = tabRect.position
    local slotTop = 64
    if self._loadOnly then
        topLeft = slotRect.position
        slotTop = 0
    end
    super(WindowSaveLoad, self).init(Engine.ToIntRect(topLeft.x, topLeft.y, 416, 320))
    self._getSaveSource = getSaveSource
    self._onCloseCallback = onClose
    self._onLoadedCallback = onLoaded
    self._mode = "load"
    self._ui = WindowSaveLoadUI.new(self)
    self._ui:attach()
    self._transition = self._ui:createTransition(self)
    self._transitionProfile = WindowTransition.DEFAULT
    self._tabWindow = nil
    if not self._loadOnly then
        self._tabWindow = WindowSaveTabs.new(Engine.ToIntRect(0, 0, 416, 64), self, self._ui:getTabsAsset())
        self:addChild(self._tabWindow)
    else
        local tabsRoot = assert(self._ui:getTabsAsset():getRoot(), "Save/load tabs root is unavailable")
        tabsRoot:setVisible(false)
    end
    self._slotWindow = WindowSaveSlot.new(Engine.ToIntRect(0, slotTop, 160, 256), self, self._ui:getSlotAsset())
    self._detailWindow = WindowSaveDetail.new(Engine.ToIntRect(160, slotTop, 256, 256), self._ui:getDetailAsset())
    self:addChild(self._slotWindow)
    self:addChild(self._detailWindow)
    if self._tabWindow ~= nil then
        self._tabWindow:setActive(false)
        self._tabWindow:setVisible(false)
    end
    self._slotWindow:setActive(false)
    self._slotWindow:setVisible(false)
    self._detailWindow:setActive(false)
    self._detailWindow:setVisible(false)
    self._lastSlotIndex = nil
    self._transition:hideImmediate()
end

function WindowSaveLoad:getTabWindow()
    return self._tabWindow
end

function WindowSaveLoad:getSlotWindow()
    return self._slotWindow
end

function WindowSaveLoad:getDetailWindow()
    return self._detailWindow
end

function WindowSaveLoad:getVisible()
    return self._transition:isBlocking()
end

function WindowSaveLoad:setVisible(visible)
    super(WindowSaveLoad, self).setVisible(visible)
    if self._tabWindow ~= nil then
        self._tabWindow:setVisible(visible)
    end
    self._slotWindow:setVisible(visible)
    self._detailWindow:setVisible(visible)
end

function WindowSaveLoad:open(transitionProfile)
    self._transitionProfile = transitionProfile or WindowTransition.DEFAULT
    self._mode = "load"
    if self._tabWindow ~= nil then
        self._tabWindow:getTabView():setSelectedIndex(0)
    end
    self._slotWindow:resetSelection()
    local latestSlot = Save.FindLatestSlot(WindowSaveSlot.MAX_SAVE_SLOTS)
    if latestSlot ~= nil then
        local latestSlotIndex = latestSlot - 1
        ---@cast latestSlotIndex integer
        self._slotWindow.index = latestSlotIndex
        self._slotWindow._oldIndex = latestSlotIndex
    end
    self._lastSlotIndex = nil
    if self._tabWindow ~= nil then
        self._tabWindow:setActive(false)
    end
    self._slotWindow:setActive(false)
    self._detailWindow:setActive(false)
    local fadeIn = WindowTransition.GetAnimationNames(self._transitionProfile)
    self._transition:show(fadeIn, function ()
        self:setActive(true)
        if self._tabWindow ~= nil then
            self._tabWindow:setActive(true)
        end
        self._slotWindow:setActive(true)
        self._slotWindow:requestKeyboardFocusAtCursor()
    end)
    self:notifySlotIndexMaybeChanged(self._slotWindow.index)
end

function WindowSaveLoad:close(onHidden)
    if self._tabWindow ~= nil then
        self._tabWindow:setActive(false)
    end
    self._slotWindow:setActive(false)
    self._detailWindow:setActive(false)
    local _, fadeOut = WindowTransition.GetAnimationNames(self._transitionProfile)
    self._transition:hide(fadeOut, onHidden)
end

function WindowSaveLoad:closeByCancel()
    ManagerFunctions.playSE(GameSystem.GetCancelSE())
    self:_closeWithReason(CLOSE_REASON_CANCEL)
end

function WindowSaveLoad:handleTabNavigationInput()
    if self._tabWindow == nil then
        return false
    end
    return self._tabWindow:handleNavigationInput()
end

---@param index integer
function WindowSaveLoad:onTabSelected(index)
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

function WindowSaveLoad:notifySlotIndexMaybeChanged(index)
    if index == self._lastSlotIndex then
        return
    end
    self._lastSlotIndex = index
    self._detailWindow:setSlot(index)
end

function WindowSaveLoad:onSlotConfirm(slot)
    local slotNumber = slot + 1
    if self._mode == "save" then
        self:_handleSave(slotNumber)
    else
        self:_handleLoad(slotNumber)
    end
end

---@param slotNumber integer
function WindowSaveLoad:_handleSave(slotNumber)
    if self._getSaveSource == nil then
        ManagerFunctions.playSE(GameSystem.GetBuzzerSE())
        return
    end
    local instance = self._getSaveSource()
    if instance == nil then
        ManagerFunctions.playSE(GameSystem.GetBuzzerSE())
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
    ManagerFunctions.playSE(GameSystem.GetSaveSE())
    self._detailWindow:refresh()
    self:_closeWithReason(CLOSE_REASON_SAVED)
end

---@param slotNumber integer
function WindowSaveLoad:_handleLoad(slotNumber)
    local filePath = Save.GetSavePath(slotNumber)
    if not os.path.isfile(filePath) then
        ManagerFunctions.playSE(GameSystem.GetBuzzerSE())
        return
    end
    local instance = Save.LoadGame(filePath)
    if instance == nil then
        ManagerFunctions.playSE(GameSystem.GetBuzzerSE())
        return
    end
    ManagerFunctions.playSE(GameSystem.GetLoadSE())
    self:_closeWithReason(CLOSE_REASON_LOADED, function ()
        if self._onLoadedCallback ~= nil then
            self._onLoadedCallback(instance)
        end
    end)
end

---@param reason   string
---@param onHidden function | nil
function WindowSaveLoad:_closeWithReason(reason, onHidden)
    self:close(function ()
        if self._onCloseCallback ~= nil then
            self._onCloseCallback(reason)
        end
        if onHidden ~= nil then
            onHidden()
        end
    end)
end

function WindowSaveLoad:dispose()
    self._transition:hideImmediate()
    if self._tabWindow ~= nil then
        self._tabWindow:dispose()
    end
    self._slotWindow:dispose()
    self._detailWindow:dispose()
    self._ui:dispose()
    self._getSaveSource = nil
    self._onCloseCallback = nil
    self._onLoadedCallback = nil
end

return class(WindowSaveLoad, Canvas)
