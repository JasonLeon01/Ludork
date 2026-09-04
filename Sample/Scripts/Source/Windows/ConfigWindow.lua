local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local ConfigWindowUI = require("Source.UI.ConfigWindow")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local ConfigSliderRowUI = require("Source.UI.Parts.ConfigWindow.ConfigSliderRow")

local Input = Engine.Input
local Direction = Engine.FocusDirection
local AudioManager = GlobalCore.AudioManager
local TextureManager = GlobalCore.TextureManager

local _DEFAULT_RECT = Engine.ToIntRect(80, 48, 480, 384)
local _ROW_HEIGHT = 32
local _GRAPHICS_PAGE_INDEX = 0

---@param index          integer
---@param scaleRowChange integer
---@return integer
local function adjustGraphicsSettingIndex(index, scaleRowChange)
    if scaleRowChange > 0 and index >= 1 then
        return index + 1
    end
    if scaleRowChange < 0 and index > 1 then
        return index - 1
    end
    return index
end

---@class Source.Windows.ConfigWindow
local ConfigWindow = {}

function ConfigWindow:init(onClose)
    self._activePageIndex = _GRAPHICS_PAGE_INDEX
    self._pageSessions = {
        { index = 0, scrollOffset = sf.Vector2f.new(0.0, 0.0) }, { index = 0, scrollOffset = sf.Vector2f.new(0.0, 0.0) },
        { index = 0, scrollOffset = sf.Vector2f.new(0.0, 0.0) }
    }
    local windowSkin = assert(TextureManager.load(
        assert(Engine.DefaultWindowskinName, "Default windowskin path is unavailable"), false, nil, true
    ), "Default windowskin texture is unavailable"):copyToImage()
    local contentWidth = _DEFAULT_RECT.size.x - 32
    super(ConfigWindow, self).init(_DEFAULT_RECT, nil, contentWidth, _ROW_HEIGHT, windowSkin, nil, nil, nil, true)
    self:setHasReturnBtn(true)
    self._ui = ConfigWindowUI.new(self, windowSkin)
    self._ui:attach()
    self._windowContent = self.content
    self._settingsContent = self._ui:getSettingsContent()
    self._tabView = self._ui:getTabView()
    self.content = self._settingsContent
    self:setScrollBox(self._ui:getSettingsScrollBox())
    self:setListView(self._ui:getPage(self._activePageIndex).list)
    self._languageRow = self._ui:getLanguageRow()
    self._graphicsPresetRow = self._ui:getGraphicsPresetRow()
    self._maximumRenderScaleRow = self._ui:getMaximumRenderScaleRow()
    self._framerateRow = self._ui:getFramerateRow()
    self._antiAliasingLevelRow = self._ui:getAntiAliasingLevelRow()
    self._lightingRenderScaleRow = self._ui:getLightingRenderScaleRow()
    self._verticalSyncRow = self._ui:getVerticalSyncRow()
    self._musicOnRow = self._ui:getMusicOnRow()
    self._musicVolumeRow = self._ui:getMusicVolumeRow()
    self._soundOnRow = self._ui:getSoundOnRow()
    self._soundVolumeRow = self._ui:getSoundVolumeRow()
    self._voiceOnRow = self._ui:getVoiceOnRow()
    self._voiceVolumeRow = self._ui:getVoiceVolumeRow()
    self._onClose = onClose
    self._open = false
    self._tabNavigationHandledThisFrame = false
    self._ui:setActivePage(self._activePageIndex)
    self:_refreshControlActivity()
    self:hideImmediate()
end

function ConfigWindow:getLanguageDropBox()
    return self._languageRow:getDropBox()
end

function ConfigWindow:getGraphicsPresetDropBox()
    return self._graphicsPresetRow:getDropBox()
end

function ConfigWindow:getScaleDropBox()
    local scaleRow = self._ui:getScaleRow()
    if scaleRow == nil then
        return nil
    end
    return scaleRow:getDropBox()
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

function ConfigWindow:getLightingRenderScaleDropBox()
    return self._lightingRenderScaleRow:getDropBox()
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
    self:_applyScaleRowChange(self._ui:refreshDisplayScaleOptions())
    for _, session in ipairs(self._pageSessions) do
        session.index = 0
        session.scrollOffset = sf.Vector2f.new(0.0, 0.0)
    end
    self:_setSelectionInputPaused(false)
    self._activePageIndex = _GRAPHICS_PAGE_INDEX
    self._ui:setActivePage(self._activePageIndex)
    self._tabView:setSelectedIndex(self._activePageIndex)
    self:setListView(self:_getActivePage().list)
    self._open = true
    self:_restorePageScroll()
    self:resetSelection()
    self:_refreshControlActivity()
    self:showWithAnimation("FadeIn", function ()
        self:setActive(true)
        self:_refreshControlActivity()
        self:requestKeyboardFocusAtCursor()
    end)
