---@meta

---@class Source.Windows.ConfigWindow.Page
---@field list        Engine.ListView
---@field allRows     Source.Windows.ConfigWindow.ConfigRow[]
---@field rows        Source.Windows.ConfigWindow.ConfigRow[]
---@field dropBoxRows Source.Windows.ConfigWindow.ConfigSettingRow.Controller[]
---@field localeKeys  string[]

---@class Source.Windows.ConfigWindow.PageSession
---@field index        integer
---@field scrollOffset sf.Vector2f

--- Each setting row combines a label and an interactive control.
---@class Source.Windows.ConfigWindow.Controller: Internal.UIBase.UiController
---@field host                           Source.Windows.ConfigWindow
---@field _activePageIndex               integer
---@field _pageSessions                  Source.Windows.ConfigWindow.PageSession[]
---@field _languageRow                   Source.Windows.ConfigWindow.ConfigSettingRow.Controller
---@field _graphicsPresetRow             Source.Windows.ConfigWindow.ConfigSettingRow.Controller
---@field _maximumRenderScaleRow         Source.Windows.ConfigWindow.ConfigSettingRow.Controller
---@field _framerateRow                  Source.Windows.ConfigWindow.ConfigSettingRow.Controller
---@field _antiAliasingLevelRow          Source.Windows.ConfigWindow.ConfigSettingRow.Controller
---@field _lightingRenderScaleRow        Source.Windows.ConfigWindow.ConfigSettingRow.Controller
---@field _verticalSyncRow               Source.Windows.ConfigWindow.ConfigCheckBoxRow.Controller
---@field _musicOnRow                    Source.Windows.ConfigWindow.ConfigCheckBoxRow.Controller
---@field _musicVolumeRow                Source.Windows.ConfigWindow.ConfigSliderRow.Controller
---@field _soundOnRow                    Source.Windows.ConfigWindow.ConfigCheckBoxRow.Controller
---@field _soundVolumeRow                Source.Windows.ConfigWindow.ConfigSliderRow.Controller
---@field _voiceOnRow                    Source.Windows.ConfigWindow.ConfigCheckBoxRow.Controller
---@field _voiceVolumeRow                Source.Windows.ConfigWindow.ConfigSliderRow.Controller
---@field _onClose                       function | nil
---@field _open                          boolean
---@field _tabNavigationHandledThisFrame boolean
---@field ui                             Internal.UI.ConfigWindow
---@field _scaleAvailable                boolean
---@field _scaleValues                   number[]
---@field _maximumRenderScaleValues      number[]
---@field _pages                         Source.Windows.ConfigWindow.Page[]
---@field _applyingGraphicsPreset        boolean
---@field _scaleRow                      Source.Windows.ConfigWindow.ConfigSettingRow.Controller
---@field _antiAliasingLevelItems        string[]
local Controller = {}

---@brief Construct the configuration window.
---
--- - @param onClose Optional callback when the window is closed
---@param onClose function | nil
function Controller:init(onClose) end

---@brief Get the language DropBox.
---
--- - @return Language DropBox coordinator
---@return Engine.DropBox
function Controller:getLanguageDropBox() end

---@brief Get the graphics-quality preset DropBox.
---
--- - @return Graphics-quality preset DropBox coordinator
---@return Engine.DropBox
function Controller:getGraphicsPresetDropBox() end

---@brief Get the scale DropBox on the scale settings row.
---
--- Displays without configurable scaling hide this generated row.
---
--- - @return Scale DropBox coordinator, or nil when display scaling is unavailable
---@return Engine.DropBox | nil
function Controller:getScaleDropBox() end

---@brief Get the maximum render scale DropBox on the settings row.
---
--- - @return Maximum render scale DropBox coordinator
---@return Engine.DropBox
function Controller:getMaximumRenderScaleDropBox() end

---@brief Get the framerate DropBox on the framerate settings row.
---
--- - @return Framerate DropBox coordinator
---@return Engine.DropBox
function Controller:getFramerateDropBox() end

---@brief Get the anti-aliasing level DropBox on the settings list.
---
--- - @return Anti-aliasing level DropBox coordinator
---@return Engine.DropBox
function Controller:getAntiAliasingLevelDropBox() end

