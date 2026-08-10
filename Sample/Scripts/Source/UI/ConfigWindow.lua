local Engine = require("Engine")
local GlobalCore = require("GlobalCore")
local MainConfig = require("Source.Config.Main")
local EventKeys = require("Source.Configs.EventKeys")
local Locale = require("Source.Locale.Core")
local ConfigCheckBoxRowUI = require("Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow")
local ConfigSettingRowUI = require("Source.UI.Parts.ConfigWindow.ConfigSettingRow")
local ConfigSliderRowUI = require("Source.UI.Parts.ConfigWindow.ConfigSliderRow")
local Ui = require("Source.UI.Ui")

local System = GlobalCore.System
local LOC = Locale.ApplyStringLocaleFormat

local _WINDOW_WIDTH = 480
local _WINDOW_HEIGHT = 320
local _CONTENT_WIDTH = 448
local _DROPBOX_WIDTH = 200
local _CHECKBOX_SIZE = 32
local _SLIDER_WIDTH = 160
local _LANGUAGE_VALUES = MainConfig.SupportedLanguages
local _SCALE_ITEMS = { "1", "1.25", "1.5", "2.0" }
local _FRAMERATE_ITEMS = { "30", "60", "90", "120" }
local _SETTING_LOCALE_KEYS = {
    "language", "scale", "framerate", "verticalsync", "musicon", "musicvolume", "soundon", "soundvolume", "voiceon",
    "voicevolume"
}

local function _getLanguageLabels()
    local languageLabels = {}
    for index, value in ipairs(_LANGUAGE_VALUES) do
        languageLabels[index] = LOC(value)
    end
    return languageLabels
end

---@class Source.UI.ConfigWindow
local ConfigWindowUI = {}

ConfigWindowUI.refreshEvents = { EventKeys.LocaleChanged }

function ConfigWindowUI:init(model, windowSkin)
    self._windowSkin = windowSkin
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
    self._languageRow:setItems(_getLanguageLabels())
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

function ConfigWindowUI.onScaleSelectedIndexChanged(index)
    local scale = tonumber(_SCALE_ITEMS[index + 1])
    ---@cast scale number
    System.saveScale(scale)
    ConfigWindowUI._showRestartMindTip()
end

function ConfigWindowUI.onFrameRateSelectedIndexChanged(index)
    System.setFrameRate(Engine.ToInteger(_FRAMERATE_ITEMS[index + 1]))
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

function ConfigWindowUI.showRestartMindTip()
    ConfigWindowUI._showRestartMindTip()
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
        LOC("language"), _getLanguageLabels(), _CONTENT_WIDTH, _DROPBOX_WIDTH, self._windowSkin,
        self.model._findSelectedIndex(_LANGUAGE_VALUES, System.getLanguage())
    )
    self._scaleRow = ConfigSettingRowUI.new(
        LOC("scale"), _SCALE_ITEMS, _CONTENT_WIDTH, _DROPBOX_WIDTH, self._windowSkin,
        self.model._findSelectedIndex(_SCALE_ITEMS, System.getConfiguredScale())
    )
    self._framerateRow = ConfigSettingRowUI.new(
        LOC("framerate"), _FRAMERATE_ITEMS, _CONTENT_WIDTH, _DROPBOX_WIDTH, self._windowSkin,
        self.model._findSelectedIndex(_FRAMERATE_ITEMS, System.getFrameRate())
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
    self._dropBoxRows = { self._languageRow, self._scaleRow, self._framerateRow }
    self._settingRows = {
        self._languageRow, self._scaleRow, self._framerateRow, self._verticalSyncRow, self._musicOnRow,
        self._musicVolumeRow, self._soundOnRow, self._soundVolumeRow, self._voiceOnRow, self._voiceVolumeRow
    }
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
    self._scaleRow:getDropBox():setOnSelectedIndexChanged(function (index)
        ConfigWindowUI.onScaleSelectedIndexChanged(index)
    end)
    self._framerateRow:getDropBox():setOnSelectedIndexChanged(function (index)
        ConfigWindowUI.onFrameRateSelectedIndexChanged(index)
    end)
end

function ConfigWindowUI._showRestartMindTip()
    local scene = System.getScene()
    if scene ~= nil then
        scene:addCommonTip(LOC("CONFIG_MIND_RESTART"))
    end
end

return Ui.define("ConfigWindow", ConfigWindowUI)
