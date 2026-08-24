local CoreSystem = require("CoreSystem")
local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local GameSystem = require("Source.System")
local Save = require("Source.Save")
local WindowSaveCommand = require("Source.Windows.WindowSaveLoad.Command")
local WindowSaveDetail = require("Source.Windows.WindowSaveLoad.Detail")
local WindowSaveSlot = require("Source.Windows.WindowSaveLoad.Slot")

local ManagerFunctions = GlobalFunctions.Manager

local _DEFAULT_COMMAND_RECT = Engine.ToIntRect(192, 0, 416, 64)
local _DEFAULT_SLOT_RECT = Engine.ToIntRect(192, 64, 160, 256)
local _DEFAULT_DETAIL_RECT = Engine.ToIntRect(352, 64, 256, 256)

local CLOSE_REASON_CANCEL = "cancel"
local CLOSE_REASON_SAVED = "saved"
local CLOSE_REASON_LOADED = "loaded"
---@type function
local getSaveFileMTime
---@type function
local isNewerSaveFile
---@type function
local findLatestSaveSlotIndex

---@param slotIndex integer
---@param filePath  string
---@return Source.Windows.SaveFileMTime | nil
function getSaveFileMTime(slotIndex, filePath)
    if not CoreSystem.exists(filePath) then
        return nil
    end
    local modificationTime = os.path.getmtime(filePath)
    return { slotIndex, modificationTime }
end

---@param candidate Source.Windows.SaveFileMTime
---@param current   Source.Windows.SaveFileMTime | nil
---@return boolean
function isNewerSaveFile(candidate, current)
    if current == nil then
        return true
    end
    if candidate[2] ~= current[2] then
        return candidate[2] > current[2]
    end
    return candidate[1] < current[1]
end

---@param maxSlots integer
---@return integer | nil
function findLatestSaveSlotIndex(maxSlots)
    ---@type Source.Windows.SaveFileMTime | nil
    local latest = nil
    for slotIndex = 0, maxSlots - 1 do
        local filePath = Save.GetSavePath(slotIndex + 1)
        local result = getSaveFileMTime(slotIndex, filePath)
        if result ~= nil and isNewerSaveFile(result, latest) then
            latest = result
        end
    end
    return latest ~= nil and latest[1] or nil
end

---@class Source.Windows.WindowSaveLoad
local WindowSaveLoad = {}

function WindowSaveLoad:init(commandRect, slotRect, detailRect, loadOnly, getSaveSource, onClose, onLoaded)
    commandRect = commandRect or _DEFAULT_COMMAND_RECT
    slotRect = slotRect or _DEFAULT_SLOT_RECT
    detailRect = detailRect or _DEFAULT_DETAIL_RECT
    self._loadOnly = loadOnly == true
    self._getSaveSource = getSaveSource
    self._onCloseCallback = onClose
    self._onLoadedCallback = onLoaded
    self._mode = "load"
    self._commandWindow = self._loadOnly and nil or WindowSaveCommand.new(commandRect, self)
    self._slotWindow = WindowSaveSlot.new(slotRect, self)
    self._detailWindow = WindowSaveDetail.new(detailRect)
    if self._commandWindow ~= nil then
        self._commandWindow:setActive(false)
        self._commandWindow:setVisible(false)
    end
    self._slotWindow:setActive(false)
    self._slotWindow:setVisible(false)
    self._detailWindow:setActive(false)
    self._detailWindow:setVisible(false)
    self._lastSlotIndex = nil
    self:_selectLatestSaveSlot()
end

function WindowSaveLoad:getCommandWindow()
    return self._commandWindow
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
    if self._commandWindow ~= nil then
        self._commandWindow:setVisible(visible)
    end
    self._slotWindow:setVisible(visible)
    self._detailWindow:setVisible(visible)
end

function WindowSaveLoad:open()
    self:setVisible(true)
    self._lastSlotIndex = nil
    if self._loadOnly then
        self._mode = "load"
        self._slotWindow:setActive(true)
        self._slotWindow:requestKeyboardFocus()
    else
        assert(self._commandWindow ~= nil)
        self._commandWindow:setActive(true)
        self._slotWindow:setActive(false)
        self._commandWindow:requestKeyboardFocus()
    end
    self:notifySlotIndexMaybeChanged(self._slotWindow.index)
end

function WindowSaveLoad:close()
    self:setVisible(false)
    if self._commandWindow ~= nil then
        self._commandWindow:setActive(false)
    end
    self._slotWindow:setActive(false)
    self._detailWindow:setActive(false)
end

function WindowSaveLoad:closeByCancel()
    ManagerFunctions.playSE(GameSystem.GetCancelSE())
    self:_closeWithReason(CLOSE_REASON_CANCEL)
end

function WindowSaveLoad:cancelSlotSelection()
    if not self:returnToCommandWindow() then
        self:closeByCancel()
    end
end

function WindowSaveLoad:returnToCommandWindow(playSE)
    if playSE == nil then
        playSE = true
    end
    if self._loadOnly or self._commandWindow == nil then
        return false
    end
    if playSE then
        ManagerFunctions.playSE(GameSystem.GetCancelSE())
    end
    self:focusCommand()
    return true
end

---@param mode "load" | "save"
function WindowSaveLoad:onCommandConfirm(mode)
    ManagerFunctions.playSE(GameSystem.GetDecisionSE())
    self._mode = mode
    self:focusSlotList()
end

function WindowSaveLoad:focusSlotList()
    if self._commandWindow ~= nil then
        self._commandWindow:setActive(false)
    end
    self._slotWindow:setActive(true)
    self._lastSlotIndex = nil
    self:notifySlotIndexMaybeChanged(self._slotWindow.index)
    self._slotWindow:requestKeyboardFocusAtCursor()
end

function WindowSaveLoad:focusCommand()
    if self._commandWindow == nil then
        return
    end
    self._commandWindow:setActive(true)
    self._slotWindow:setActive(false)
    self._commandWindow:requestKeyboardFocus()
end

function WindowSaveLoad:notifySlotIndexMaybeChanged(index)
    if index == self._lastSlotIndex then
        return
    end
    self._lastSlotIndex = index
    self._detailWindow:setSlot(index)
end

function WindowSaveLoad:_selectLatestSaveSlot()
    local latestIndex = findLatestSaveSlotIndex(WindowSaveSlot.MAX_SAVE_SLOTS)
    if latestIndex ~= nil then
        self._slotWindow.index = latestIndex
    end
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

return class(WindowSaveLoad)
