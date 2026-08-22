local Engine = require("Engine")
local GlobalFunctions = require("GlobalFunctions")
local GameSystem = require("Source.System")
local ConfigWindowUI = require("Source.UI.ConfigWindow")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local ConfigSliderRowUI = require("Source.UI.Parts.ConfigWindow.ConfigSliderRow")

local Input = Engine.Input
local ManagerFunctions = GlobalFunctions.Manager

local _DEFAULT_RECT = Engine.ToIntRect(80, 48, 480, 320)
local _ROW_HEIGHT = 32
local _TOUCH_OWNER_LIST = "list"
local _TOUCH_OWNER_SLIDER = "slider"

---@class Source.Windows.ConfigWindow
local ConfigWindow = {}

function ConfigWindow:init(onClose)
    local windowSkin = ManagerFunctions.loadSystem(Engine.DefaultWindowskinName, false, nil, true):copyToImage()
    local contentWidth = _DEFAULT_RECT.size.x
    super(ConfigWindow, self).init(_DEFAULT_RECT, nil, contentWidth, _ROW_HEIGHT, windowSkin)
    self:setHasReturnBtn(true)
    self._ui = ConfigWindowUI.new(self, windowSkin)
    self._ui:attach()
    self._listView = self._ui:getListView()
    self._languageRow = self._ui:getLanguageRow()
    self._scaleRow = self._ui:getScaleRow()
    self._maximumRenderScaleRow = self._ui:getMaximumRenderScaleRow()
    self._framerateRow = self._ui:getFramerateRow()
    self._antiAliasingLevelRow = self._ui:getAntiAliasingLevelRow()
    self._verticalSyncRow = self._ui:getVerticalSyncRow()
    self._musicOnRow = self._ui:getMusicOnRow()
    self._musicVolumeRow = self._ui:getMusicVolumeRow()
    self._soundOnRow = self._ui:getSoundOnRow()
    self._soundVolumeRow = self._ui:getSoundVolumeRow()
    self._voiceOnRow = self._ui:getVoiceOnRow()
    self._voiceVolumeRow = self._ui:getVoiceVolumeRow()
    self._dropBoxRows = self._ui:getDropBoxRows()
    self._settingRows = self._ui:getSettingRows()
    self._onClose = onClose
    self._open = false
    self._capturedTouchSlider = nil
    self._capturedTouchSliderIndex = nil
    self._capturedTouchOwner = nil
    self:setVisible(false)
    self:setActive(false)
end

function ConfigWindow:getLanguageDropBox()
    return self._languageRow:getDropBox()
end

function ConfigWindow:getScaleDropBox()
    if self._scaleRow == nil then
        return nil
    end
    return self._scaleRow:getDropBox()
end

function ConfigWindow:getMaximumRenderScaleDropBox()
    return self._maximumRenderScaleRow:getDropBox()
end

function ConfigWindow:getFramerateDropBox()
    return self._framerateRow:getDropBox()
end

function ConfigWindow:getAntiAliasingLevelDropBox()
    return self._antiAliasingLevelRow:getDropBox()
end

function ConfigWindow:getVerticalSyncCheckBox()
    return self._verticalSyncRow:getCheckBox()
end

function ConfigWindow:getMusicOnCheckBox()
    return self._musicOnRow:getCheckBox()
end

function ConfigWindow:getMusicVolumeSlider()
    return self._musicVolumeRow:getSlider()
end

function ConfigWindow:getSoundOnCheckBox()
    return self._soundOnRow:getCheckBox()
end

function ConfigWindow:getSoundVolumeSlider()
    return self._soundVolumeRow:getSlider()
end

function ConfigWindow:getVoiceOnCheckBox()
    return self._voiceOnRow:getCheckBox()
end

function ConfigWindow:getVoiceVolumeSlider()
    return self._voiceVolumeRow:getSlider()
end

function ConfigWindow:isOpen()
    return self._open
end

function ConfigWindow:open()
    self._ui:refreshDisplayScaleOptions()
    self:_setSelectionInputPaused(false)
    self._open = true
    self:setVisible(true)
    self:setActive(true)
    self:_setSettingRowsActive(true)
    self:requestKeyboardFocus()
end

function ConfigWindow:close()
    self._open = false
    self:setActive(false)
    self:_setSettingRowsActive(false)
    self:_collapseAllDropBoxes()
    self:setVisible(false)
    if self._onClose ~= nil then
        self._onClose()
    end
end