end

function ConfigWindow:close()
    self._open = false
    self:_collapseAllDropBoxes()
    self:_setSelectionInputPaused(false)
    self:setActive(false)
    self:_refreshControlActivity()
    self:hideWithAnimation("FadeOut", function ()
        if self._onClose ~= nil then
            self._onClose()
        end
    end)
end

function ConfigWindow:dispose()
    self:hideImmediate()
    self:_detachSelectionRect()
    self.content = self._windowContent
    if self._ui ~= nil then
        self._ui:dispose()
        self._ui = nil
    end
    self._windowContent = nil
    self._settingsContent = nil
    self._tabView = nil
    self:setListView(nil)
    self._languageRow = nil
    self._graphicsPresetRow = nil
    self._maximumRenderScaleRow = nil
    self._framerateRow = nil
    self._antiAliasingLevelRow = nil
    self._lightingRenderScaleRow = nil
    self._verticalSyncRow = nil
    self._musicOnRow = nil
    self._musicVolumeRow = nil
    self._soundOnRow = nil
    self._soundVolumeRow = nil
    self._voiceOnRow = nil
    self._voiceVolumeRow = nil
    self._pageSessions = nil
    self._onClose = nil
    self._tabNavigationHandledThisFrame = nil
end

function ConfigWindow:_closeByCancel()
    AudioManager.playSound(GameSystem.GetCancelSE())
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

function ConfigWindow:update(deltaTime)
    self._tabNavigationHandledThisFrame = false
    super(ConfigWindow, self).update(deltaTime)
end

function ConfigWindow:onTick(deltaTime)
    if self._open then
        self:_applyScaleRowChange(self._ui:syncDisplayScaleAvailability())
    end
    self._ui:tick(deltaTime)
    super(ConfigWindow, self).onTick(deltaTime)
end

---@param scaleRowChange integer
function ConfigWindow:_applyScaleRowChange(scaleRowChange)
    if scaleRowChange == 0 then
        return
    end
    local session = self:_getPageSession(_GRAPHICS_PAGE_INDEX)
    session.index = adjustGraphicsSettingIndex(session.index, scaleRowChange)
    if self._activePageIndex == _GRAPHICS_PAGE_INDEX then
        self.index = adjustGraphicsSettingIndex(self.index or 0, scaleRowChange)
        self._oldIndex = self.index
        self._ensureSelectionVisibleRequested = true
    end
    self:_refreshControlActivity()
end

function ConfigWindow:onKeyDown(kwargs)
    if self._tabNavigationHandledThisFrame then
        return
    end
    if Input.isActionTriggered(Input.getCancelKeys(), false) then
        self:onReturn()
        Input.isActionTriggered(Input.getCancelKeys(), true)
        return
    end
    if self._selectionInputPaused then
        return
    end
    if self:_handleTabNavigation() then
        return
    end
    if self:_handleSelectedSliderKeyDown() then
        return
    end
    super(ConfigWindow, self).onKeyDown(kwargs)
end

function ConfigWindow:onDirectionalKey(direction)
    if direction == Direction.UP then
        if self.index == 0 then
            return false
        end
        return self:_setIndexIfChanged(self.index - 1)
    end
    if direction == Direction.DOWN then
        if self.index + 1 >= self:_itemCount() then
            return false
        end
        return self:_setIndexIfChanged(self.index + 1)
    end
    return false
end

---@param index integer
function ConfigWindow:_setPointerIndex(index)
    self.index = index
    self:_synchronizeSelectionScrollState()
end

---@return boolean
function ConfigWindow:_handleTabNavigation()
    if self._tabNavigationHandledThisFrame then
        return true
    end
    if not self._open then
        return false
    end
    local handled = self._tabView:handleNavigationInput()
    if handled then
        self._tabNavigationHandledThisFrame = true
    end
    return handled
end

