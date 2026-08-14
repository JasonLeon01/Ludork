local CoreSystem = require("CoreSystem")
local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local GameSystem = require("Source.System")
local Save = require("Source.Save")
local WindowSaveDetailUI = require("Source.UI.Parts.WindowSaveLoad.WindowSaveDetail")
local WindowSaveSlotUI = require("Source.UI.Parts.WindowSaveLoad.WindowSaveSlot")
local WindowCommand = require("Source.Windows.WindowCommand")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local WindowBase = require("Source.Windows.Base.WindowBase")

local Input = Engine.Input
local ManagerFunctions = GlobalFunctions.Manager

local _SLOT_ROW_HEIGHT = 32
local MAX_SAVE_SLOTS = 100

local _DETAIL_WINDOW_SIZE = 256
local _DETAIL_THUMB_WIDTH = 224
local _DETAIL_THUMB_HEIGHT = 168

local _DEFAULT_COMMAND_RECT = Engine.ToIntRect(192, 0, 416, 64)
local _DEFAULT_SLOT_RECT = Engine.ToIntRect(192, 64, 160, 256)
local _DEFAULT_DETAIL_RECT = Engine.ToIntRect(352, 64, _DETAIL_WINDOW_SIZE, _DETAIL_WINDOW_SIZE)

local CLOSE_REASON_CANCEL = "cancel"
local CLOSE_REASON_SAVED = "saved"
local CLOSE_REASON_LOADED = "loaded"

local WindowSaveCommandController = {}

function WindowSaveCommandController.createCommands(owner)
    return {
        {
            key = "Load",
            localeKey = "MENU_LOAD",
            callback = function (_obj, _kwargs)
                owner:onCommandConfirm("load")
            end
        },
        {
            key = "Save",
            localeKey = "MENU_SAVE",
            callback = function (_obj, _kwargs)
                owner:onCommandConfirm("save")
            end
        }
    }
end

local FinalWindowSaveCommandController = class(WindowSaveCommandController, WindowCommand.Controller)

local WindowSaveLoadExports = {}

---@param slotIndex integer
---@param filePath  string
---@return Source.Windows.SaveFileMTime | nil
function WindowSaveLoadExports._getSaveFileMTime(slotIndex, filePath)
    if not CoreSystem.exists(filePath) then
        return nil
    end
    local modificationTime = os.path.getmtime(filePath)
    return { slotIndex, modificationTime }
end

---@param candidate Source.Windows.SaveFileMTime
---@param current   Source.Windows.SaveFileMTime | nil
---@return boolean
function WindowSaveLoadExports._isNewerSaveFile(candidate, current)
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
function WindowSaveLoadExports._findLatestSaveSlotIndex(maxSlots)
    ---@type Source.Windows.SaveFileMTime | nil
    local latest = nil
    for slotIndex = 0, maxSlots - 1 do
        local filePath = Save.GetSavePath(slotIndex + 1)
        local result = WindowSaveLoadExports._getSaveFileMTime(slotIndex, filePath)
        if result ~= nil and WindowSaveLoadExports._isNewerSaveFile(result, latest) then
            latest = result
        end
    end
    return latest ~= nil and latest[1] or nil
end

local WindowSaveCommand = {}

WindowSaveCommand.controllerClass = FinalWindowSaveCommandController

function WindowSaveCommand:init(rect, owner)
    local commands = FinalWindowSaveCommandController.createCommands(owner)
    super(WindowSaveCommand, self).init(rect, commands, nil, 32, nil, nil, 2)
    self._owner = owner
end

function WindowSaveCommand:onKeyDown(kwargs)
    if Input.isActionTriggered(Input.getCancelKeys(), false) then
        self._owner:closeByCancel()
        Input.isActionTriggered(Input.getCancelKeys(), true)
        return
    end
    super(WindowSaveCommand, self).onKeyDown(kwargs)
end

function WindowSaveCommand:onMouseButtonDown(kwargs)
    if kwargs.button == sf.Mouse.Button.Right then
        self._owner:closeByCancel()
        return true
    end
    return false
end

local FinalWindowSaveCommand = class(WindowSaveCommand, WindowCommand)

local WindowSaveSlot = {}