function ConfigWindow:dispose()
    if self._ui ~= nil then
        self._ui:dispose()
        self._ui = nil
    end
    self._listView = nil
    self._languageRow = nil
    self._scaleRow = nil
    self._maximumRenderScaleRow = nil
    self._framerateRow = nil
    self._antiAliasingLevelRow = nil
    self._verticalSyncRow = nil
    self._musicOnRow = nil
    self._musicVolumeRow = nil
    self._soundOnRow = nil
    self._soundVolumeRow = nil
    self._voiceOnRow = nil
    self._voiceVolumeRow = nil
    self._dropBoxRows = nil
    self._settingRows = nil
    self._onClose = nil
    self._capturedTouchSlider = nil
    self._capturedTouchSliderIndex = nil
    self._capturedTouchOwner = nil
end

function ConfigWindow:_closeByCancel()
    ManagerFunctions.playSE(GameSystem.getCancelSE())
    self:close()
end

function ConfigWindow:onReturn()
    local expandedRow = self:_getExpandedSettingRow()
    if expandedRow ~= nil then
        expandedRow:getDropBox():cancel()
        return
    end
    self:_closeByCancel()
end

function ConfigWindow:onTick(deltaTime)
    self._rect:setVisible(false)
    self._ui:tick(deltaTime)
    super(ConfigWindow, self).onTick(deltaTime)
end

function ConfigWindow:onKeyDown(kwargs)
    if Input.isActionTriggered(Input.getCancelKeys(), false) then
        self:onReturn()
        Input.isActionTriggered(Input.getCancelKeys(), true)
        return
    end
    if self._selectionInputPaused then
        return
    end
    if self:_handleSelectedSliderKeyDown() then
        return
    end
    super(ConfigWindow, self).onKeyDown(kwargs)
end

function ConfigWindow:onMouseButtonDown(kwargs)
    if kwargs.button == sf.Mouse.Button.Right then
        self:onReturn()
        return true
    end
    return false
end

---@param row Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@return function
function ConfigWindow._makeSettingRowConfirmCallback(row)
    return function (_obj, _kwargs)
        row:getDropBox():open()
    end
end

---@param expanded boolean
function ConfigWindow:_onDropBoxExpandedChanged(expanded)
    if expanded then
        self:_setSelectionInputPaused(true)
        local expandedRow = self:_getExpandedSettingRow()
        for _, row in ipairs(self._settingRows) do
            row:setActive(row == expandedRow)
        end
    else
        self:_setSelectionInputPaused(false)
        if self._open then
            self:_setSettingRowsActive(true)
            self:requestKeyboardFocus()
        end
    end
end

---@return Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI | nil
function ConfigWindow:_getExpandedSettingRow()
    for _, row in ipairs(self._dropBoxRows) do
        if row:getDropBox():isExpanded() then
            return row
        end
    end
    return nil
end

---@return boolean
function ConfigWindow:_anyDropBoxExpanded()
    return self:_getExpandedSettingRow() ~= nil
end

function ConfigWindow:_collapseAllDropBoxes()
    for _, row in ipairs(self._dropBoxRows) do
        row:getDropBox():setExpanded(false)
    end
end

---@param active boolean
function ConfigWindow:_setSettingRowsActive(active)
    for _, row in ipairs(self._settingRows) do
        row:setActive(active)
    end
end

---@param checked boolean
function ConfigWindow:_onVerticalSyncCheckedChanged(checked)
    self._ui.onVerticalSyncCheckedChanged(checked)
end

---@param index integer
function ConfigWindow:_onLanguageSelectedIndexChanged(index)
    self._ui.onLanguageSelectedIndexChanged(index)
end

---@param index integer
function ConfigWindow:_onFrameRateSelectedIndexChanged(index)
    self._ui.onFrameRateSelectedIndexChanged(index)
end

---@param checked boolean
function ConfigWindow:_onMusicOnCheckedChanged(checked)
    self._ui.onMusicOnCheckedChanged(checked)
end

---@param value integer
function ConfigWindow:_onMusicVolumeChanged(value)
    self._ui.onMusicVolumeChanged(value)
end

---@param checked boolean
function ConfigWindow:_onSoundOnCheckedChanged(checked)
    self._ui.onSoundOnCheckedChanged(checked)
end

---@param value integer
function ConfigWindow:_onSoundVolumeChanged(value)
    self._ui.onSoundVolumeChanged(value)
end

---@param checked boolean
function ConfigWindow:_onVoiceOnCheckedChanged(checked)
    self._ui.onVoiceOnCheckedChanged(checked)
end

---@param value integer
function ConfigWindow:_onVoiceVolumeChanged(value)
    self._ui.onVoiceVolumeChanged(value)
end

