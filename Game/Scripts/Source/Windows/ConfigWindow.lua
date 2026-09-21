local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local GameSystem = require("Source.System")
local View = require("Source.UI.ConfigWindow")
local WindowSelectable = require("Source.Windows.Base.WindowSelectable")
local ConfigSliderRowController = require("Source.Windows.ConfigWindow.ConfigSliderRow.Controller")
local MainConfig = require("Source.Configs.Main")
local EventKeys = require("Source.Configs.EventKeys")
local Locale = require("Source.Locale.Core")
local ConfigCheckBoxRowController = require("Source.Windows.ConfigWindow.ConfigCheckBoxRow.Controller")
local ConfigSettingRowController = require("Source.Windows.ConfigWindow.ConfigSettingRow.Controller")
local Ui = require("Source.UIBase.Ui")

local Display = GlobalCore.Display
local Graphics = GlobalCore.Graphics
local System = GlobalCore.System
local Input = Engine.Input
local Direction = Engine.FocusDirection
local AudioManager = GlobalCore.AudioManager
---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat

local _GRAPHICS_PAGE_INDEX = 0
local _AUDIO_PAGE_INDEX = 1
local _LANGUAGE_PAGE_INDEX = 2
local _TAB_LOCALE_KEYS = { "graphics", "audio", "other" }
local _LANGUAGE_VALUES = MainConfig.SupportedLanguages
local _FRAMERATE_ITEMS = { "30", "60", "90", "120", "0" }
---@type string[]
local _ANTI_ALIASING_LEVEL_ITEMS = { "0", "2", "4", "8" }
local _LIGHTING_RENDER_SCALE_VALUES = { 0.5, 0.75, 1.0 }
local _LIGHTING_RENDER_SCALE_ITEMS = { "50%", "75%", "100%" }
local _GRAPHICS_PRESET_LOCALE_KEYS = { "low", "medium", "high", "extrahigh", "original" }
local _GRAPHICS_PRESETS = {
    { 0.75, 30, 0, 0.5 }, { 1.0, 60, 2, 0.75 }, { 2.0, 60, 8, 1.0 }, { 3.0, 90, 8, 1.0 }, { 0.0, 120, 8, 1.0 }
}

