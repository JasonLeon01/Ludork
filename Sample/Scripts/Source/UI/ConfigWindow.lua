local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local MainConfig = require("Source.Configs.Main")
local EventKeys = require("Source.Configs.EventKeys")
local Locale = require("Source.Locale.Core")
local CommandRowUI = require("Source.UI.Helpers.CommandRow")
local ConfigCheckBoxRowUI = require("Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow")
local ConfigSettingRowUI = require("Source.UI.Parts.ConfigWindow.ConfigSettingRow")
local ConfigSliderRowUI = require("Source.UI.Parts.ConfigWindow.ConfigSliderRow")
local Ui = require("Source.UI.Ui")

local System = GlobalCore.System
---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat

local _WINDOW_WIDTH = 480
local _WINDOW_HEIGHT = 384
local _CONTENT_WIDTH = 448
local _TAB_COUNT = 3
local _TAB_ROW_WIDTH = math.floor((_CONTENT_WIDTH - 32) / _TAB_COUNT)
local _DROPBOX_WIDTH = 200
local _CHECKBOX_SIZE = 32
local _SLIDER_WIDTH = 160
local _GRAPHICS_PAGE_INDEX = 0
local _AUDIO_PAGE_INDEX = 1
local _LANGUAGE_PAGE_INDEX = 2
local _TAB_LOCALE_KEYS = { "graphics", "audio", "language" }
local _LANGUAGE_VALUES = MainConfig.SupportedLanguages
local _FRAMERATE_ITEMS = { "30", "60", "90", "120" }
local _ANTI_ALIASING_LEVEL_ITEMS = { "0", "2", "4", "8" }
local _LIGHTING_RENDER_SCALE_VALUES = { 0.5, 0.75, 1.0 }
local _LIGHTING_RENDER_SCALE_ITEMS = { "50%", "75%", "100%" }
local _GRAPHICS_PRESET_LOCALE_KEYS = { "low", "medium", "high", "extrahigh", "original", "custom" }
local _GRAPHICS_PRESETS = {
    { 0.75, 30, 0, 0.5 }, { 1.0, 60, 2, 0.75 }, { 2.0, 60, 8, 1.0 }, { 3.0, 90, 8, 1.0 }, { 0.0, 120, 8, 1.0 }
}

local function getLanguageLabels()
    local languageLabels = {}
    for index, value in ipairs(_LANGUAGE_VALUES) do
        languageLabels[index] = LOC(value)
    end
    return languageLabels
end

local function getGraphicsPresetLabels()
    local labels = {}
    for index, key in ipairs(_GRAPHICS_PRESET_LOCALE_KEYS) do
        labels[index] = LOC(key)
    end
    return labels
end

local function getScaleLabels(scaleValues)
    local labels = {}
    for index, value in ipairs(scaleValues) do
        if value == 0.0 then
            labels[index] = LOC("fullscreen")
        else
            labels[index] = string.format("%.8g", value)
        end
    end
    return labels
end

local function getMaximumRenderScaleLabels(values)
    local labels = {}
    for index, value in ipairs(values) do
        if value == 0.0 then
            labels[index] = LOC("unlimited")
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
    for index, value in ipairs(scaleValues) do
        if value == scale then
            return index - 1
        end
    end
    return 0
end

