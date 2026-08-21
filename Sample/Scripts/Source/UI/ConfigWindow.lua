local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local MainConfig = require("Source.Configs.Main")
local EventKeys = require("Source.Configs.EventKeys")
local Locale = require("Source.Locale.Core")
local ConfigCheckBoxRowUI = require("Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow")
local ConfigSettingRowUI = require("Source.UI.Parts.ConfigWindow.ConfigSettingRow")
local ConfigSliderRowUI = require("Source.UI.Parts.ConfigWindow.ConfigSliderRow")
local Ui = require("Source.UI.Ui")

local System = GlobalCore.System
---@type fun(value: string): string
local LOC = Locale.ApplyStringLocaleFormat

local _WINDOW_WIDTH = 480
local _WINDOW_HEIGHT = 320
local _CONTENT_WIDTH = 448
local _DROPBOX_WIDTH = 200
local _CHECKBOX_SIZE = 32
local _SLIDER_WIDTH = 160
local _LANGUAGE_VALUES = MainConfig.SupportedLanguages
local _FRAMERATE_ITEMS = { "30", "60", "90", "120" }
local _ANTI_ALIASING_LEVEL_ITEMS = { "0", "2", "4", "8" }
local _SETTING_LOCALE_KEYS = {
    "language", "framerate", "antialiasinglevel", "verticalsync", "musicon", "musicvolume", "soundon",
    "soundvolume", "voiceon", "voicevolume"
}
if not LUDORK_MOBILE then
    table.insert(_SETTING_LOCALE_KEYS, 2, "scale")
end

local function getLanguageLabels()
    local languageLabels = {}
    for index, value in ipairs(_LANGUAGE_VALUES) do
        languageLabels[index] = LOC(value)
    end
    return languageLabels
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

---@class Source.UI.ConfigWindow
local ConfigWindowUI = {}

ConfigWindowUI.refreshEvents = { EventKeys.LocaleChanged }

function ConfigWindowUI:init(model, windowSkin)
    self._windowSkin = windowSkin
    self._scaleValues = {}
    super(ConfigWindowUI, self).init(model, nil)
end

function ConfigWindowUI:bind()
    self._windowFrame = self:requireControl("WindowFrame")
    self._content = self:requireControl("Content")
    self._listView = self:requireControl("SettingsList")
    self:_createRows()
end

function ConfigWindowUI:refresh()
    for index, rowUI in ipairs(self._settingRows) do
        rowUI:setLabelText(LOC(assert(_SETTING_LOCALE_KEYS[index])))
    end
    self._languageRow:setItems(getLanguageLabels())
    if self._scaleRow ~= nil then
        self._scaleRow:setItems(getScaleLabels(self._scaleValues))
    end
end

function ConfigWindowUI:refreshDisplayScaleOptions()
    if self._scaleRow == nil then
        return
    end
    local scaleValues, effectiveScale = self:_getCurrentDisplayScaleOptions()
    self:_setScaleOptions(scaleValues, effectiveScale)
end

function ConfigWindowUI:prepare()
    return super(ConfigWindowUI, self).prepare(sf.Vector2u.new(_WINDOW_WIDTH, _WINDOW_HEIGHT))
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

function ConfigWindowUI:getListView()
    return self._listView
end

function ConfigWindowUI:getLanguageRow()
    return self._languageRow
end

function ConfigWindowUI:getScaleRow()
    return self._scaleRow
end

function ConfigWindowUI:getFramerateRow()
    return self._framerateRow
end

function ConfigWindowUI:getAntiAliasingLevelRow()
    return self._antiAliasingLevelRow
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

function ConfigWindowUI:getDropBoxRows()
    return self._dropBoxRows
end

function ConfigWindowUI:getSettingRows()
    return self._settingRows
end

function ConfigWindowUI.onVerticalSyncCheckedChanged(checked)
    System.setVerticalSync(checked)
end

function ConfigWindowUI.onLanguageSelectedIndexChanged(index)
    local language = _LANGUAGE_VALUES[index + 1]
    ---@cast language string
    System.setLanguage(language)
    Locale.setLanguage(language)