local function getFrameRateLabels()
    local labels = copy(_FRAMERATE_ITEMS)
    labels[#labels] = LOC("unlimited")
    return labels
end

local function getGraphicsPresetLabels(selectedIndex)
    local labels = Locale.ApplyListLocaleFormat(_GRAPHICS_PRESET_LOCALE_KEYS)
    if selectedIndex == #_GRAPHICS_PRESETS then
        labels[#labels + 1] = LOC("custom")
    end
    return labels
end

local function getScaleLabels(scaleValues, zeroLabelKey)
    local labels = {}
    for index, value in ipairs(scaleValues) do
        if value == 0.0 then
            labels[index] = LOC(zeroLabelKey)
        else
            labels[index] = string.format("%.8g", value)
        end
    end
    return labels
end

---@param scaleValues number[]
---@param scale       number
---@return integer
local function findScaleIndex(scaleValues, scale)
    return (table.index(scaleValues, scale) or 1) - 1
end

local function getAntiAliasingLevelItems(level)
    local items = copy(_ANTI_ALIASING_LEVEL_ITEMS)
    local value = tostring(level)
    if table.contains(items, value) then
        return items
    end
    items[#items + 1] = value
    table.sort(items, function (left, right)
        return assert(tonumber(left)) < assert(tonumber(right))
    end)
    return items
end

local function resetRows(listView, rows)
    listView:clearChildren()
    for _, rowUI in ipairs(rows) do
        listView:addChild(rowUI.root)
    end
    listView:applyPositions()
end

local function setRowsActive(rows, active)
    for _, rowUI in ipairs(rows) do
        rowUI:setActive(active)
    end
end

local function refreshRowLabels(rows, localeKeys)
    for index, rowUI in ipairs(rows) do
        rowUI:setLabelText(LOC(assert(localeKeys[index])))
    end
end

local function getGraphicsPresetIndex()
    local maximumRenderScale = Graphics.getMaximumRenderScale()
    local frameRate = Display.getFrameRate()
    local antiAliasingLevel = Display.getAntiAliasingLevel()
    local lightingRenderScale = Graphics.getLightingRenderScale()
    for index, preset in ipairs(_GRAPHICS_PRESETS) do
        if maximumRenderScale == preset[1] and frameRate == preset[2]
            and antiAliasingLevel == preset[3] and lightingRenderScale == preset[4] then
            return index - 1
        end
    end
    return #_GRAPHICS_PRESETS
end

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

---@class Source.Windows.ConfigWindow.Controller
local Controller = {}

Controller.windowOptions = {
    centered = true,
    hidden = true,
    returnButton = true,
    content = "SettingsContent",
    list = "GraphicsList",
    scroll = "SettingsScrollBox",
    itemHeight = 32
}

Controller.refreshEvents = { EventKeys.LocaleChanged }

function Controller:init(onClose)
    self._activePageIndex = _GRAPHICS_PAGE_INDEX
    self._pageSessions = {
        { index = 0, scrollOffset = sf.Vector2f.new(0.0, 0.0) }, { index = 0, scrollOffset = sf.Vector2f.new(0.0, 0.0) },
        { index = 0, scrollOffset = sf.Vector2f.new(0.0, 0.0) }
    }
    self._scaleAvailable = Display.isDisplayScaleConfigurable()
    self._scaleValues = {}
    self._maximumRenderScaleValues = {}
    self._pages = {}
    self._applyingGraphicsPreset = false
    self._onClose = onClose
    self._open = false
    self._tabNavigationHandledThisFrame = false
end

function Controller:ready()
    self:setActivePage(self._activePageIndex)
    self:_refreshControlActivity()
end

function Controller:getLanguageDropBox()
    return self._languageRow.ui.controls["DropBox"]
end

function Controller:getGraphicsPresetDropBox()
    return self._graphicsPresetRow.ui.controls["DropBox"]
end

function Controller:getScaleDropBox()
    if not self._scaleAvailable then
        return nil
    end
    return self._scaleRow.ui.controls["DropBox"]
end

function Controller:getMaximumRenderScaleDropBox()
    return self._maximumRenderScaleRow.ui.controls["DropBox"]
end

function Controller:getFramerateDropBox()
    return self._framerateRow.ui.controls["DropBox"]
end

function Controller:getAntiAliasingLevelDropBox()
    return self._antiAliasingLevelRow.ui.controls["DropBox"]
end

function Controller:getLightingRenderScaleDropBox()
    return self._lightingRenderScaleRow.ui.controls["DropBox"]
end

function Controller:getVerticalSyncCheckBox()
    return self._verticalSyncRow.ui.controls["CheckBox"]
end

function Controller:getMusicOnCheckBox()
    return self._musicOnRow.ui.controls["CheckBox"]
end

function Controller:getMusicVolumeSlider()
    return self._musicVolumeRow.ui.controls["Slider"]
end

function Controller:getSoundOnCheckBox()
    return self._soundOnRow.ui.controls["CheckBox"]
end

function Controller:getSoundVolumeSlider()
    return self._soundVolumeRow.ui.controls["Slider"]
end

function Controller:getVoiceOnCheckBox()
    return self._voiceOnRow.ui.controls["CheckBox"]
end

function Controller:getVoiceVolumeSlider()
    return self._voiceVolumeRow.ui.controls["Slider"]
end

function Controller:isOpen()
    return self._open
end

function Controller:open()
    self:_applyScaleRowChange(self:refreshDisplayScaleOptions())
    for _, session in ipairs(self._pageSessions) do
        session.index = 0
        session.scrollOffset = sf.Vector2f.new(0.0, 0.0)
    end
    self.host:setSelectionInputPaused(false)
    self._activePageIndex = _GRAPHICS_PAGE_INDEX
    self:setActivePage(self._activePageIndex)
    self.ui.controls["TabView"]:setSelectedIndex(self._activePageIndex)
    self.host:setListView(self:_getActivePage().list)
    self._open = true
    self:_restorePageScroll()
    self.host:resetSelection()
    self:_refreshControlActivity()
    self.host:showWithAnimation("FadeIn", function ()
        self.host:setActive(true)
        self:_refreshControlActivity()
        self.host:requestKeyboardFocusAtCursor()
    end)
end

function Controller:close()
    self._open = false
    self:_collapseAllDropBoxes()
    self.host:setSelectionInputPaused(false)
    self.host:setActive(false)
    self:_refreshControlActivity()
    self.host:hideWithAnimation("FadeOut", function ()
        if self._onClose ~= nil then
            self._onClose()
        end
    end)
end

function Controller:dispose()
    self.host:hideImmediate()
    self.host:detachSelectionRect()
    self.host.content = self.ui.controls["Content"]
    self.host:setListView(nil)
    self._onClose = nil
    super(Controller, self).dispose()
end

function Controller:_closeByCancel()
    AudioManager.playSound(GameSystem.GetCancelSE())
    self:close()
end

function Controller:onReturn()
    local expandedRow = self:_getExpandedSettingRow()
    if expandedRow ~= nil then
        expandedRow.ui.controls["DropBox"]:cancel()
        return
    end
    self:_closeByCancel()
end

function Controller:update(deltaTime)
    self._tabNavigationHandledThisFrame = false
    WindowSelectable.update(self.host, deltaTime)
end

function Controller:onTick(deltaTime)
    if self._open then
        self:_applyScaleRowChange(self:syncDisplayScaleAvailability())
    end
    for _, rowUI in ipairs(self:_getActivePage().rows) do
        rowUI:onTick(deltaTime)
    end
    WindowSelectable.onTick(self.host, deltaTime)
end

---@param scaleRowChange integer
function Controller:_applyScaleRowChange(scaleRowChange)
    if scaleRowChange == 0 then
        return
    end
    local session = self:_getPageSession(_GRAPHICS_PAGE_INDEX)
    session.index = adjustGraphicsSettingIndex(session.index, scaleRowChange)
    if self._activePageIndex == _GRAPHICS_PAGE_INDEX then
        self.host:selectIndex(adjustGraphicsSettingIndex(self.host.index or 0, scaleRowChange))
    end
    self:_refreshControlActivity()
end

function Controller:onKeyDown(kwargs)
    if self._tabNavigationHandledThisFrame then
        return
    end
    if Input.isActionTriggered(Input.getCancelKeys(), false) then
        self:onReturn()
        Input.isActionTriggered(Input.getCancelKeys(), true)
        return
    end
    if self.host:isSelectionInputPaused() then
        return
    end
    if self:handleTabNavigation() then
        return
    end
    if self:_handleSelectedSliderKeyDown() then
        return
    end
    WindowSelectable.onKeyDown(self.host, kwargs)
end

function Controller:onDirectionalKey(direction)
    if self.host.index == nil then
        return false
    end
    if direction == Direction.UP then
        if self.host.index == 0 then
            return false
        end
        return self.host:changeSelection(self.host.index - 1)
    end
    if direction == Direction.DOWN then
        if self.host.index + 1 >= #self:_getActivePage().rows then
            return false
        end
        return self.host:changeSelection(self.host.index + 1)
    end
    return false
end

---@return boolean
function Controller:handleTabNavigation()
    if self._tabNavigationHandledThisFrame then
        return true
    end
    if not self._open then
        return false
    end
    local handled = self.ui.controls["TabView"]:handleNavigationInput()
    if handled then
        self._tabNavigationHandledThisFrame = true
    end
    return handled
end

---@param tabIndex integer
function Controller:selectTab(tabIndex)
    self._tabNavigationHandledThisFrame = true
    if tabIndex == self._activePageIndex then
        return
    end
    self:_savePageSession()
    local expandedRow = self:_getExpandedSettingRow()
    if expandedRow ~= nil then
        expandedRow.ui.controls["DropBox"]:setExpanded(false)
    end
    self._activePageIndex = tabIndex
    self:setActivePage(tabIndex)
    self.host:setListView(self:_getActivePage().list)
    local session = self:_getPageSession(tabIndex)
    session.index = math.trunc(math.clamp(session.index, 0, math.max(0, #self:_getActivePage().rows - 1)))
    local scrollBox = assert(self.host:getScrollBox())
    local maximum = scrollBox:getMaxScrollOffset()
    session.scrollOffset = sf.Vector2f.new(
        math.clamp(session.scrollOffset.x, 0.0, maximum.x), math.clamp(session.scrollOffset.y, 0.0, maximum.y)
    )
    self.host:selectIndex(session.index)
    self:_restorePageScroll()
    self:_refreshControlActivity()
    if self._open then
        self.host:requestKeyboardFocus()
    end
end

---@param expanded boolean
function Controller:onDropBoxExpandedChanged(expanded)
    if expanded then
        self.host:setSelectionInputPaused(true)
        local expandedRow = self:_getExpandedSettingRow()
        local page = self:_getActivePage()
        for _, row in ipairs(page.rows) do
            row:setActive(row == expandedRow)
        end
    else
        self.host:setSelectionInputPaused(false)
        self:_refreshControlActivity()
        if self._open then
            self.host:requestKeyboardFocus()
        end
    end
end

---@return Source.Windows.ConfigWindow.ConfigSettingRow.Controller | nil
function Controller:_getExpandedSettingRow()
    local page = self:_getActivePage()
    for _, row in ipairs(page.dropBoxRows) do
        if row.ui.controls["DropBox"]:isExpanded() then
            return row
        end
    end
    return nil
end

function Controller:_collapseAllDropBoxes()
    for pageIndex = 0, self:getPageCount() - 1 do
        local page = self:getPage(pageIndex)
        for _, row in ipairs(page.dropBoxRows) do
            row.ui.controls["DropBox"]:setExpanded(false)
        end
    end
end

---@return boolean
function Controller:_handleSelectedSliderKeyDown()
    local row = self:_getSelectedSettingRow()
    if not Class.isInstance(row, ConfigSliderRowController) then
        return false
    end
    ---@cast row Source.Windows.ConfigWindow.ConfigSliderRow.Controller
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
function Controller:_getSliderAt(position)
    local page = self:_getActivePage()
    for luaIndex, row in ipairs(page.rows) do
        if Class.isInstance(row, ConfigSliderRowController) then
            local slider = row.ui.controls["Slider"]
            if slider:getVisible() and slider:getActive() and slider:getAbsoluteTouchHitBounds():contains(position) then
                return slider, luaIndex - 1
            end
        end
    end
    return nil, nil
end

---@param position sf.Vector2f
function Controller:_shouldCaptureTouch(position)
    local slider, sliderIndex = self:_getSliderAt(position)
    if slider == nil or sliderIndex == nil then
        return self.host:shouldCaptureTouch(position)
    end
    self.host:setPointerIndex(sliderIndex)
    return false
end

---@return Source.Windows.ConfigWindow.ConfigRow | nil
function Controller:_getSelectedSettingRow()
    if self.host.index == nil then
        return nil
    end
    local page = self:_getActivePage()
    if self.host.index >= 0 and self.host.index < #page.rows then
        return page.rows[self.host.index + 1]
    end
    return nil
end

---@param items table
---@param value string | number
---@return integer
local function findSelectedIndex(items, value)
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
function Controller:getItemWidth()
    return self.host.content:getSize().x
end

---@param index integer
---@return sf.Vector2f
function Controller:_getRectPositionForIndex(index)
    if self.ui == nil then
        return sf.Vector2f.new(0.0, index * self.host:getSelectionRowHeight())
    end
    local page = self:_getActivePage()
    page.list:applyPositions()
    local child = page.list:getChildren()[index + 1]
    if child == nil then
        return sf.Vector2f.new(0.0, index * self.host:getSelectionRowHeight())
    end
    return sf.Vector2f.new(0.0, child:getPosition().y)
end

---@return Source.Windows.ConfigWindow.Page
function Controller:_getActivePage()
    return self:getPage(self._activePageIndex)
end

---@param pageIndex integer
---@return Source.Windows.ConfigWindow.PageSession
function Controller:_getPageSession(pageIndex)
    return assert(self._pageSessions[pageIndex + 1])
end

function Controller:_savePageSession()
    local session = self:_getPageSession(self._activePageIndex)
    session.index = self.host.index or 0
    session.scrollOffset = assert(self.host:getScrollBox()):getScrollOffset()
end

function Controller:_restorePageScroll()
    local session = self:_getPageSession(self._activePageIndex)
    assert(self.host:getScrollBox()):setScrollOffset(session.scrollOffset)
end

function Controller:_refreshControlActivity()
    local windowActive = self._open and self.host:getActive()
    self.ui.controls["TabView"]:setActive(windowActive)
    for pageIndex = 0, self:getPageCount() - 1 do
        local page = self:getPage(pageIndex)
        local active = windowActive and pageIndex == self._activePageIndex
        page.list:setActive(active)
        self:setPageRowsActive(pageIndex, active)
    end
end

function Controller:bind()
    self.ui.controls["TabView"]:setCursorSound(tostring(GameSystem.GetCursorSE()))
    self.ui.controls["TabView"]:setOnSelectedIndexChanged(self:bindCallback(Controller.selectTab))
    self.ui.controls["TabView"]:setKeyHint(
        Engine.KeyHint.new({ Keyboard = sf.Keyboard.Key.Q, Joystick = Engine.JoystickButton.getLB() }),
        Engine.KeyHint.new({ Keyboard = sf.Keyboard.Key.E, Joystick = Engine.JoystickButton.getRB() })
    )
    self:_createRows()
    self:setActivePage(_GRAPHICS_PAGE_INDEX)
end

function Controller:refresh()
    for index = 0, self:getPageCount() - 1 do
        local page = self:getPage(index)
        refreshRowLabels(page.rows, page.localeKeys)
    end
    self.ui.controls["TabView"]:setItems(Locale.ApplyListLocaleFormat(_TAB_LOCALE_KEYS))
    self._framerateRow:setItems(getFrameRateLabels())
    self._languageRow:setItems(Locale.ApplyListLocaleFormat(_LANGUAGE_VALUES))
    if self._scaleAvailable then
        self._scaleRow:setItems(getScaleLabels(self._scaleValues, "fullscreen"))
    end
    self._maximumRenderScaleRow:setItems(getScaleLabels(self._maximumRenderScaleValues, "unlimited"))
    self:_syncGraphicsPresetSelection()
end

function Controller:refreshDisplayScaleOptions()
    local scaleRowChange = self:syncDisplayScaleAvailability()
    if self._scaleAvailable then
        local scaleValues, effectiveScale = self:_getCurrentDisplayScaleOptions()
        self:_setScaleOptions(scaleValues, effectiveScale)
    end
    local maximumRenderScaleValues, effectiveMaximumRenderScale = MainConfig.GetMaximumRenderScaleOptions(
        Graphics.getMaximumRenderScale()
    )
    self:_setMaximumRenderScaleOptions(maximumRenderScaleValues, effectiveMaximumRenderScale)
    self:_syncGraphicsPresetSelection()
    return scaleRowChange
end

function Controller:syncDisplayScaleAvailability()
    local configurable = Display.isDisplayScaleConfigurable()
    if configurable == self._scaleAvailable then
        return 0
    end
    local page = self:getPage(_GRAPHICS_PAGE_INDEX)
    for _, rowUI in ipairs(page.dropBoxRows) do
        rowUI.ui.controls["DropBox"]:setExpanded(false)
    end
    self._scaleAvailable = configurable
    self._scaleRow.root:setVisible(configurable)
    if configurable then
        local scaleValues, effectiveScale = self:_getCurrentDisplayScaleOptions()
        self:_setScaleOptions(scaleValues, effectiveScale)
        self._scaleRow:setLabelText(LOC("scale"))
        table.insert(page.rows, 2, self._scaleRow)
        table.insert(page.dropBoxRows, 2, self._scaleRow)
        table.insert(page.localeKeys, 2, "scale")
        resetRows(page.list, page.rows)
        return 1
    end
    self._scaleRow:setActive(false)
    table.remove(page.rows, 2)
    table.remove(page.dropBoxRows, 2)
    table.remove(page.localeKeys, 2)
    page.list:removeChild(self._scaleRow.root)
    page.list:applyPositions()
    self._scaleValues = {}
    return -1
end

function Controller:getPageCount()
    return #self._pages
end

function Controller:getPage(index)
    return assert(self._pages[index + 1])
end

function Controller:setActivePage(index)
    assert(index >= 0 and index < self:getPageCount(), "Config page index is out of range")
    self._activePageIndex = index
    for pageIndex = 0, self:getPageCount() - 1 do
        local page = self:getPage(pageIndex)
        local visible = pageIndex == index
        page.list:setVisible(visible)
        page.list:setActive(visible)
        if not visible then
            setRowsActive(page.rows, false)
        end
    end
end

local function onLanguageSelectedIndexChanged(index)
    local language = _LANGUAGE_VALUES[index + 1]
    ---@cast language string
    System.setLanguage(language)
    Locale.SetLanguage(language)
end

function Controller:onFrameRateSelectedIndexChanged(index)
    if self._applyingGraphicsPreset then
        return
    end
    Display.setFrameRate(math.trunc(assert(tonumber(_FRAMERATE_ITEMS[index + 1]))))
    self:_syncGraphicsPresetSelection()
end

function Controller:onAntiAliasingLevelSelectedIndexChanged(index)
    if self._applyingGraphicsPreset then
        return
    end
    local value = assert(self._antiAliasingLevelItems[index + 1])
    Display.setAntiAliasingLevel(math.trunc(assert(tonumber(value))))
    self:_syncGraphicsPresetSelection()
end

function Controller:onLightingRenderScaleSelectedIndexChanged(index)
    if self._applyingGraphicsPreset then
        return
    end
    Graphics.setLightingRenderScale(assert(_LIGHTING_RENDER_SCALE_VALUES[index + 1]))
    self:_syncGraphicsPresetSelection()
end

function Controller:_createRows()
    self:_createGraphicsRows()
    self:_createAudioRows()
    self:_createLanguageRows()
    for pageIndex = 0, self:getPageCount() - 1 do
        local page = self:getPage(pageIndex)
        for _, rowUI in ipairs(page.allRows) do
            rowUI:prepare()
            if Class.isInstance(rowUI, ConfigSettingRowController) then
                ---@cast rowUI Source.Windows.ConfigWindow.ConfigSettingRow.Controller
                self:_bindDropBoxRow(rowUI)
            end
        end
    end
    if not self._scaleAvailable then
        self._scaleRow:setActive(false)
        self._scaleRow.root:setVisible(false)
        self.ui.controls["GraphicsList"]:removeChild(self._scaleRow.root)
    end
    self:_bindGraphicsRows()
    self:_bindLanguageRows()
end

function Controller:_bindDropBoxRow(rowUI)
    local dropBox = rowUI.ui.controls["DropBox"]
    dropBox:setOnExpandedChanged(self:bindCallback(Controller.onDropBoxExpandedChanged))
    dropBox:addKeyDownCallback(self:bindCallback(Controller.handleTabNavigation))
end

function Controller:_createScaleRow()
    local effectiveScale
    if self._scaleAvailable then
        self._scaleValues, effectiveScale = self:_getCurrentDisplayScaleOptions()
    else
        self._scaleValues, effectiveScale = MainConfig.GetDisplayScaleOptions(nil, Display.getConfiguredScale())
    end
    self._scaleRow = ConfigSettingRowController.new(
        self.ui.assets["GraphicsItem2"], LOC("scale"), getScaleLabels(self._scaleValues, "fullscreen"),
        findScaleIndex(self._scaleValues, effectiveScale)
    )
end

function Controller:_createGraphicsRows()
    local graphicsPresetIndex = getGraphicsPresetIndex()
    self._graphicsPresetRow = ConfigSettingRowController.new(
        self.ui.assets["GraphicsItem1"], LOC("graphicspreset"), getGraphicsPresetLabels(graphicsPresetIndex),
        graphicsPresetIndex
    )
    self:_createScaleRow()
    local effectiveMaximumRenderScale
    self._maximumRenderScaleValues, effectiveMaximumRenderScale = MainConfig.GetMaximumRenderScaleOptions(
        Graphics.getMaximumRenderScale()
    )
    self._maximumRenderScaleRow = ConfigSettingRowController.new(
        self.ui.assets["GraphicsItem3"], LOC("maxrenderscale"),
        getScaleLabels(self._maximumRenderScaleValues, "unlimited"),
        findScaleIndex(self._maximumRenderScaleValues, effectiveMaximumRenderScale)
    )
    self._framerateRow = ConfigSettingRowController.new(
        self.ui.assets["GraphicsItem4"], LOC("framerate"), getFrameRateLabels(),
        findSelectedIndex(_FRAMERATE_ITEMS, Display.getFrameRate())
    )
    self._antiAliasingLevelItems = getAntiAliasingLevelItems(Display.getAntiAliasingLevel())
    self._antiAliasingLevelRow = ConfigSettingRowController.new(
        self.ui.assets["GraphicsItem5"], LOC("antialiasinglevel"), self._antiAliasingLevelItems,
        findSelectedIndex(self._antiAliasingLevelItems, Display.getAntiAliasingLevel())
    )
    self._verticalSyncRow = ConfigCheckBoxRowController.new(
        self.ui.assets["GraphicsItem6"], LOC("verticalsync"), Display.getVerticalSync(), Display.setVerticalSync
    )
    self._lightingRenderScaleRow = ConfigSettingRowController.new(
        self.ui.assets["GraphicsItem7"], LOC("lightingrenderscale"), _LIGHTING_RENDER_SCALE_ITEMS,
        findScaleIndex(_LIGHTING_RENDER_SCALE_VALUES, Graphics.getLightingRenderScale())
    )
    local allRows = {
        self._graphicsPresetRow, self._scaleRow, self._maximumRenderScaleRow, self._framerateRow,
        self._antiAliasingLevelRow, self._verticalSyncRow, self._lightingRenderScaleRow
    }
    local rows = copy(allRows)
    local dropBoxRows = {
        self._graphicsPresetRow, self._scaleRow, self._maximumRenderScaleRow, self._framerateRow,
        self._antiAliasingLevelRow, self._lightingRenderScaleRow
    }
    local localeKeys = {
        "graphicspreset", "scale", "maxrenderscale", "framerate", "antialiasinglevel", "verticalsync",
        "lightingrenderscale"
    }
    if not self._scaleAvailable then
        table.remove(rows, 2)
        table.remove(dropBoxRows, 2)
        table.remove(localeKeys, 2)
    end
    self._pages[_GRAPHICS_PAGE_INDEX + 1] = {
        list = self.ui.controls["GraphicsList"],
        allRows = allRows,
        rows = rows,
        dropBoxRows = dropBoxRows,
        localeKeys = localeKeys
    }
end

function Controller:_createAudioRows()
    self._musicOnRow = ConfigCheckBoxRowController.new(
        self.ui.assets["AudioItem1"], LOC("musicon"), AudioManager.getMusicOn(), AudioManager.setMusicOn
    )
    self._musicVolumeRow = ConfigSliderRowController.new(
        self.ui.assets["AudioItem2"], LOC("musicvolume"), math.round(AudioManager.getMusicVolume()),
        AudioManager.setMusicVolume
    )
    self._soundOnRow = ConfigCheckBoxRowController.new(
        self.ui.assets["AudioItem3"], LOC("soundon"), AudioManager.getSoundOn(), AudioManager.setSoundOn
    )
    self._soundVolumeRow = ConfigSliderRowController.new(
        self.ui.assets["AudioItem4"], LOC("soundvolume"), math.round(AudioManager.getSoundVolume()),
        AudioManager.setSoundVolume
    )
    self._voiceOnRow = ConfigCheckBoxRowController.new(
        self.ui.assets["AudioItem5"], LOC("voiceon"), AudioManager.getVoiceOn(), AudioManager.setVoiceOn
    )
    self._voiceVolumeRow = ConfigSliderRowController.new(
        self.ui.assets["AudioItem6"], LOC("voicevolume"), math.round(AudioManager.getVoiceVolume()),
        AudioManager.setVoiceVolume
    )
    local rows = {
        self._musicOnRow, self._musicVolumeRow, self._soundOnRow, self._soundVolumeRow, self._voiceOnRow,
        self._voiceVolumeRow
    }
    self._pages[_AUDIO_PAGE_INDEX + 1] = {
        list = self.ui.controls["AudioList"],
        allRows = rows,
        rows = rows,
        dropBoxRows = {},
        localeKeys = { "musicon", "musicvolume", "soundon", "soundvolume", "voiceon", "voicevolume" }
    }
end

function Controller:_createLanguageRows()
    self._languageRow = ConfigSettingRowController.new(
        self.ui.assets["LanguageItem1"], LOC("language"), Locale.ApplyListLocaleFormat(_LANGUAGE_VALUES),
        findSelectedIndex(_LANGUAGE_VALUES, System.getLanguage())
    )
    local rows = { self._languageRow }
    self._pages[_LANGUAGE_PAGE_INDEX + 1] = {
        list = self.ui.controls["LanguageList"],
        allRows = rows,
        rows = rows,
        dropBoxRows = rows,
        localeKeys = { "language" }
    }
end

function Controller:_bindGraphicsRows()
    self._graphicsPresetRow.ui.controls["DropBox"]:setOnSelectionConfirmed(
        self:bindCallback(Controller._onGraphicsPresetSelectionConfirmed)
    )
    self:_bindScaleRow()
    self._maximumRenderScaleRow.ui.controls["DropBox"]:setOnSelectionConfirmed(
        self:bindCallback(Controller._onMaximumRenderScaleSelectionConfirmed)
    )
    self._framerateRow.ui.controls["DropBox"]:setOnSelectedIndexChanged(
        self:bindCallback(Controller.onFrameRateSelectedIndexChanged)
    )
    self._antiAliasingLevelRow.ui.controls["DropBox"]:setOnSelectedIndexChanged(
        self:bindCallback(Controller.onAntiAliasingLevelSelectedIndexChanged)
    )
    self._lightingRenderScaleRow.ui.controls["DropBox"]:setOnSelectedIndexChanged(
        self:bindCallback(Controller.onLightingRenderScaleSelectedIndexChanged)
    )
end

function Controller:_bindScaleRow()
    self._scaleRow.ui.controls["DropBox"]:setOnSelectionConfirmed(
        self:bindCallback(Controller._onScaleSelectionConfirmed)
    )
end

function Controller:_bindLanguageRows()
    self._languageRow.ui.controls["DropBox"]:setOnSelectedIndexChanged(onLanguageSelectedIndexChanged)
end

function Controller:_syncGraphicsPresetSelection()
    if self._applyingGraphicsPreset then
        return
    end
    local index = getGraphicsPresetIndex()
    self._graphicsPresetRow:setItems(getGraphicsPresetLabels(index))
    self._graphicsPresetRow.ui.controls["DropBox"]:setSelectedIndex(index)
end

function Controller:_onGraphicsPresetSelectionConfirmed(index)
    if index < 0 or index >= #_GRAPHICS_PRESETS then
        self:_syncGraphicsPresetSelection()
        return
    end
    local preset = _GRAPHICS_PRESETS[index + 1]
    self._applyingGraphicsPreset = true
    Graphics.setMaximumRenderScale(preset[1])
    Display.setFrameRate(preset[2])
    Display.setAntiAliasingLevel(preset[3])
    Graphics.setLightingRenderScale(preset[4])
    local maximumRenderScaleValues, effectiveMaximumRenderScale = MainConfig.GetMaximumRenderScaleOptions(preset[1])
    self:_setMaximumRenderScaleOptions(maximumRenderScaleValues, effectiveMaximumRenderScale)
    self._framerateRow.ui.controls["DropBox"]:setSelectedIndex(findSelectedIndex(_FRAMERATE_ITEMS, preset[2]))
    self
        ._antiAliasingLevelRow
        .ui.controls["DropBox"]
        :setSelectedIndex(findSelectedIndex(self._antiAliasingLevelItems, preset[3]))
    self._lightingRenderScaleRow.ui.controls["DropBox"]:setSelectedIndex(
        findScaleIndex(_LIGHTING_RENDER_SCALE_VALUES, preset[4])
    )
    self._applyingGraphicsPreset = false
    self:_syncGraphicsPresetSelection()
end

---@diagnostic disable-next-line: unused
function Controller:_getCurrentDisplayScaleOptions()
    local configuredScale = Display.getConfiguredScale()
    local maximumScale = Display.getMaximumWindowedScale(Display.getGameSize())
    local scaleValues, effectiveScale = MainConfig.GetDisplayScaleOptions(maximumScale, configuredScale)
    if maximumScale ~= nil and effectiveScale ~= configuredScale then
        Display.setScale(effectiveScale)
    end
    return scaleValues, effectiveScale
end

function Controller:_setScaleOptions(scaleValues, selectedScale)
    self._scaleValues = scaleValues
    self._scaleRow:setItems(getScaleLabels(scaleValues, "fullscreen"))
    self._scaleRow.ui.controls["DropBox"]:setSelectedIndex(findScaleIndex(scaleValues, selectedScale))
end

function Controller:_setMaximumRenderScaleOptions(scaleValues, selectedScale)
    self._maximumRenderScaleValues = scaleValues
    self._maximumRenderScaleRow:setItems(getScaleLabels(scaleValues, "unlimited"))
    self._maximumRenderScaleRow.ui.controls["DropBox"]:setSelectedIndex(findScaleIndex(scaleValues, selectedScale))
end

function Controller:_onScaleSelectionConfirmed(index)
    if not Display.isDisplayScaleConfigurable() then
        local scaleValues, effectiveScale = self:_getCurrentDisplayScaleOptions()
        self:_setScaleOptions(scaleValues, effectiveScale)
        return
    end
    local selectedScale = assert(self._scaleValues[index + 1])
    local maximumScale = Display.getMaximumWindowedScale(Display.getGameSize())
    local scaleValues, effectiveScale = MainConfig.GetDisplayScaleOptions(maximumScale, selectedScale)
    self:_setScaleOptions(scaleValues, effectiveScale)
    Display.setScale(effectiveScale)
end

function Controller:_onMaximumRenderScaleSelectionConfirmed(index)
    local selectedScale = assert(self._maximumRenderScaleValues[index + 1])
    local scaleValues, effectiveScale = MainConfig.GetMaximumRenderScaleOptions(selectedScale)
    self:_setMaximumRenderScaleOptions(scaleValues, effectiveScale)
    Graphics.setMaximumRenderScale(effectiveScale)
    self:_syncGraphicsPresetSelection()
end

function Controller:setPageRowsActive(index, active)
    setRowsActive(self:getPage(index).rows, active)
end

return Ui.DefineWindow(View, Controller, WindowSelectable)