function WindowSaveSlot:init(rect, owner)
    super(WindowSaveSlot, self).init(rect, nil, nil, _SLOT_ROW_HEIGHT)
    self._owner = owner
    self._ui = WindowSaveSlotUI.new(self, rect.size, MAX_SAVE_SLOTS)
    self._ui:attach()
    self._listView = self._ui:getListView()
end

function WindowSaveSlot:onKeyDown(kwargs)
    if Input.isActionTriggered(Input.getCancelKeys(), false) then
        self._owner:cancelSlotSelection()
        Input.isActionTriggered(Input.getCancelKeys(), true)
        return
    end
    super(WindowSaveSlot, self).onKeyDown(kwargs)
end

function WindowSaveSlot:onTick(deltaTime)
    super(WindowSaveSlot, self).onTick(deltaTime)
    self._owner:notifySlotIndexMaybeChanged(self.index)
end

function WindowSaveSlot:onMouseButtonDown(kwargs)
    if kwargs.button == sf.Mouse.Button.Right then
        self._owner:cancelSlotSelection()
        return true
    end
    return false
end

local FinalWindowSaveSlot = class(WindowSaveSlot, WindowSelectable)

local WindowSaveDetail = {}

function WindowSaveDetail:init(rect)
    super(WindowSaveDetail, self).init(rect)
    self._currentSlot = nil
    self._cachedFilePath = ""
    self._cachedFileMTime = -1.0
    self._thumbTexture = nil
    self._ui = WindowSaveDetailUI.new(self, rect.size)
    self._ui:attach()
    self._thumbnail = self._ui:getThumbnail()
    self._timestampText = self._ui:getTimestampText()
    self._thumbnail:setVisible(false)
    self._timestampText:setVisible(false)
end

function WindowSaveDetail:setSlot(slot)
    if slot == self._currentSlot then
        self:_refreshIfFileChanged()
        return
    end
    self._currentSlot = slot
    self._cachedFilePath = ""
    self._cachedFileMTime = -1.0
    self:_refreshContent()
end

function WindowSaveDetail:refresh()
    self._cachedFilePath = ""
    self._cachedFileMTime = -1.0
    self:_refreshContent()
end

function WindowSaveDetail:onTick(deltaTime)
    super(WindowSaveDetail, self).onTick(deltaTime)
    self:_refreshIfFileChanged()
end

---@return sf.Vector2u
function WindowSaveDetail._thumbSize()
    local size = sf.Vector2u.new(_DETAIL_THUMB_WIDTH, _DETAIL_THUMB_HEIGHT)
    ---@cast size sf.Vector2u
    return size
end

function WindowSaveDetail:_refreshIfFileChanged()
    if self._currentSlot == nil then
        return
    end
    local slotNumber = self._currentSlot + 1
    ---@cast slotNumber integer
    local filePath = Save.GetSavePath(slotNumber)
    if not CoreSystem.exists(filePath) then
        if bool(self._cachedFilePath) or self._thumbnail:getVisible() then
            self._cachedFilePath = ""
            self._cachedFileMTime = -1.0
            self:_hideContent()
        end
        return
    end
    local modificationTime = os.path.getmtime(filePath)
    if filePath == self._cachedFilePath and modificationTime == self._cachedFileMTime then
        return
    end
    self._cachedFilePath = filePath
    self._cachedFileMTime = modificationTime
    self:_loadAndDisplay(filePath, modificationTime)
end

function WindowSaveDetail:_refreshContent()
    if self._currentSlot == nil then
        self:_hideContent()
        return
    end
    local slotNumber = self._currentSlot + 1
    ---@cast slotNumber integer
    local filePath = Save.GetSavePath(slotNumber)
    if not CoreSystem.exists(filePath) then
        self:_hideContent()
        return
    end
    local modificationTime = os.path.getmtime(filePath)
    self._cachedFilePath = filePath
    self._cachedFileMTime = modificationTime
    self:_loadAndDisplay(filePath, modificationTime)
end

---@param filePath         string
---@param modificationTime number
function WindowSaveDetail:_loadAndDisplay(filePath, modificationTime)
    local instance = Save.LoadGame(filePath)
    if instance == nil then
        self:_hideContent()
        return
    end
    local screenshot = instance:getScreenshot()
    if not self:_applyScreenshot(screenshot) then
        self._thumbnail:setVisible(false)
    end
    self._ui:setModificationTime(modificationTime)
    self._timestampText:setVisible(true)
end