end

function ConfigWindowUI.onFrameRateSelectedIndexChanged(index)
    System.setFrameRate(Engine.ToInteger(_FRAMERATE_ITEMS[index + 1]))
end

function ConfigWindowUI:onAntiAliasingLevelSelectedIndexChanged(index)
    local value = assert(self._antiAliasingLevelItems[index + 1])
    System.setAntiAliasingLevel(Engine.ToInteger(value))
end

function ConfigWindowUI.onMusicOnCheckedChanged(checked)
    System.setMusicOn(checked)
end

function ConfigWindowUI.onMusicVolumeChanged(value)
    System.setMusicVolume(value)
end

function ConfigWindowUI.onSoundOnCheckedChanged(checked)
    System.setSoundOn(checked)
end

function ConfigWindowUI.onSoundVolumeChanged(value)
    System.setSoundVolume(value)
end

function ConfigWindowUI.onVoiceOnCheckedChanged(checked)
    System.setVoiceOn(checked)
end

function ConfigWindowUI.onVoiceVolumeChanged(value)
    System.setVoiceVolume(value)
end

function ConfigWindowUI:tick(deltaTime)
    for _, rowUI in ipairs(self._settingRows) do
        rowUI:onTick(deltaTime)
    end
end

function ConfigWindowUI:dispose()
    if self._settingRows ~= nil then
        for _, rowUI in ipairs(self._settingRows) do
            rowUI:dispose()
        end
    end
    self._dropBoxRows = {}
    self._settingRows = {}
    super(ConfigWindowUI, self).dispose()
end

