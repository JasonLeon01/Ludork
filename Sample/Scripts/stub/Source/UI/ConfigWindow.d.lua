---@meta Source.UI.ConfigWindow

---@class Source.UI.ConfigWindow.Page
---@field list        Engine.ListView
---@field rows        Source.UI.Parts.ConfigWindow.ConfigRow.ConfigRowControllerBase[]
---@field dropBoxRows Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI[]
---@field localeKeys  string[]

---@class Source.UI.ConfigWindow: Source.UI.UiController
---@field model                     Source.Windows.ConfigWindow
---@field _windowSkin               sf.Image
---@field _scaleValues              number[]
---@field _maximumRenderScaleValues number[]
---@field _pages                    Source.UI.ConfigWindow.Page[]
---@field _tabControllers           Source.UI.Helpers.CommandRow[]
---@field _activePageIndex          integer
---@field _applyingGraphicsPreset   boolean
---@field _windowFrame              Engine.Window
---@field _content                  Engine.Canvas
---@field _settingsWindowFrame      Engine.Window
---@field _tabList                  Engine.ListView
---@field _settingsContent          Engine.Canvas
---@field _graphicsList             Engine.ListView
---@field _audioList                Engine.ListView
---@field _languageList             Engine.ListView
---@field _languageRow              Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _graphicsPresetRow        Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _scaleRow                 Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI | nil
---@field _maximumRenderScaleRow    Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _framerateRow             Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _antiAliasingLevelItems   string[]
---@field _antiAliasingLevelRow     Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _lightingRenderScaleRow   Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _verticalSyncRow          Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
---@field _musicOnRow               Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
---@field _musicVolumeRow           Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI
---@field _soundOnRow               Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
---@field _soundVolumeRow           Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI
---@field _voiceOnRow               Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
---@field _voiceVolumeRow           Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI
local ConfigWindowUI = {}

---@return Source.UI.ConfigWindow
function ConfigWindowUI.new(...) end

---@param model      Source.Windows.ConfigWindow
---@param windowSkin sf.Image
function ConfigWindowUI:init(model, windowSkin) end

function ConfigWindowUI:bind() end

function ConfigWindowUI:refresh() end

---@return integer
function ConfigWindowUI:refreshDisplayScaleOptions() end

---@return integer
function ConfigWindowUI:syncDisplayScaleAvailability() end

---@return Engine.Canvas
function ConfigWindowUI:prepare() end

function ConfigWindowUI:attach() end

---@return Engine.Window
function ConfigWindowUI:getWindowFrame() end

---@return Engine.Canvas
function ConfigWindowUI:getContent() end

---@return Engine.ListView
function ConfigWindowUI:getTabList() end

---@return Engine.Canvas
function ConfigWindowUI:getSettingsContent() end

---@return integer
function ConfigWindowUI:getPageCount() end

---@param index integer
---@return Source.UI.ConfigWindow.Page
function ConfigWindowUI:getPage(index) end

---@return integer
function ConfigWindowUI:getActivePageIndex() end

---@param index integer
function ConfigWindowUI:setActivePage(index) end

---@return Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
function ConfigWindowUI:getLanguageRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
function ConfigWindowUI:getGraphicsPresetRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI | nil
function ConfigWindowUI:getScaleRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
function ConfigWindowUI:getMaximumRenderScaleRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
function ConfigWindowUI:getFramerateRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
function ConfigWindowUI:getAntiAliasingLevelRow() end

---@return Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
function ConfigWindowUI:getLightingRenderScaleRow() end

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

---@param index integer
function ConfigWindowUI:onFrameRateSelectedIndexChanged(index) end

---@param index integer
function ConfigWindowUI:onAntiAliasingLevelSelectedIndexChanged(index) end

---@param index integer
function ConfigWindowUI:onLightingRenderScaleSelectedIndexChanged(index) end

---@param deltaTime number
function ConfigWindowUI:tick(deltaTime) end

function ConfigWindowUI:dispose() end

---@param index  integer
---@param active boolean
function ConfigWindowUI:setPageRowsActive(index, active) end

return ConfigWindowUI