---@return boolean
function ConfigWindow:_handleSelectedSliderKeyDown()
    local row = self:_getSelectedSettingRow()
    if not Class.isInstance(row, ConfigSliderRowUI) then
        return false
    end
    ---@cast row Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI
    local repeatDelay = 0.4
    local repeatInterval = 0.05
    if Input.isActionTriggered(Input.getLeftKeys(), false, repeatDelay, repeatInterval) then
        row:adjust(-1)
        Input.isActionTriggered(Input.getLeftKeys(), true, repeatDelay, repeatInterval)
        return true
    end
    if Input.isActionTriggered(Input.getRightKeys(), false, repeatDelay, repeatInterval) then
        row:adjust(1)
        Input.isActionTriggered(Input.getRightKeys(), true, repeatDelay, repeatInterval)
        return true
    end
    return false
end

---@param position sf.Vector2f
function ConfigWindow:_onCapturedTouchBegan(position)
    self._capturedTouchSlider = nil
    self._capturedTouchSliderIndex = nil
    self._capturedTouchOwner = nil
    for luaIndex, row in ipairs(self._settingRows) do
        if Class.isInstance(row, ConfigSliderRowUI) then
            local slider = row:getSlider()
            if slider:getVisible() and slider:getActive() and slider:getAbsoluteTouchHitBounds():contains(position) then
                self._capturedTouchSlider = slider
                self._capturedTouchSliderIndex = luaIndex - 1
                return
            end
        end
    end
end

---@param position sf.Vector2f
---@return boolean
function ConfigWindow:_handleCapturedTouchDrag(position)
    if self._capturedTouchSlider == nil then
        return false
    end
    if self._capturedTouchOwner == nil then
        ---@cast self._touchStartPosition sf.Vector2f
        local deltaX = position.x - self._touchStartPosition.x
        local deltaY = position.y - self._touchStartPosition.y
        if math.abs(deltaX) > math.abs(deltaY) then
            self._capturedTouchOwner = _TOUCH_OWNER_SLIDER
            ---@cast self._capturedTouchSliderIndex integer
            self:_setPointerIndex(self._capturedTouchSliderIndex)
        else
            self._capturedTouchOwner = _TOUCH_OWNER_LIST
        end
    end
    if self._capturedTouchOwner ~= _TOUCH_OWNER_SLIDER then
        return false
    end
    if self._capturedTouchSlider:getVisible() and self._capturedTouchSlider:getActive() then
        self._capturedTouchSlider:setValueFromBoundsPosition(self._capturedTouchSlider:getAbsoluteBounds(), position)
    end
    return true
end

---@param position sf.Vector2f
---@return boolean
function ConfigWindow:_handleCapturedTouchTap(position)
    if self._capturedTouchSlider == nil then
        return false
    end
    ---@cast self._capturedTouchSliderIndex integer
    self:_setPointerIndex(self._capturedTouchSliderIndex)
    if self._capturedTouchSlider:getVisible() and self._capturedTouchSlider:getActive() then
        self._capturedTouchSlider:setValueFromBoundsPosition(self._capturedTouchSlider:getAbsoluteBounds(), position)
    end
    return true
end

function ConfigWindow:_onCapturedTouchReset()
    self._capturedTouchSlider = nil
    self._capturedTouchSliderIndex = nil
    self._capturedTouchOwner = nil
end

---@return Source.UI.Parts.ConfigWindow.ConfigRow.ConfigRowControllerBase | nil
function ConfigWindow:_getSelectedSettingRow()
    if self._settingRows == nil or self.index == nil then
        return nil
    end
    if self.index >= 0 and self.index < #self._settingRows then
        return self._settingRows[self.index + 1]
    end
    return nil
end

---@param items table
---@param value string | number
---@return integer
function ConfigWindow._findSelectedIndex(items, value)
    local textValue = tostring(value)
    for luaIndex, item in ipairs(items) do
        if item == textValue then
            return luaIndex - 1
        end
    end
    local numericValue = tonumber(value)
    if numericValue == nil then
        return 0
    end
    for luaIndex, item in ipairs(items) do
        local numericItem = tonumber(item)
        if numericItem ~= nil and numericItem == numericValue then
            return luaIndex - 1
        end
    end
    return 0
end

-- Selection width spans the full content area.
---
--- - @return  Content width in logical UI units
---@return integer
function ConfigWindow:_getRectWidth()
    return self.content:getSize().x
end

-- Selection rect in content space; rows are full-width without ListView column inset.
---
--- - @param index  Zero-based item index.
--- - @return  Top-left of the selection rectangle in content space.
---@param index integer
---@return sf.Vector2f
function ConfigWindow:_getRectPositionForIndex(index)
    local columns = self:_getColumns()
    local x = index % columns * self._rectWidth
    local y = math.floor(index / columns) * self._rectHeight
    return sf.Vector2f.new(x, y)
end

return class(ConfigWindow, WindowSelectable)