---@param tabIndex integer
function ConfigWindow:_onTabSelected(tabIndex)
    self._tabNavigationHandledThisFrame = true
    if tabIndex == self._activePageIndex then
        return
    end
    self:_savePageSession()
    local expandedRow = self:_getExpandedSettingRow()
    if expandedRow ~= nil then
        expandedRow:getDropBox():setExpanded(false)
    end
    self._activePageIndex = tabIndex
    self._ui:setActivePage(tabIndex)
    self:setListView(self:_getActivePage().list)
    local session = self:_getPageSession(tabIndex)
    session.index = Engine.ToInteger(Engine.Clamp(session.index, 0, math.max(0, #self:_getActivePage().rows - 1)))
    local scrollBox = assert(self:getScrollBox())
    local maximum = scrollBox:getMaxScrollOffset()
    session.scrollOffset = sf.Vector2f.new(
        Engine.Clamp(session.scrollOffset.x, 0.0, maximum.x), Engine.Clamp(session.scrollOffset.y, 0.0, maximum.y)
    )
    self.index = session.index
    self._oldIndex = self.index
    self._ensureSelectionVisibleRequested = true
    self:_restorePageScroll()
    self:_refreshControlActivity()
    if self._open then
        self:requestKeyboardFocus()
    end
end

---@param row Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@return function
function ConfigWindow.MakeSettingRowConfirmCallback(row)
    return function (_obj, _kwargs)
        row:getDropBox():open()
    end
end

---@param expanded boolean
function ConfigWindow:_onDropBoxExpandedChanged(expanded)
    if expanded then
        self:_setSelectionInputPaused(true)
        local expandedRow = self:_getExpandedSettingRow()
        local page = self:_getActivePage()
        for _, row in ipairs(page.rows) do
            row:setActive(row == expandedRow)
        end
    else
        self:_setSelectionInputPaused(false)
        self:_refreshControlActivity()
        if self._open then
            self:requestKeyboardFocus()
        end
    end
end

---@return Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI | nil
function ConfigWindow:_getExpandedSettingRow()
    local page = self:_getActivePage()
    for _, row in ipairs(page.dropBoxRows) do
        if row:getDropBox():isExpanded() then
            return row
        end
    end
    return nil
end

function ConfigWindow:_collapseAllDropBoxes()
    for pageIndex = 0, self._ui:getPageCount() - 1 do
        local page = self._ui:getPage(pageIndex)
        for _, row in ipairs(page.dropBoxRows) do
            row:getDropBox():setExpanded(false)
        end
    end
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
---@return Engine.Slider | nil, integer | nil
function ConfigWindow:_getSliderAt(position)
    local page = self:_getActivePage()
    for luaIndex, row in ipairs(page.rows) do
        if Class.isInstance(row, ConfigSliderRowUI) then
            local slider = row:getSlider()
            if slider:getVisible() and slider:getActive() and slider:getAbsoluteTouchHitBounds():contains(position) then
                return slider, luaIndex - 1
            end
        end
    end
    return nil, nil
end

---@param position sf.Vector2f
function ConfigWindow:_shouldCaptureTouch(position)
    local slider, sliderIndex = self:_getSliderAt(position)
    if slider == nil or sliderIndex == nil then
        return super(ConfigWindow, self)._shouldCaptureTouch(position)
    end
    self:_setPointerIndex(sliderIndex)
    return false
end

---@return Source.UI.Parts.ConfigWindow.ConfigRow.ConfigRowControllerBase | nil
function ConfigWindow:_getSelectedSettingRow()
    if self.index == nil then
        return nil
    end
    local page = self:_getActivePage()
    if self.index >= 0 and self.index < #page.rows then
        return page.rows[self.index + 1]
    end
    return nil
end

---@param items table
---@param value string | number
---@return integer
function ConfigWindow.FindSelectedIndex(items, value)
    local textValue = tostring(value)
    local exactIndex = table.index(items, textValue)
    if exactIndex ~= nil then
        return exactIndex - 1
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

---@return integer
function ConfigWindow:_getRectWidth()
    return self.content:getSize().x
end

---@param index integer
---@return sf.Vector2f
function ConfigWindow:_getRectPositionForIndex(index)
    if self._ui == nil then
        return sf.Vector2f.new(0.0, index * self._rectHeight)
    end
    local page = self:_getActivePage()
    page.list:applyPositions()
    local child = page.list:getChildren()[index + 1]
    if child == nil then
        return sf.Vector2f.new(0.0, index * self._rectHeight)
    end
    return sf.Vector2f.new(0.0, child:getPosition().y)
end

---@return Source.UI.ConfigWindow.Page
function ConfigWindow:_getActivePage()
    return self._ui:getPage(self._activePageIndex)
end

---@param pageIndex integer
---@return Source.Windows.ConfigWindow.PageSession
function ConfigWindow:_getPageSession(pageIndex)
    return assert(self._pageSessions[pageIndex + 1])
end

function ConfigWindow:_savePageSession()
    local session = self:_getPageSession(self._activePageIndex)
    session.index = self.index or 0
    session.scrollOffset = assert(self:getScrollBox()):getScrollOffset()
end

function ConfigWindow:_restorePageScroll()
    local session = self:_getPageSession(self._activePageIndex)
    assert(self:getScrollBox()):setScrollOffset(session.scrollOffset)
end

function ConfigWindow:_refreshControlActivity()
    local windowActive = self._open and self:getActive()
    self._tabView:setActive(windowActive)
    for pageIndex = 0, self._ui:getPageCount() - 1 do
        local page = self._ui:getPage(pageIndex)
        local active = windowActive and pageIndex == self._activePageIndex
        page.list:setActive(active)
        self._ui:setPageRowsActive(pageIndex, active)
    end
end

return class(ConfigWindow, WindowSelectable)