---@brief Get the lighting resolution DropBox on the settings list.
---
--- - @return Lighting resolution DropBox coordinator
---@return Engine.DropBox
function Controller:getLightingRenderScaleDropBox() end

---@brief Get the vertical-sync CheckBox on the settings list.
---
--- - @return Vertical-sync CheckBox coordinator
---@return Engine.CheckBox
function Controller:getVerticalSyncCheckBox() end

---@brief Get the music-enabled CheckBox on the settings list.
---
--- - @return Music-enabled CheckBox coordinator
---@return Engine.CheckBox
function Controller:getMusicOnCheckBox() end

---@brief Get the music-volume Slider on the settings list.
---
--- - @return Music-volume Slider coordinator
---@return Engine.Slider
function Controller:getMusicVolumeSlider() end

---@brief Get the sound-enabled CheckBox on the settings list.
---
--- - @return Sound-enabled CheckBox coordinator
---@return Engine.CheckBox
function Controller:getSoundOnCheckBox() end

---@brief Get the sound-volume Slider on the settings list.
---
--- - @return Sound-volume Slider coordinator
---@return Engine.Slider
function Controller:getSoundVolumeSlider() end

---@brief Get the voice-enabled CheckBox on the settings list.
---
--- - @return Voice-enabled CheckBox coordinator
---@return Engine.CheckBox
function Controller:getVoiceOnCheckBox() end

---@brief Get the voice-volume Slider on the settings list.
---
--- - @return Voice-volume Slider coordinator
---@return Engine.Slider
function Controller:getVoiceVolumeSlider() end

---@param position sf.Vector2f
---@return Engine.Slider | nil, integer | nil
function Controller:_getSliderAt(position) end

---@brief Check whether this window is currently open.
---
--- - @return True if open, False otherwise
---@return boolean
function Controller:isOpen() end

---@brief Show the configuration window at the Graphics tab and reset every page cursor and scroll position.
function Controller:open() end

---@brief Hide and deactivate the configuration window.
function Controller:close() end

function Controller:dispose() end

---@brief Collapse an expanded DropBox or close the window.
function Controller:onReturn() end

---@param deltaTime number
function Controller:update(deltaTime) end

---@brief Update the active configuration page and selection input.
---
--- - @param deltaTime Elapsed time in seconds
---@param deltaTime number
function Controller:onTick(deltaTime) end

---@param scaleRowChange integer
function Controller:_applyScaleRowChange(scaleRowChange) end

---@brief Handle configuration input, giving an expanded DropBox priority.
---
--- - @param kwargs Event arguments
---@param kwargs Engine.UiInputEventArguments
function Controller:onKeyDown(kwargs) end

---@brief Move the selection within the active settings page.
---@param direction string
---@return boolean
function Controller:onDirectionalKey(direction) end

---@return boolean
function Controller:handleTabNavigation() end

---@param tabIndex integer
function Controller:selectTab(tabIndex) end

---@param expanded boolean
function Controller:onDropBoxExpandedChanged(expanded) end

function Controller:bind() end

function Controller:refresh() end

---@return integer
function Controller:refreshDisplayScaleOptions() end

---@return integer
function Controller:syncDisplayScaleAvailability() end

---@return integer
function Controller:getPageCount() end

---@param index integer
---@return Source.Windows.ConfigWindow.Page
function Controller:getPage(index) end

---@param index integer
function Controller:setActivePage(index) end

---@param index integer
function Controller:onFrameRateSelectedIndexChanged(index) end

---@param index integer
function Controller:onAntiAliasingLevelSelectedIndexChanged(index) end

---@param index integer
function Controller:onLightingRenderScaleSelectedIndexChanged(index) end

---@param index  integer
---@param active boolean
function Controller:setPageRowsActive(index, active) end

---@param onClose function | nil
---@return Source.Windows.ConfigWindow
function Controller.new(onClose) end

function Controller:ready() end

---@param index integer
---@return sf.FloatRect
function Controller:getSelectionLayoutRect(index) end