local function getAntiAliasingLevelItems(level)
    local items = copy(_ANTI_ALIASING_LEVEL_ITEMS)
    local value = tostring(level)
    for _, item in ipairs(items) do
        if item == value then
            return items
        end
    end
    items[#items + 1] = value
    table.sort(items, function (left, right)
        return assert(tonumber(left)) < assert(tonumber(right))
    end)
    return items
end

local function addRows(listView, rows)
    for _, rowUI in ipairs(rows) do
        listView:addChild(rowUI:prepare())
    end
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

local function disposeRows(rows)
    for _, rowUI in ipairs(rows) do
        rowUI:dispose()
    end
end

local function getGraphicsPresetIndex()
    local maximumRenderScale = System.getMaximumRenderScale()
    local frameRate = System.getFrameRate()
    local antiAliasingLevel = System.getAntiAliasingLevel()
    local lightingRenderScale = System.getLightingRenderScale()
    for index, preset in ipairs(_GRAPHICS_PRESETS) do
        if maximumRenderScale == preset[1] and frameRate == preset[2]
            and antiAliasingLevel == preset[3] and lightingRenderScale == preset[4] then
            return index - 1
        end
    end
    return #_GRAPHICS_PRESETS
end

---@class Source.UI.ConfigWindow
local ConfigWindowUI = {}

ConfigWindowUI.refreshEvents = { EventKeys.LocaleChanged }

function ConfigWindowUI:init(model, windowSkin)
    self._windowSkin = windowSkin
    self._scaleValues = {}
    self._maximumRenderScaleValues = {}
    self._pages = {}
    self._tabControllers = {}
    self._activePageIndex = _GRAPHICS_PAGE_INDEX
    self._applyingGraphicsPreset = false
    super(ConfigWindowUI, self).init(model, nil)
end

function ConfigWindowUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._settingsWindowFrame = self:requireControl("SettingsWindowFrame")
    self._tabList = self:requireControl("TabList")
    self._settingsContent = self:requireControl("SettingsContent")
    self._graphicsList = self:requireControl("GraphicsList")
    self._audioList = self:requireControl("AudioList")
    self._languageList = self:requireControl("LanguageList")
    ---@cast self._settingsWindowFrame Engine.Window
    self._settingsWindowFrame:setWindowSkin(self._windowSkin, false)
    self:_createTabs()
    self:_createRows()
    self:setActivePage(_GRAPHICS_PAGE_INDEX)
end

function ConfigWindowUI:refresh()
    for index = 0, self:getPageCount() - 1 do
        local page = self:getPage(index)
        refreshRowLabels(page.rows, page.localeKeys)
    end
    for _, controller in ipairs(self._tabControllers) do
        controller:refresh()
    end
    self._graphicsPresetRow:setItems(getGraphicsPresetLabels())
    self._languageRow:setItems(getLanguageLabels())
    if self._scaleRow ~= nil then
        self._scaleRow:setItems(getScaleLabels(self._scaleValues))
    end
    self._maximumRenderScaleRow:setItems(getMaximumRenderScaleLabels(self._maximumRenderScaleValues))
    self:_syncGraphicsPresetSelection()
end

function ConfigWindowUI:refreshDisplayScaleOptions()
    local scaleRowChange = self:syncDisplayScaleAvailability()
    if self._scaleRow ~= nil then
        local scaleValues, effectiveScale = self:_getCurrentDisplayScaleOptions()
        self:_setScaleOptions(scaleValues, effectiveScale)
    end
    local maximumRenderScaleValues, effectiveMaximumRenderScale = MainConfig.GetMaximumRenderScaleOptions(
        System.getMaximumRenderScale()
    )
    self:_setMaximumRenderScaleOptions(maximumRenderScaleValues, effectiveMaximumRenderScale)
    self:_syncGraphicsPresetSelection()
    return scaleRowChange
end

function ConfigWindowUI:syncDisplayScaleAvailability()
    local configurable = System.isDisplayScaleConfigurable()
    if configurable == (self._scaleRow ~= nil) then
        return 0
    end
    local page = self:getPage(_GRAPHICS_PAGE_INDEX)
    for _, rowUI in ipairs(page.dropBoxRows) do
        rowUI:getDropBox():setExpanded(false)
    end
    if configurable then
        self:_createScaleRow()
        ---@cast self._scaleRow - nil
        self._scaleRow:prepare()
        self:_bindDropBoxRow(self._scaleRow)
        self:_bindScaleRow()
        table.insert(page.rows, 2, self._scaleRow)
        table.insert(page.dropBoxRows, 2, self._scaleRow)
        table.insert(page.localeKeys, 2, "scale")
        resetRows(page.list, page.rows)
        return 1
    end
    table.remove(page.rows, 2)
    table.remove(page.dropBoxRows, 2)
    table.remove(page.localeKeys, 2)
    resetRows(page.list, page.rows)
    ---@cast self._scaleRow - nil
    self._scaleRow:dispose()
    self._scaleRow = nil
    self._scaleValues = {}
    return -1
end

function ConfigWindowUI:prepare()
    local settingsView
    if self._settingsContent ~= nil then
        settingsView = self._settingsContent:getView()
    end
    local root = super(ConfigWindowUI, self).prepare(sf.Vector2u.new(_WINDOW_WIDTH, _WINDOW_HEIGHT))
    if settingsView ~= nil and self._settingsContent ~= nil then
        self._settingsContent:setView(settingsView)
    end
    return root
end

function ConfigWindowUI:attach()
    self:attachWindowView(self.model)
end

function ConfigWindowUI:getWindowFrame()
    return self._windowFrame
end

function ConfigWindowUI:getContent()
    return self._content
end

function ConfigWindowUI:getTabList()
    return self._tabList
end

function ConfigWindowUI:getSettingsContent()
    return self._settingsContent
end

function ConfigWindowUI:getPageCount()
    return #self._pages
end

function ConfigWindowUI:getPage(index)
    return assert(self._pages[index + 1])
end

function ConfigWindowUI:getActivePageIndex()
    return self._activePageIndex
end

function ConfigWindowUI:setActivePage(index)
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

function ConfigWindowUI:getLanguageRow()
    return self._languageRow
end

function ConfigWindowUI:getGraphicsPresetRow()
    return self._graphicsPresetRow
end

function ConfigWindowUI:getScaleRow()
    return self._scaleRow
end

function ConfigWindowUI:getMaximumRenderScaleRow()
    return self._maximumRenderScaleRow
end

function ConfigWindowUI:getFramerateRow()
    return self._framerateRow
end

function ConfigWindowUI:getAntiAliasingLevelRow()
    return self._antiAliasingLevelRow
end

function ConfigWindowUI:getLightingRenderScaleRow()
    return self._lightingRenderScaleRow
end

function ConfigWindowUI:getVerticalSyncRow()
    return self._verticalSyncRow
end

function ConfigWindowUI:getMusicOnRow()
    return self._musicOnRow
end

function ConfigWindowUI:getMusicVolumeRow()
    return self._musicVolumeRow
end

function ConfigWindowUI:getSoundOnRow()
    return self._soundOnRow
end

function ConfigWindowUI:getSoundVolumeRow()
    return self._soundVolumeRow
end

function ConfigWindowUI:getVoiceOnRow()
    return self._voiceOnRow
end

function ConfigWindowUI:getVoiceVolumeRow()
    return self._voiceVolumeRow
end

local function onVerticalSyncCheckedChanged(checked)
    System.setVerticalSync(checked)
end

local function onLanguageSelectedIndexChanged(index)
    local language = _LANGUAGE_VALUES[index + 1]
    ---@cast language string
    System.setLanguage(language)
    Locale.SetLanguage(language)
end

function ConfigWindowUI:onFrameRateSelectedIndexChanged(index)
    if self._applyingGraphicsPreset then
        return
    end
    System.setFrameRate(Engine.ToInteger(_FRAMERATE_ITEMS[index + 1]))
    self:_syncGraphicsPresetSelection()
end

function ConfigWindowUI:onAntiAliasingLevelSelectedIndexChanged(index)
    if self._applyingGraphicsPreset then
        return
    end
    local value = assert(self._antiAliasingLevelItems[index + 1])
    System.setAntiAliasingLevel(Engine.ToInteger(value))
    self:_syncGraphicsPresetSelection()
end

function ConfigWindowUI:onLightingRenderScaleSelectedIndexChanged(index)
    if self._applyingGraphicsPreset then
        return
    end
    System.setLightingRenderScale(assert(_LIGHTING_RENDER_SCALE_VALUES[index + 1]))
    self:_syncGraphicsPresetSelection()
end

local function onMusicOnCheckedChanged(checked)
    System.setMusicOn(checked)
end

local function onMusicVolumeChanged(value)
    System.setMusicVolume(value)
end

local function onSoundOnCheckedChanged(checked)
    System.setSoundOn(checked)
end

local function onSoundVolumeChanged(value)
    System.setSoundVolume(value)
end

local function onVoiceOnCheckedChanged(checked)
    System.setVoiceOn(checked)
end

local function onVoiceVolumeChanged(value)
    System.setVoiceVolume(value)
end

function ConfigWindowUI:tick(deltaTime)
    local page = self:getPage(self._activePageIndex)
    for _, rowUI in ipairs(page.rows) do
        rowUI:onTick(deltaTime)
    end
end

function ConfigWindowUI:dispose()
    for index = 0, self:getPageCount() - 1 do
        disposeRows(self:getPage(index).rows)
    end
    for _, controller in ipairs(self._tabControllers) do
        ---@diagnostic disable-next-line: param-type-mismatch
        controller.root:addConfirmCallback(nil)
    end
    self._pages = {}
    self._tabControllers = {}
    super(ConfigWindowUI, self).dispose()
end

function ConfigWindowUI:_createTabs()
    for index, localeKey in ipairs(_TAB_LOCALE_KEYS) do
        local tabIndex = index - 1
        local controller = CommandRowUI.new({
            localeKey = localeKey,
            callback = function ()
                self.model:_onTabConfirmed(tabIndex)
            end
        })
        local logicalSize = sf.Vector2u.new(_TAB_ROW_WIDTH, 32)
        ---@cast logicalSize sf.Vector2u
        self._tabControllers[index] = controller
        self._tabList:addChild(controller:prepare(logicalSize))
    end
    self._tabList:applyPositions()
end

function ConfigWindowUI:_createRows()
    self:_createGraphicsRows()
    self:_createAudioRows()
    self:_createLanguageRows()
    for pageIndex = 0, self:getPageCount() - 1 do
        local page = self:getPage(pageIndex)
        addRows(page.list, page.rows)
        for _, rowUI in ipairs(page.dropBoxRows) do
            self:_bindDropBoxRow(rowUI)
        end
    end
    self:_bindGraphicsRows()
    self:_bindLanguageRows()
end

function ConfigWindowUI:_bindDropBoxRow(rowUI)
    rowUI:addConfirmCallback(self.model.MakeSettingRowConfirmCallback(rowUI))
    rowUI:getDropBox():setOnExpandedChanged(function (expanded)
        self.model:_onDropBoxExpandedChanged(expanded)
    end)
end

function ConfigWindowUI:_createScaleRow()
    local effectiveScale
    self._scaleValues, effectiveScale = self:_getCurrentDisplayScaleOptions()
    self._scaleRow = ConfigSettingRowUI.new(
        LOC("scale"), getScaleLabels(self._scaleValues), _CONTENT_WIDTH, _DROPBOX_WIDTH, self._windowSkin,
        findScaleIndex(self._scaleValues, effectiveScale)
    )
end

function ConfigWindowUI:_createGraphicsRows()
    self._graphicsPresetRow = ConfigSettingRowUI.new(
        LOC("graphicspreset"), getGraphicsPresetLabels(), _CONTENT_WIDTH, _DROPBOX_WIDTH, self._windowSkin,
        getGraphicsPresetIndex()
    )
    if System.isDisplayScaleConfigurable() then
        self:_createScaleRow()
    end
    local effectiveMaximumRenderScale
    self._maximumRenderScaleValues, effectiveMaximumRenderScale = MainConfig.GetMaximumRenderScaleOptions(
        System.getMaximumRenderScale()
    )
    self._maximumRenderScaleRow = ConfigSettingRowUI.new(
        LOC("maxrenderscale"), getMaximumRenderScaleLabels(self._maximumRenderScaleValues), _CONTENT_WIDTH,
        _DROPBOX_WIDTH, self._windowSkin, findScaleIndex(self._maximumRenderScaleValues, effectiveMaximumRenderScale)
    )
    self._framerateRow = ConfigSettingRowUI.new(
        LOC("framerate"), _FRAMERATE_ITEMS, _CONTENT_WIDTH, _DROPBOX_WIDTH, self._windowSkin,
        self.model.FindSelectedIndex(_FRAMERATE_ITEMS, System.getFrameRate())
    )
    self._antiAliasingLevelItems = getAntiAliasingLevelItems(System.getAntiAliasingLevel())
    self._antiAliasingLevelRow = ConfigSettingRowUI.new(
        LOC("antialiasinglevel"), self._antiAliasingLevelItems, _CONTENT_WIDTH, _DROPBOX_WIDTH, self._windowSkin,
        self.model.FindSelectedIndex(self._antiAliasingLevelItems, System.getAntiAliasingLevel())
    )
    self._lightingRenderScaleRow = ConfigSettingRowUI.new(
        LOC("lightingrenderscale"), _LIGHTING_RENDER_SCALE_ITEMS, _CONTENT_WIDTH, _DROPBOX_WIDTH, self._windowSkin,
        findScaleIndex(_LIGHTING_RENDER_SCALE_VALUES, System.getLightingRenderScale())
    )
    self._verticalSyncRow = ConfigCheckBoxRowUI.new(
        LOC("verticalsync"),
        _CONTENT_WIDTH,
        _CHECKBOX_SIZE,
        self._windowSkin,
        System.getVerticalSync(),
        function (checked)
            onVerticalSyncCheckedChanged(checked)
        end
    )
    local rows = {
        self._graphicsPresetRow, self._maximumRenderScaleRow, self._framerateRow, self._antiAliasingLevelRow,
        self._verticalSyncRow, self._lightingRenderScaleRow
    }
    local dropBoxRows = {
        self._graphicsPresetRow, self._maximumRenderScaleRow, self._framerateRow, self._antiAliasingLevelRow,
        self._lightingRenderScaleRow
    }
    local localeKeys = {
        "graphicspreset", "maxrenderscale", "framerate", "antialiasinglevel", "verticalsync", "lightingrenderscale"
    }
    if self._scaleRow ~= nil then
        table.insert(rows, 2, self._scaleRow)
        table.insert(dropBoxRows, 2, self._scaleRow)
        table.insert(localeKeys, 2, "scale")
    end
    self._pages[_GRAPHICS_PAGE_INDEX + 1] = {
        list = self._graphicsList,
        rows = rows,
        dropBoxRows = dropBoxRows,
        localeKeys = localeKeys
    }
end

function ConfigWindowUI:_createAudioRows()
    self._musicOnRow = ConfigCheckBoxRowUI.new(LOC("musicon"), _CONTENT_WIDTH, _CHECKBOX_SIZE, self._windowSkin, System.getMusicOn(), function (
        checked
    )
        onMusicOnCheckedChanged(checked)
    end)
    self._musicVolumeRow = ConfigSliderRowUI.new(LOC("musicvolume"), _CONTENT_WIDTH, _SLIDER_WIDTH, Engine.Round(
        System.getMusicVolume()
    ), function (value)
        onMusicVolumeChanged(value)
    end)
    self._soundOnRow = ConfigCheckBoxRowUI.new(LOC("soundon"), _CONTENT_WIDTH, _CHECKBOX_SIZE, self._windowSkin, System.getSoundOn(), function (
        checked
    )
        onSoundOnCheckedChanged(checked)
    end)
    self._soundVolumeRow = ConfigSliderRowUI.new(LOC("soundvolume"), _CONTENT_WIDTH, _SLIDER_WIDTH, Engine.Round(
        System.getSoundVolume()
    ), function (value)
        onSoundVolumeChanged(value)
    end)
    self._voiceOnRow = ConfigCheckBoxRowUI.new(LOC("voiceon"), _CONTENT_WIDTH, _CHECKBOX_SIZE, self._windowSkin, System.getVoiceOn(), function (
        checked
    )
        onVoiceOnCheckedChanged(checked)
    end)
    self._voiceVolumeRow = ConfigSliderRowUI.new(LOC("voicevolume"), _CONTENT_WIDTH, _SLIDER_WIDTH, Engine.Round(
        System.getVoiceVolume()
    ), function (value)
        onVoiceVolumeChanged(value)
    end)
    self._pages[_AUDIO_PAGE_INDEX + 1] = {
        list = self._audioList,
        rows = {
            self._musicOnRow,
            self._musicVolumeRow,
            self._soundOnRow,
            self._soundVolumeRow,
            self._voiceOnRow,
            self._voiceVolumeRow
        },
        dropBoxRows = {},
        localeKeys = { "musicon", "musicvolume", "soundon", "soundvolume", "voiceon", "voicevolume" }
    }
end

function ConfigWindowUI:_createLanguageRows()
    self._languageRow = ConfigSettingRowUI.new(
        LOC("language"), getLanguageLabels(), _CONTENT_WIDTH, _DROPBOX_WIDTH, self._windowSkin,
        self.model.FindSelectedIndex(_LANGUAGE_VALUES, System.getLanguage())
    )
    self._pages[_LANGUAGE_PAGE_INDEX + 1] = {
        list = self._languageList,
        rows = { self._languageRow },
        dropBoxRows = { self._languageRow },
        localeKeys = { "language" }
    }
end

function ConfigWindowUI:_bindGraphicsRows()
    self._graphicsPresetRow:getDropBox():setOnSelectionConfirmed(function (index)
        self:_onGraphicsPresetSelectionConfirmed(index)
    end)
    self:_bindScaleRow()
    self._maximumRenderScaleRow:getDropBox():setOnSelectionConfirmed(function (index)
        self:_onMaximumRenderScaleSelectionConfirmed(index)
    end)
    self._framerateRow:getDropBox():setOnSelectedIndexChanged(function (index)
        self:onFrameRateSelectedIndexChanged(index)
    end)
    self._antiAliasingLevelRow:getDropBox():setOnSelectedIndexChanged(function (index)
        self:onAntiAliasingLevelSelectedIndexChanged(index)
    end)
    self._lightingRenderScaleRow:getDropBox():setOnSelectedIndexChanged(function (index)
        self:onLightingRenderScaleSelectedIndexChanged(index)
    end)
end

function ConfigWindowUI:_bindScaleRow()
    if self._scaleRow == nil then
        return
    end
    self._scaleRow:getDropBox():setOnSelectionConfirmed(function (index)
        self:_onScaleSelectionConfirmed(index)
    end)
end

function ConfigWindowUI:_bindLanguageRows()
    self._languageRow:getDropBox():setOnSelectedIndexChanged(function (index)
        onLanguageSelectedIndexChanged(index)
    end)
end

function ConfigWindowUI:_syncGraphicsPresetSelection()
    if self._applyingGraphicsPreset then
        return
    end
    self._graphicsPresetRow:getDropBox():setSelectedIndex(getGraphicsPresetIndex())
end

function ConfigWindowUI:_onGraphicsPresetSelectionConfirmed(index)
    if index < 0 or index >= #_GRAPHICS_PRESETS then
        self:_syncGraphicsPresetSelection()
        return
    end
    local preset = _GRAPHICS_PRESETS[index + 1]
    self._applyingGraphicsPreset = true
    System.setMaximumRenderScale(preset[1])
    System.setFrameRate(preset[2])
    System.setAntiAliasingLevel(preset[3])
    System.setLightingRenderScale(preset[4])
    local maximumRenderScaleValues, effectiveMaximumRenderScale = MainConfig.GetMaximumRenderScaleOptions(preset[1])
    self:_setMaximumRenderScaleOptions(maximumRenderScaleValues, effectiveMaximumRenderScale)
    self._framerateRow:getDropBox():setSelectedIndex(self.model.FindSelectedIndex(_FRAMERATE_ITEMS, preset[2]))
    self
        ._antiAliasingLevelRow
        :getDropBox()
        :setSelectedIndex(self.model.FindSelectedIndex(self._antiAliasingLevelItems, preset[3]))
    self._lightingRenderScaleRow:getDropBox():setSelectedIndex(findScaleIndex(_LIGHTING_RENDER_SCALE_VALUES, preset[4]))
    self._applyingGraphicsPreset = false
    self:_syncGraphicsPresetSelection()
end

---@diagnostic disable-next-line: unused
function ConfigWindowUI:_getCurrentDisplayScaleOptions()
    local configuredScale = System.getConfiguredScale()
    local maximumScale = System.getMaximumWindowedScale(System.getGameSize())
    local scaleValues, effectiveScale = MainConfig.GetDisplayScaleOptions(maximumScale, configuredScale)
    if maximumScale ~= nil and effectiveScale ~= configuredScale then
        System.setScale(effectiveScale)
    end
    return scaleValues, effectiveScale
end

function ConfigWindowUI:_setScaleOptions(scaleValues, selectedScale)
    self._scaleValues = scaleValues
    ---@cast self._scaleRow - nil
    self._scaleRow:setItems(getScaleLabels(scaleValues))
    self._scaleRow:getDropBox():setSelectedIndex(findScaleIndex(scaleValues, selectedScale))
end

function ConfigWindowUI:_setMaximumRenderScaleOptions(scaleValues, selectedScale)
    self._maximumRenderScaleValues = scaleValues
    self._maximumRenderScaleRow:setItems(getMaximumRenderScaleLabels(scaleValues))
    self._maximumRenderScaleRow:getDropBox():setSelectedIndex(findScaleIndex(scaleValues, selectedScale))
end

function ConfigWindowUI:_onScaleSelectionConfirmed(index)
    if not System.isDisplayScaleConfigurable() then
        local scaleValues, effectiveScale = self:_getCurrentDisplayScaleOptions()
        self:_setScaleOptions(scaleValues, effectiveScale)
        return
    end
    local selectedScale = assert(self._scaleValues[index + 1])
    local maximumScale = System.getMaximumWindowedScale(System.getGameSize())
    local scaleValues, effectiveScale = MainConfig.GetDisplayScaleOptions(maximumScale, selectedScale)
    self:_setScaleOptions(scaleValues, effectiveScale)
    System.setScale(effectiveScale)
end

function ConfigWindowUI:_onMaximumRenderScaleSelectionConfirmed(index)
    local selectedScale = assert(self._maximumRenderScaleValues[index + 1])
    local scaleValues, effectiveScale = MainConfig.GetMaximumRenderScaleOptions(selectedScale)
    self:_setMaximumRenderScaleOptions(scaleValues, effectiveScale)
    System.setMaximumRenderScale(effectiveScale)
    self:_syncGraphicsPresetSelection()
end

function ConfigWindowUI:setPageRowsActive(index, active)
    setRowsActive(self:getPage(index).rows, active)
end

return Ui.Define("ConfigWindow", ConfigWindowUI)
