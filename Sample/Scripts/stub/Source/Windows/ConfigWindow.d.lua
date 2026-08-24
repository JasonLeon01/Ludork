---@meta Source.Windows.ConfigWindow

---@class Source.Windows.ConfigWindow.PageSession
---@field index         integer
---@field scrollOriginY number

--- Each setting row combines a label and an interactive control.
---@class Source.Windows.ConfigWindow: Source.Windows.Base.WindowSelectable
---@field _activePageIndex          integer
---@field _focusLevel               "tabs" | "settings"
---@field _pageSessions             Source.Windows.ConfigWindow.PageSession[]
---@field _ui                       Source.UI.ConfigWindow
---@field _windowContent            Engine.Canvas
---@field _settingsContent          Engine.Canvas
---@field _tabList                  Engine.ListView
---@field _languageRow              Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _graphicsPresetRow        Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _maximumRenderScaleRow    Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _framerateRow             Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _antiAliasingLevelRow     Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _lightingRenderScaleRow   Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@field _verticalSyncRow          Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
---@field _musicOnRow               Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
---@field _musicVolumeRow           Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI
---@field _soundOnRow               Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
---@field _soundVolumeRow           Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI
---@field _voiceOnRow               Source.UI.Parts.ConfigWindow.ConfigCheckBoxRow.ConfigCheckBoxRowUI
---@field _voiceVolumeRow           Source.UI.Parts.ConfigWindow.ConfigSliderRow.ConfigSliderRowUI
---@field _onClose                  function | nil
---@field _open                     boolean
---@field _capturedTouchSlider      Engine.Slider | nil
---@field _capturedTouchSliderIndex integer | nil
---@field _capturedTouchOwner       "list" | "slider" | nil
local ConfigWindow = {}

---@param onClose function | nil
---@return Source.Windows.ConfigWindow
function ConfigWindow.new(onClose) end

---@param row Source.UI.Parts.ConfigWindow.ConfigSettingRow.ConfigSettingRowUI
---@return function
function ConfigWindow.MakeSettingRowConfirmCallback(row) end

---@param items table
---@param value string | number
---@return integer
function ConfigWindow.FindSelectedIndex(items, value) end

---@brief Construct the configuration window.
---
--- - @param onClose Optional callback when the window is closed
---@param onClose function | nil
function ConfigWindow:init(onClose) end

---@brief Get the language DropBox.
---
--- - @return Language DropBox coordinator
---@return Engine.DropBox
function ConfigWindow:getLanguageDropBox() end

---@brief Get the graphics-quality preset DropBox.
---
--- - @return Graphics-quality preset DropBox coordinator
---@return Engine.DropBox
function ConfigWindow:getGraphicsPresetDropBox() end

---@brief Get the scale DropBox on the scale settings row.
---
--- Displays without configurable scaling do not create this row.
---
--- - @return Scale DropBox coordinator, or nil when display scaling is unavailable
---@return Engine.DropBox | nil
function ConfigWindow:getScaleDropBox() end

---@brief Get the maximum render scale DropBox on the settings row.
---
--- - @return Maximum render scale DropBox coordinator
---@return Engine.DropBox
function ConfigWindow:getMaximumRenderScaleDropBox() end

---@brief Get the framerate DropBox on the framerate settings row.
---
--- - @return Framerate DropBox coordinator
---@return Engine.DropBox
function ConfigWindow:getFramerateDropBox() end

---@brief Get the anti-aliasing level DropBox on the settings list.
---
--- - @return Anti-aliasing level DropBox coordinator
---@return Engine.DropBox
function ConfigWindow:getAntiAliasingLevelDropBox() end

---@brief Get the lighting resolution DropBox on the settings list.
---
--- - @return Lighting resolution DropBox coordinator
---@return Engine.DropBox
function ConfigWindow:getLightingRenderScaleDropBox() end

---@brief Get the vertical-sync CheckBox on the settings list.
---
--- - @return Vertical-sync CheckBox coordinator
---@return Engine.CheckBox
function ConfigWindow:getVerticalSyncCheckBox() end

---@brief Get the music-enabled CheckBox on the settings list.
---
--- - @return Music-enabled CheckBox coordinator
---@return Engine.CheckBox
function ConfigWindow:getMusicOnCheckBox() end

---@brief Get the music-volume Slider on the settings list.
---
--- - @return Music-volume Slider coordinator
---@return Engine.Slider
function ConfigWindow:getMusicVolumeSlider() end

---@brief Get the sound-enabled CheckBox on the settings list.
---
--- - @return Sound-enabled CheckBox coordinator
---@return Engine.CheckBox
function ConfigWindow:getSoundOnCheckBox() end

---@brief Get the sound-volume Slider on the settings list.
---
--- - @return Sound-volume Slider coordinator
---@return Engine.Slider
function ConfigWindow:getSoundVolumeSlider() end

---@brief Get the voice-enabled CheckBox on the settings list.
---
--- - @return Voice-enabled CheckBox coordinator
---@return Engine.CheckBox
function ConfigWindow:getVoiceOnCheckBox() end

---@brief Get the voice-volume Slider on the settings list.
---
--- - @return Voice-volume Slider coordinator
---@return Engine.Slider
function ConfigWindow:getVoiceVolumeSlider() end

---@brief Check whether this window is currently open.
---
--- - @return True if open, False otherwise
---@return boolean
function ConfigWindow:isOpen() end

---@brief Show and activate the configuration window.
function ConfigWindow:open() end

---@brief Hide and deactivate the configuration window.
function ConfigWindow:close() end

function ConfigWindow:dispose() end

---@brief Collapse an expanded DropBox, leave settings, or close the window.
function ConfigWindow:onReturn() end

---@brief Update the active configuration page and selection input.
---
--- - @param deltaTime Elapsed time in seconds
---@param deltaTime number
function ConfigWindow:onTick(deltaTime) end

---@param scaleRowChange integer
function ConfigWindow:_applyScaleRowChange(scaleRowChange) end

---@brief Handle configuration input, giving an expanded DropBox priority.
---
--- - @param kwargs Event arguments
---@param kwargs table
function ConfigWindow:onKeyDown(kwargs) end

---@brief Apply the state-aware cancel action on right-click.
---@param direction string
---@return boolean
function ConfigWindow:onDirectionalKey(direction) end

---@param tabIndex integer
function ConfigWindow:_onTabConfirmed(tabIndex) end

---@param expanded boolean
function ConfigWindow:_onDropBoxExpandedChanged(expanded) end

---@return number
function ConfigWindow:_getMaxScrollOriginY() end

---@param position sf.Vector2f
function ConfigWindow:_onCapturedTouchBegan(position) end

---@param position sf.Vector2f
---@return boolean
function ConfigWindow:_handleCapturedTouchDrag(position) end

---@param position sf.Vector2f
---@return boolean
function ConfigWindow:_handleCapturedTouchTap(position) end

function ConfigWindow:_onCapturedTouchReset() end

return ConfigWindow