---@param screenshot integer[] | nil
---@return boolean
function WindowSaveDetail:_applyScreenshot(screenshot)
    if not bool(screenshot) then
        return false
    end
    local image = sf.Image.new(screenshot)
    local imageSize = image:getSize()
    assert(imageSize.x > 0 and imageSize.y > 0, "Save screenshot image has invalid dimensions")
    local texture = sf.Texture.new(image)
    texture:setSmooth(true)
    self._thumbTexture = texture
    self._thumbnail:setTexture(texture, true)
    local scaleX = _DETAIL_THUMB_WIDTH / imageSize.x
    local scaleY = _DETAIL_THUMB_HEIGHT / imageSize.y
    self._thumbnail:setScale(sf.Vector2f.new(scaleX, scaleY))
    self._thumbnail:setPosition(sf.Vector2f.new(0.0, 0.0))
    self._thumbnail:setVisible(true)
    return true
end

function WindowSaveDetail:_hideContent()
    self._thumbnail:setVisible(false)
    self._timestampText:setVisible(false)
    self._ui:setTimestamp("")
end

---@param modificationTime number
---@return string
function WindowSaveDetail._formatTimestamp(modificationTime)
    return WindowSaveDetailUI.formatTimestamp(modificationTime)
end

local FinalWindowSaveDetail = class(WindowSaveDetail, WindowBase)

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
    self._commandWindow = self._loadOnly and nil or FinalWindowSaveCommand.new(commandRect, self)
    self._slotWindow = FinalWindowSaveSlot.new(slotRect, self)
    self._detailWindow = FinalWindowSaveDetail.new(detailRect)
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
    ManagerFunctions.playSE(GameSystem.getCancelSE())
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
        ManagerFunctions.playSE(GameSystem.getCancelSE())
    end
    self:focusCommand()
    return true
end

---@param mode "load" | "save"
function WindowSaveLoad:onCommandConfirm(mode)
    ManagerFunctions.playSE(GameSystem.getDecisionSE())
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
    local latestIndex = WindowSaveLoadExports._findLatestSaveSlotIndex(MAX_SAVE_SLOTS)
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
        ManagerFunctions.playSE(GameSystem.getBuzzerSE())
        return
    end
    local instance = self._getSaveSource()
    if instance == nil then
        ManagerFunctions.playSE(GameSystem.getBuzzerSE())
        return
    end
    local filePath = Save.GetSavePath(slotNumber)
    local screenImage = GameSystem.getSavedScreenImage()
    if screenImage ~= nil then
        local encoded = screenImage:saveToMemory("png")
        assert(bool(encoded), "Failed to encode save screenshot as PNG")
        instance:setScreenshot(encoded)
    else
        instance:setScreenshot(nil)
    end
    Save.SaveGame(filePath, instance)
    ManagerFunctions.playSE(GameSystem.getSaveSE())
    self._detailWindow:refresh()
    self:_closeWithReason(CLOSE_REASON_SAVED)
end

---@param slotNumber integer
function WindowSaveLoad:_handleLoad(slotNumber)
    local filePath = Save.GetSavePath(slotNumber)
    if not CoreSystem.exists(filePath) then
        ManagerFunctions.playSE(GameSystem.getBuzzerSE())
        return
    end
    local instance = Save.LoadGame(filePath)
    if instance == nil then
        ManagerFunctions.playSE(GameSystem.getBuzzerSE())
        return
    end
    ManagerFunctions.playSE(GameSystem.getLoadSE())
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

local FinalWindowSaveLoad = class(WindowSaveLoad)

WindowSaveLoadExports.MAX_SAVE_SLOTS = MAX_SAVE_SLOTS
WindowSaveLoadExports.CLOSE_REASON_CANCEL = CLOSE_REASON_CANCEL
WindowSaveLoadExports.CLOSE_REASON_SAVED = CLOSE_REASON_SAVED
WindowSaveLoadExports.CLOSE_REASON_LOADED = CLOSE_REASON_LOADED
WindowSaveLoadExports.WindowSaveCommand = FinalWindowSaveCommand
WindowSaveLoadExports.WindowSaveSlot = FinalWindowSaveSlot
WindowSaveLoadExports.WindowSaveDetail = FinalWindowSaveDetail
WindowSaveLoadExports.WindowSaveLoad = FinalWindowSaveLoad

return WindowSaveLoadExports