function ConfigWindowUI:_createRows()
    self._languageRow = ConfigSettingRowUI.new(
        LOC("language"), getLanguageLabels(), _CONTENT_WIDTH, _DROPBOX_WIDTH, self._windowSkin,
        self.model._findSelectedIndex(_LANGUAGE_VALUES, System.getLanguage())
    )
    if not LUDORK_MOBILE then
        local effectiveScale
        self._scaleValues, effectiveScale = self:_getCurrentDisplayScaleOptions()
        local scaleIndex = findScaleIndex(self._scaleValues, effectiveScale)
        self._scaleRow = ConfigSettingRowUI.new(
            LOC("scale"), getScaleLabels(self._scaleValues), _CONTENT_WIDTH, _DROPBOX_WIDTH, self._windowSkin,
            scaleIndex
        )
    end
    self._framerateRow = ConfigSettingRowUI.new(
        LOC("framerate"), _FRAMERATE_ITEMS, _CONTENT_WIDTH, _DROPBOX_WIDTH, self._windowSkin,
        self.model._findSelectedIndex(_FRAMERATE_ITEMS, System.getFrameRate())
    )
    self._antiAliasingLevelItems = getAntiAliasingLevelItems(System.getAntiAliasingLevel())
    self._antiAliasingLevelRow = ConfigSettingRowUI.new(
        LOC("antialiasinglevel"), self._antiAliasingLevelItems, _CONTENT_WIDTH, _DROPBOX_WIDTH,
        self._windowSkin,
        self.model._findSelectedIndex(self._antiAliasingLevelItems, System.getAntiAliasingLevel())
    )
    self._verticalSyncRow = ConfigCheckBoxRowUI.new(
        LOC("verticalsync"),
        _CONTENT_WIDTH,
        _CHECKBOX_SIZE,
        self._windowSkin,
        System.getVerticalSync(),
        function (checked)
            ConfigWindowUI.onVerticalSyncCheckedChanged(checked)
        end
    )
    self._musicOnRow = ConfigCheckBoxRowUI.new(
        LOC("musicon"),
        _CONTENT_WIDTH,
        _CHECKBOX_SIZE,
        self._windowSkin,
        System.getMusicOn(),
        function (checked)
            ConfigWindowUI.onMusicOnCheckedChanged(checked)
        end
    )
    self._musicVolumeRow = ConfigSliderRowUI.new(
        LOC("musicvolume"),
        _CONTENT_WIDTH,
        _SLIDER_WIDTH,
        Engine.Round(System.getMusicVolume()),
        function (value)
            ConfigWindowUI.onMusicVolumeChanged(value)
        end
    )
    self._soundOnRow = ConfigCheckBoxRowUI.new(
        LOC("soundon"),
        _CONTENT_WIDTH,
        _CHECKBOX_SIZE,
        self._windowSkin,
        System.getSoundOn(),
        function (checked)
            ConfigWindowUI.onSoundOnCheckedChanged(checked)
        end
    )
    self._soundVolumeRow = ConfigSliderRowUI.new(
        LOC("soundvolume"),
        _CONTENT_WIDTH,
        _SLIDER_WIDTH,
        Engine.Round(System.getSoundVolume()),
        function (value)
            ConfigWindowUI.onSoundVolumeChanged(value)
        end
    )
    self._voiceOnRow = ConfigCheckBoxRowUI.new(
        LOC("voiceon"),
        _CONTENT_WIDTH,
        _CHECKBOX_SIZE,
        self._windowSkin,
        System.getVoiceOn(),
        function (checked)
            ConfigWindowUI.onVoiceOnCheckedChanged(checked)
        end
    )
    self._voiceVolumeRow = ConfigSliderRowUI.new(
        LOC("voicevolume"),
        _CONTENT_WIDTH,
        _SLIDER_WIDTH,
        Engine.Round(System.getVoiceVolume()),
        function (value)
            ConfigWindowUI.onVoiceVolumeChanged(value)
        end
    )
    self._dropBoxRows = { self._languageRow, self._framerateRow, self._antiAliasingLevelRow }
    self._settingRows = {
        self._languageRow, self._framerateRow, self._antiAliasingLevelRow, self._verticalSyncRow, self._musicOnRow,
        self._musicVolumeRow, self._soundOnRow, self._soundVolumeRow, self._voiceOnRow, self._voiceVolumeRow
    }
    if self._scaleRow ~= nil then
        table.insert(self._dropBoxRows, 2, self._scaleRow)
        table.insert(self._settingRows, 2, self._scaleRow)
    end
    for _, rowUI in ipairs(self._settingRows) do
        self._listView:addChild(rowUI:prepare())
    end
    for _, rowUI in ipairs(self._dropBoxRows) do
        rowUI:addConfirmCallback(self.model._makeSettingRowConfirmCallback(rowUI))
        rowUI:getDropBox():setOnExpandedChanged(function (expanded)
            self.model:_onDropBoxExpandedChanged(expanded)
        end)
    end
    self._languageRow:getDropBox():setOnSelectedIndexChanged(function (index)
        ConfigWindowUI.onLanguageSelectedIndexChanged(index)
    end)
    if self._scaleRow ~= nil then
        self._scaleRow:getDropBox():setOnSelectionConfirmed(function (index)
            self:_onScaleSelectionConfirmed(index)
        end)
    end
    self._framerateRow:getDropBox():setOnSelectedIndexChanged(function (index)
        ConfigWindowUI.onFrameRateSelectedIndexChanged(index)
    end)
    self._antiAliasingLevelRow:getDropBox():setOnSelectedIndexChanged(function (index)
        self:onAntiAliasingLevelSelectedIndexChanged(index)
    end)
end

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
    ---@cast self._scaleRow -nil
    self._scaleRow:setItems(getScaleLabels(scaleValues))
    local scaleIndex = findScaleIndex(scaleValues, selectedScale)
    self._scaleRow:getDropBox():setSelectedIndex(scaleIndex)
end

function ConfigWindowUI:_onScaleSelectionConfirmed(index)
    local selectedScale = assert(self._scaleValues[index + 1])
    local maximumScale = System.getMaximumWindowedScale(System.getGameSize())
    local scaleValues, effectiveScale = MainConfig.GetDisplayScaleOptions(maximumScale, selectedScale)
    self:_setScaleOptions(scaleValues, effectiveScale)
    System.setScale(effectiveScale)
end

return Ui.define("ConfigWindow", ConfigWindowUI)
