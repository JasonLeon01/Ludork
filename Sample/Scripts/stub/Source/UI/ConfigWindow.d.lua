---@meta Source.UI.ConfigWindow

---@class Source.UI.ConfigWindow : Source.UI.UiController
---@field model Source.Windows.ConfigWindow
---@field _windowSkin sf.Image
---@field _scaleValues number[]
---@field _windowFrame Engine.Window
---@field _content Engine.Canvas
---@field _listView Engine.ListView
---@field _languageRow Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _scaleRow Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI | nil
---@field _framerateRow Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _antiAliasingLevelItems string[]
---@field _antiAliasingLevelRow Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _verticalSyncRow Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
---@field _musicOnRow Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
---@field _musicVolumeRow Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI
---@field _soundOnRow Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
---@field _soundVolumeRow Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI
---@field _voiceOnRow Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
---@field _voiceVolumeRow Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI
---@field _dropBoxRows Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI[]
---@field _settingRows Source.UI.Parts.ConfigWindow.ConfigRow.ConfigRowControllerBase[]
local ConfigWindowUI = {}

---@return Source.UI.ConfigWindow
function ConfigWindowUI.new(...) end

---@param model      Source.Windows.ConfigWindow
---@param windowSkin sf.Image
function ConfigWindowUI:init(model, windowSkin) end

function ConfigWindowUI:bind() end

function ConfigWindowUI:refresh() end

function ConfigWindowUI:refreshDisplayScaleOptions() end

---@return Engine.Canvas
function ConfigWindowUI:prepare() end

function ConfigWindowUI:attach() end

---@return Engine.Window
function ConfigWindowUI:getWindowFrame() end

---@return Engine.Canvas
function ConfigWindowUI:getContent() end

---@return Engine.ListView
function ConfigWindowUI:getListView() end

---@return Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
function ConfigWindowUI:getLanguageRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI | nil
function ConfigWindowUI:getScaleRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
function ConfigWindowUI:getFramerateRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
function ConfigWindowUI:getAntiAliasingLevelRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
function ConfigWindowUI:getVerticalSyncRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
function ConfigWindowUI:getMusicOnRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI
function ConfigWindowUI:getMusicVolumeRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
function ConfigWindowUI:getSoundOnRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI
function ConfigWindowUI:getSoundVolumeRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
function ConfigWindowUI:getVoiceOnRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI
function ConfigWindowUI:getVoiceVolumeRow() end

---@return table
function ConfigWindowUI:getDropBoxRows() end

---@return table
function ConfigWindowUI:getSettingRows() end

---@param checked boolean
function ConfigWindowUI.onVerticalSyncCheckedChanged(checked) end

---@param index integer
function ConfigWindowUI.onLanguageSelectedIndexChanged(index) end

---@param index integer
function ConfigWindowUI.onFrameRateSelectedIndexChanged(index) end

---@param index integer
function ConfigWindowUI:onAntiAliasingLevelSelectedIndexChanged(index) end

---@param checked boolean
function ConfigWindowUI.onMusicOnCheckedChanged(checked) end

---@param value integer
function ConfigWindowUI.onMusicVolumeChanged(value) end

---@param checked boolean
function ConfigWindowUI.onSoundOnCheckedChanged(checked) end

---@param value integer
function ConfigWindowUI.onSoundVolumeChanged(value) end

---@param checked boolean
function ConfigWindowUI.onVoiceOnCheckedChanged(checked) end

---@param value integer
function ConfigWindowUI.onVoiceVolumeChanged(value) end

---@param deltaTime number
function ConfigWindowUI:tick(deltaTime) end

function ConfigWindowUI:dispose() end

return ConfigWindowUI
