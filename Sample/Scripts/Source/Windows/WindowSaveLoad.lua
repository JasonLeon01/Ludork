local CoreSystem = require("CoreSystem")
local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local GameSystem = require("Source.System")
local Save = require("Source.Save")
local WindowSaveDetail = require("Source.Windows.WindowSaveLoad.Detail")
local WindowSaveSlot = require("Source.Windows.WindowSaveLoad.Slot")
local WindowSaveTabs = require("Source.Windows.WindowSaveLoad.Tabs")

local ManagerFunctions = GlobalFunctions.Manager

local _DEFAULT_TAB_RECT = Engine.ToIntRect(192, 0, 416, 64)
local _DEFAULT_SLOT_RECT = Engine.ToIntRect(192, 64, 160, 256)
local _DEFAULT_DETAIL_RECT = Engine.ToIntRect(352, 64, 256, 256)

local CLOSE_REASON_CANCEL = "cancel"
local CLOSE_REASON_SAVED = "saved"
local CLOSE_REASON_LOADED = "loaded"

---@class Source.Windows.WindowSaveLoad
local WindowSaveLoad = {}

function WindowSaveLoad:init(tabRect, slotRect, detailRect, loadOnly, getSaveSource, onClose, onLoaded)
    tabRect = tabRect or _DEFAULT_TAB_RECT
    slotRect = slotRect or _DEFAULT_SLOT_RECT
    detailRect = detailRect or _DEFAULT_DETAIL_RECT
    self._loadOnly = loadOnly == true
    self._getSaveSource = getSaveSource
    self._onCloseCallback = onClose
    self._onLoadedCallback = onLoaded
    self._mode = "load"
    self._tabWindow = nil
    if not self._loadOnly then
        self._tabWindow = WindowSaveTabs.new(tabRect, self)
    end
    self._slotWindow = WindowSaveSlot.new(slotRect, self)
    self._detailWindow = WindowSaveDetail.new(detailRect)
    if self._tabWindow ~= nil then
        self._tabWindow:setActive(false)
        self._tabWindow:setVisible(false)
    end
    self._slotWindow:setActive(false)
    self._slotWindow:setVisible(false)
    self._detailWindow:setActive(false)
    self._detailWindow:setVisible(false)
    self._lastSlotIndex = nil
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
    return self._slotWindow:getVisible()
end

function WindowSaveLoad:setVisible(visible)
    if self._tabWindow ~= nil then
        self._tabWindow:setVisible(visible)
    end
    self._slotWindow:setVisible(visible)
    self._detailWindow:setVisible(visible)
end

function WindowSaveLoad:open()
    self._mode = "load"
    if self._tabWindow ~= nil then
        self._tabWindow:getTabView():setSelectedIndex(0)
    end
    self._slotWindow:resetSelection()
    self:setVisible(true)
    self._lastSlotIndex = nil
    if self._tabWindow ~= nil then
        self._tabWindow:setActive(true)
    end
    self._slotWindow:setActive(true)
    self._slotWindow:requestKeyboardFocusAtCursor()
    self:notifySlotIndexMaybeChanged(self._slotWindow.index)
end

function WindowSaveLoad:close()
    self:setVisible(false)
    if self._tabWindow ~= nil then
        self._tabWindow:setActive(false)
    end
    self._slotWindow:setActive(false)
    self._detailWindow:setActive(false)
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
    if not CoreSystem.exists(filePath) then
        ManagerFunctions.playSE(GameSystem.GetBuzzerSE())
        return
    end
    local instance = Save.LoadGame(filePath)
    if instance == nil then
        ManagerFunctions.playSE(GameSystem.GetBuzzerSE())
        return
    end
    ManagerFunctions.playSE(GameSystem.GetLoadSE())
    self:_closeWithReason(CLOSE_REASON_LOADED)
    if self._onLoadedCallback ~= nil then
        self._onLoadedCallback(instance)
    end
end

---@param reason string
function WindowSaveLoad:_closeWithReason(reason)
    self:close()
    if self._onCloseCallback ~= nil then
        self._onCloseCallback(reason)
    end
end

function WindowSaveLoad:dispose()
    self:close()
    if self._tabWindow ~= nil then
        self._tabWindow:dispose()
    end
    self._slotWindow:dispose()
    self._detailWindow:dispose()
    self._getSaveSource = nil
    self._onCloseCallback = nil
    self._onLoadedCallback = nil
end

return class(WindowSaveLoad)
